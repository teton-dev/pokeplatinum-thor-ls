#ifndef PORT_NUZLOCKE_MODE_H
#define PORT_NUZLOCKE_MODE_H

// Nuzlocke mode (docs/thor/dev/nuzlocke.md): an enforced Nuzlocke, per save, at one of three
// levels. A save can turn it on, change level and turn it off from Settings > Gameplay; once
// off (by the player or by whiting out) it can never be turned on again on that save.
//
// The state is kept in the save image itself, in flash sectors the DS game doesn't use, so it
// goes along with the save (exports, imports, save sync) and reloading an older save doesn't
// bring a dropped run back. Records are keyed by the save's trainer ID: a new game started
// over an old save doesn't inherit its mode. The core here is game-neutral; each game supplies
// nuzlocke_mode_game.h.
//
// Threads: the getters are for the game thread (and good enough for the GUI to show); the
// Request functions are for the GUI, applied by NuzlockeMode_Update on the game thread.

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum NuzlockeLevel {
    NUZLOCKE_OFF = 0,
    NUZLOCKE_SIMPLE,
    NUZLOCKE_NORMAL,
    NUZLOCKE_HARD,
    NUZLOCKE_LEVEL_COUNT,
};

// What the GUI shows: a snapshot the game thread publishes every frame
typedef struct NuzlockeModeView {
    BOOL hasSave;      // a game is running (the field is up): its save can be set
    u8 level;          // NuzlockeLevel
    BOOL critGuard;
    BOOL dropped;      // was a Nuzlocke and left it: can't be one again
    BOOL storageError; // the record couldn't be written
} NuzlockeModeView;

// Game thread
void NuzlockeMode_Update(void); // once a frame, from the main loop
int NuzlockeMode_Level(void);   // NuzlockeLevel of the running save (OFF with no save)
BOOL NuzlockeMode_On(void);
BOOL NuzlockeMode_CritGuard(void); // never with Hard
BOOL NuzlockeMode_WasDropped(void);
// Whiting out: the run is over (off, and dropped for good)
void NuzlockeMode_Drop(void);
// The level of a save that isn't running, read from the records in the loaded slot's save image (the main menu)
int NuzlockeMode_LevelOfSave(u32 saveId);

// GUI
void NuzlockeMode_GetView(NuzlockeModeView *out);
void NuzlockeMode_RequestLevel(int level); // OFF drops the run; ignored on a dropped save
void NuzlockeMode_RequestCritGuard(BOOL on);

const char *NuzlockeMode_LevelName(int level);

// The catch rule (Normal and Hard, once the save has had Poke Balls), both engines. An area is a location name.
// The first wild encounter in an area decides its species: caught, the area is used up; missed (fled, fainted,
// run from), only that species may be caught there. Shinies never count and can always be caught; static
// (scripted) encounters count only with Hard.
enum NuzlockeAreaState {
    NUZLOCKE_AREA_OPEN = 0,
    NUZLOCKE_AREA_CAUGHT,
    NUZLOCKE_AREA_MISSED,
};
void NuzlockeMode_MarkStatic(void); // the battle about to start is a scripted encounter
void NuzlockeMode_OnWildBattleStart(u16 location, u16 species, BOOL shiny);
BOOL NuzlockeMode_CatchAllowed(u16 species, BOOL shiny); // FALSE: the ball is knocked back
void NuzlockeMode_OnWildBattleEnd(BOOL captured);
BOOL NuzlockeMode_BallsReceived(void);
int NuzlockeMode_AreaState(u16 location, u16 *species); // NuzlockeAreaState
BOOL NuzlockeMode_CatchRuleStarted(void); // Normal or Hard, and the save has had Poke Balls
// What the Gear shows for an area: the catch rule's view of it (Hard: a missed first encounter fails the area)
enum NuzlockeAreaStatus {
    NUZLOCKE_STATUS_NONE = 0, // the rule isn't on (Off, Simple, no Poke Balls yet)
    NUZLOCKE_STATUS_AVAILABLE, // nothing met yet (or the encounter is going on): a catch is available
    NUZLOCKE_STATUS_LIMITED,   // missed (Normal): only *species may be caught
    NUZLOCKE_STATUS_FAILED,    // missed (Hard): nothing can be caught here; *species is what was missed
    NUZLOCKE_STATUS_CAUGHT,    // caught: *species
};
int NuzlockeMode_AreaStatus(u16 location, u16 *species);
#define NUZLOCKE_CATCH_REFUSED_TEXT "You've already used your catch for this area!"

// The dead flag on a Pokemon (set when it faints with the mode on; keeps its HP at 0 for good).
// Implemented per game, on a spare field of the Pokemon data that both games leave unused.
struct Pokemon;
struct BoxPokemon;
BOOL NuzlockeMode_IsDead(struct Pokemon *mon);
BOOL NuzlockeMode_BoxIsDead(struct BoxPokemon *boxMon);
void NuzlockeMode_SetDead(struct Pokemon *mon, BOOL dead);

// The books, as the Gear shows them (src/port/nuzlocke_gear.c hands them to port-common's Nuzlocke API). Areas:
// result 0 in battle, 1 caught, 2 missed. Deaths are logged when the dead flag is set (game thread).
#define NUZLOCKE_MODE_NAME_LEN 12
typedef struct NuzlockeModeDeath {
    u32 personality;
    u16 species;
    u8 level;
    u16 location; // a location name; 0xFFFF: not known
    char name[NUZLOCKE_MODE_NAME_LEN];
} NuzlockeModeDeath;
int NuzlockeMode_AreaCount(void);
BOOL NuzlockeMode_AreaAt(int index, u16 *location, u16 *species, u8 *result);
int NuzlockeMode_DeathCount(void);
BOOL NuzlockeMode_DeathAt(int index, NuzlockeModeDeath *out);
BOOL NuzlockeMode_DeathLogged(u32 personality);
void NuzlockeMode_RecordDeath(const NuzlockeModeDeath *death);
// What the wild battle going on is to the catch rule
enum NuzlockeModeEncounter {
    NUZLOCKE_MODE_ENC_NONE = 0, // no wild battle, or the rule doesn't apply (Simple, no Poke Balls yet)
    NUZLOCKE_MODE_ENC_FIRST,    // the area's first encounter: it counts
    NUZLOCKE_MODE_ENC_USED,     // the area's encounter was already used
    NUZLOCKE_MODE_ENC_SHINY,    // shiny: doesn't count, can be caught
    NUZLOCKE_MODE_ENC_EXEMPT,   // a static encounter with Normal: doesn't count, can be caught
};
int NuzlockeMode_CurrentEncounter(void);

// The rules, implemented per game (game thread). All are no-ops with the mode off.
// The level no Pokemon may pass by EXP or Rare Candy (the next gym leader's strongest Pokemon's level, its
// weakest's with Hard); 0 with the mode off
int NuzlockeMode_LevelCap(void);
// The HP a Pokemon's HP is about to be set to: 0 for a dead one; and a living one set to 0 becomes dead
u32 NuzlockeMode_FilterHP(struct Pokemon *mon, u32 hp);
void NuzlockeMode_OnHPSet(struct Pokemon *mon, u32 hp);
// A medicine (or Rare Candy) the mode refuses on mon: any, on a dead one; level-raising ones past the cap
BOOL NuzlockeMode_RefusesItem(struct Pokemon *mon, u16 item, int heapID);

#ifdef __cplusplus
}
#endif

#endif // PORT_NUZLOCKE_MODE_H

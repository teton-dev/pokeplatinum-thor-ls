#ifndef GEN5_API_H
#define GEN5_API_H

// The Gen 5 battle code's interface to the port (teton-dev fork; docs/thor/dev/gen5-battles.md). Its functions are
// named Gen5_..., which the build's renaming of BW2's symbols (bw2_...) leaves alone, so that Platinum's code can call
// them. Built with -Dpokebw2=..., which defines GEN5_BATTLES for the game's code.

#ifdef __cplusplus
extern "C" {
#endif

// BW2's archives are read from the Black 2 or White 2 files in gen5Dir (the data folder's gen5/, an absolute path),
// whatever folder the current game runs in. FALSE if the folder can't be reached from there.
int Gen5_FilesInit(const char *gen5Dir);

// Reads a file through BW2's own archive code and checks it: Bulbasaur's base stats in the Pokémon data. message
// says what was read, or what went wrong. Gen5_FilesInit first.
int Gen5_SelfCheck(char *message, int size);

// A Pokémon as the port and BW2's code exchange it, in neither game's format. Species, moves, items and abilities
// keep their numbers from Gen 4 to Gen 5. Strings are UTF-16, ended by 0xffff
#define GEN5_PARTY_MAX 6
#define GEN5_NICKNAME_LENGTH 11 // With the end
#define GEN5_NAME_LENGTH 8 // A trainer's, with the end

enum Gen5Stat {
    GEN5_STAT_HP,
    GEN5_STAT_ATTACK,
    GEN5_STAT_DEFENSE,
    GEN5_STAT_SPEED,
    GEN5_STAT_SP_ATTACK,
    GEN5_STAT_SP_DEFENSE,
    GEN5_STAT_COUNT,
};

typedef struct Gen5Mon {
    int species;
    int form;
    int isEgg;
    unsigned personality;
    unsigned otId; // The trainer's ID, with the secret ID in the upper half
    unsigned exp;
    int level;
    int heldItem;
    int ability;
    int nature;
    int friendship;
    int moves[4];
    int pp[4];
    int ppUps[4];
    int ivs[GEN5_STAT_COUNT];
    int evs[GEN5_STAT_COUNT];
    int hp; // Current HP (BW2 works out the stats itself)
    unsigned status; // The party status bits, which Gen 4 and Gen 5 share (sleep turns, poison, burn...)
    int ball; // The Poké Ball's item
    int metLevel;
    int originGame;
    int language;
    int otGender;
    int pokerus;
    int fatefulEncounter;
    int hasNickname;
    unsigned short nickname[GEN5_NICKNAME_LENGTH];
    unsigned short otName[GEN5_NAME_LENGTH];
} Gen5Mon;

// An item in the bag and how many there are
typedef struct Gen5Item {
    int item;
    int count;
} Gen5Item;

#define GEN5_ITEMS_MAX 512

// A wild single battle (milestones 3 and 4): the player's party against one wild Pokémon
typedef struct Gen5BattleSetup {
    Gen5Mon party[GEN5_PARTY_MAX];
    int partyCount;
    Gen5Mon wild;
    unsigned short playerName[GEN5_NAME_LENGTH];
    unsigned playerId; // With the secret ID in the upper half
    int playerGender;
    int badges; // How many: obedience
    unsigned money;
    // The bag's items the battle can use (medicine, Poké Balls, berries, battle items)
    Gen5Item items[GEN5_ITEMS_MAX];
    int itemCount;
    int hour;
    int minute;
} Gen5BattleSetup;

// How a battle ended, for Platinum
enum Gen5BattleResult {
    GEN5_BATTLE_WON,
    GEN5_BATTLE_LOST,
    GEN5_BATTLE_RAN,
    GEN5_BATTLE_CAUGHT,
    GEN5_BATTLE_FAILED, // The Gen 5 code couldn't start or run it
};

// What the battle left: the party as it is after it (in the setup's order), the wild Pokémon (caught: with its
// ball), and the counts of the setup's items
typedef struct Gen5BattleResults {
    enum Gen5BattleResult result;
    Gen5Mon party[GEN5_PARTY_MAX];
    int partyCount;
    Gen5Mon wild;
    Gen5Item items[GEN5_ITEMS_MAX];
    int itemCount;
    int moneyChange; // Pay Day and the like, or what a loss costs (negative)
} Gen5BattleResults;

// Starts BW2's systems the first time (they stay up), then the battle. Its files are in gen5Dir (Gen5_FilesInit).
// FALSE if it can't start.
int Gen5_BattleBegin(const Gen5BattleSetup *setup, const char *gen5Dir);

// The keys held (PAD_Read's bits) and the touch screen this frame, as the game read them: BW2's code reads them
// from here. Before each Gen5_BattleFrame
void Gen5_SetInput(unsigned keys, int touch, int x, int y);

// What happened in the battle since the last call, as bits (the game's test scripts wait for them)
enum Gen5BattleEvent {
    GEN5_EVENT_COMMAND = 1 << 0, // The battle asks the player what to do
};

unsigned Gen5_TakeEvents(void);

// Once a frame, before the VBlank: TRUE while the battle runs
int Gen5_BattleFrame(void);

// In the VBlank: BW2's VBlank work (VRAM transfers, its VBlank tasks and callback)
void Gen5_BattleVBlank(void);

// After the last frame: frees the battle and gives what it left
void Gen5_BattleEnd(Gen5BattleResults *results);

#ifdef __cplusplus
}
#endif

#endif // GEN5_API_H

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

// A Pokémon as both games see it (milestone 4). Species, forms, moves, items, abilities, natures and balls have the
// same numbers in Gen 4 and Gen 5. Stats are left out: BW2 computes them from these.
#define GEN5_NAME_LENGTH 11 // Characters of a Pokémon's name, with the terminator; a trainer's has 8
#define GEN5_PARTY_MAX 6

// Major status conditions, as BW2 numbers them
enum Gen5Status {
    GEN5_STATUS_NONE,
    GEN5_STATUS_PARALYSIS,
    GEN5_STATUS_SLEEP,
    GEN5_STATUS_FREEZE,
    GEN5_STATUS_BURN,
    GEN5_STATUS_POISON,
};

typedef struct Gen5Pokemon {
    unsigned species;
    unsigned form;
    unsigned level;
    unsigned exp;
    unsigned personality;
    unsigned otId;
    unsigned heldItem;
    unsigned ability;
    unsigned friendship;
    unsigned gender; // 0 male, 1 female, 2 none
    unsigned moves[4];
    unsigned pp[4];
    unsigned ppUps[4];
    unsigned hp;
    unsigned status; // enum Gen5Status
    unsigned ivs[6]; // HP, Attack, Defense, Speed, Sp. Atk, Sp. Def
    unsigned evs[6];
    unsigned ball;
    unsigned pokerus;
    unsigned metLevel;
    unsigned otGender;
    unsigned isEgg;
    unsigned hasNickname;
    unsigned short nickname[GEN5_NAME_LENGTH]; // Unicode, 0xFFFF after the last character
    unsigned short otName[8];
} Gen5Pokemon;

// The player, for BW2's messages, its traded-Pokémon check (the OT's ID and name) and the money a loss costs
typedef struct Gen5Player {
    unsigned short name[8]; // Unicode, 0xFFFF after the last character
    unsigned id;
    unsigned gender; // 0 male, 1 female
    unsigned money;
    unsigned badges;
} Gen5Player;

// An item of the player's bag and how many there are (item numbers are the same in both games)
#define GEN5_ITEMS_MAX 512
typedef struct Gen5Item {
    unsigned short item;
    unsigned short count;
} Gen5Item;

// A trainer the player battles (milestone 5): Platinum's trainer, whose party is made as Platinum makes it. Its class
// name is Platinum's; BW2 shows the sprite of its nearest class (gen5/src/trainers.c).
#define GEN5_TEXT_LENGTH 400
typedef struct Gen5Trainer {
    unsigned id; // Platinum's trainer ID
    unsigned platinumClass;
    unsigned aiFlags; // Platinum's AI flags (gen5/src/trainers.c makes them BW2's)
    unsigned items[4];
    unsigned prizeBase; // The class's prize: the money for winning is its last Pokémon's level times 4 times this
    unsigned short name[16]; // Unicode, 0xFFFF after the last character
    unsigned short className[32];
    // What the trainer says when beaten, in BW2's text: 0xFFFE between lines, a page break as BW2's command
    unsigned short defeatText[GEN5_TEXT_LENGTH];
    Gen5Pokemon party[GEN5_PARTY_MAX];
    int partyCount;
} Gen5Trainer;

// A single battle: the player's party and bag against one wild Pokémon, or a trainer (isTrainer). Gen5_BattleEnd writes back the party as
// the battle left it (HP, status, PP, experience, levels, moves, held items), what is left of each item, the money
// won or lost, and a caught Pokémon as the battle left it (with the ball it was caught in).
typedef struct Gen5Battle {
    Gen5Player player;
    int arenaBg; // BW2's battle background (the field environment's bgType) and terrain: where the battle is
    int arenaTerrain;
    int textSpeed; // Platinum's Text Speed option: 0 slow, 1 normal, 2 fast (BW2's own numbers for them)
    int setStyle; // Platinum's Battle Style option: 1 is Set (no free switch after the opponent's Pokémon faints)
    Gen5Pokemon party[GEN5_PARTY_MAX];
    int partyCount;
    Gen5Item items[GEN5_ITEMS_MAX];
    int itemCount;
    Gen5Pokemon wild;
    int isTrainer;
    Gen5Trainer trainer;
    int moneyChange;
    int caught; // The wild Pokémon was caught: caughtMon has it
    Gen5Pokemon caughtMon;
} Gen5Battle;

// How a battle ended, for Platinum
enum Gen5BattleResult {
    GEN5_BATTLE_WON,
    GEN5_BATTLE_LOST,
    GEN5_BATTLE_RAN,
    GEN5_BATTLE_CAUGHT,
    GEN5_BATTLE_FAILED, // The Gen 5 code couldn't start or run it
};

// Starts BW2's systems the first time (they stay up), then the battle. Its files are in gen5Dir (Gen5_FilesInit).
// FALSE if it can't start.
int Gen5_BattleBegin(const Gen5Battle *battle, const char *gen5Dir);

// Once a frame, before the VBlank: TRUE while the battle runs
// The keys held (PAD_Read's bits) and the touch screen this frame, as the game read them: BW2's code reads them
// from here. Before each Gen5_BattleFrame
void Gen5_SetInput(unsigned keys, int touch, int x, int y);

// What happened in the battle since the last call, as bits (the game's test scripts wait for them)
enum Gen5BattleEvent {
    GEN5_EVENT_COMMAND = 1 << 0, // The battle asks the player what to do
};

unsigned Gen5_TakeEvents(void);

int Gen5_BattleFrame(void);

// Widescreen (milestone 6): BW2's 3D drawn ratio times as wide as the DS's, unstretched (its camera and the Pokémon
// sprites' own projection widened), for libntr's wide scene; 1 for the DS's. Before each Gen5_BattleFrame
void Gen5_SetWidescreen(float ratio);
// Whether a move's full-screen background is up, for its sides to be mirrored in a wide scene
int Gen5_MoveBgShown(void);

// In the VBlank: BW2's VBlank work (VRAM transfers, its VBlank tasks and callback)
void Gen5_BattleVBlank(void);

// After the last frame: frees the battle and gives its result. The party and money change go into battle, the one
// Gen5_BattleBegin had (unchanged if the battle failed).
enum Gen5BattleResult Gen5_BattleEnd(Gen5Battle *battle);

// The Pokémon summary screen, in BW2's own (the summary of "Gen 5 screens"): the player's party (or one Pokémon) and
// which to show. partyCount Pokémon can be paged through; mode 1 asks which move to forget for `move` (Gen5Summary.slot
// and result say what was chosen). Gen5_SummaryBegin, then the same Gen5_SetInput / Gen5_SetWidescreen /
// Gen5_BattleFrame / Gen5_BattleVBlank as a battle, until Gen5_BattleFrame returns 0, then Gen5_SummaryEnd.
enum Gen5SummaryMode {
    GEN5_SUMMARY_NORMAL,
    GEN5_SUMMARY_FORGET_MOVE, // Pick a move to forget for `move`
};

enum Gen5SummaryResult {
    GEN5_SUMMARY_BACK, // Left with B or the back button
    GEN5_SUMMARY_CLOSE, // Left with X, to the field
    GEN5_SUMMARY_FORGET, // A move was chosen to forget: slot
    GEN5_SUMMARY_FAILED,
};

typedef struct Gen5Summary {
    Gen5Player player;
    Gen5Pokemon party[GEN5_PARTY_MAX];
    int partyCount;
    int index; // The Pokémon shown first; the one shown last when it ends
    int mode; // enum Gen5SummaryMode
    unsigned move; // The move to be learnt, in GEN5_SUMMARY_FORGET_MOVE
    int slot; // The move slot chosen to forget
    int result; // enum Gen5SummaryResult
} Gen5Summary;

int Gen5_SummaryBegin(const Gen5Summary *summary, const char *gen5Dir);
enum Gen5SummaryResult Gen5_SummaryEnd(Gen5Summary *summary);

#ifdef __cplusplus
}
#endif

#endif // GEN5_API_H

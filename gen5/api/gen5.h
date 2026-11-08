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

// A wild single battle in BW2's engine (milestone 3): the player's Pokémon and the wild one, by species and level
// (Gen 4 and Gen 5 share their numbers)
typedef struct Gen5WildBattle {
    int playerSpecies;
    int playerLevel;
    int wildSpecies;
    int wildLevel;
} Gen5WildBattle;

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
int Gen5_BattleBegin(const Gen5WildBattle *battle, const char *gen5Dir);

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

// In the VBlank: BW2's VBlank work (VRAM transfers, its VBlank tasks and callback)
void Gen5_BattleVBlank(void);

// After the last frame: frees the battle and gives its result
enum Gen5BattleResult Gen5_BattleEnd(void);

#ifdef __cplusplus
}
#endif

#endif // GEN5_API_H

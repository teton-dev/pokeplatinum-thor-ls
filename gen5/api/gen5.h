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

#ifdef __cplusplus
}
#endif

#endif // GEN5_API_H

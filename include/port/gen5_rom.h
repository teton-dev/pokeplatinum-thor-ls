#ifndef POKEPLATINUM_PORT_GEN5_ROM_H
#define POKEPLATINUM_PORT_GEN5_ROM_H

// The player's Black 2 or White 2 (USA) ROM for the Gen 5 battles (teton-dev fork; docs/thor/dev/gen5-battles.md):
// checked against the dumps pokebw2 decompiles (game code and SHA1), and its archives and sound data extracted into
// the data folder's gen5/, shared by every game. The ROM itself isn't kept. Built with GEN5_BATTLES.

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Gen5RomState {
    GEN5_ROM_NONE, // Nothing imported
    GEN5_ROM_IMPORTING, // Checking or extracting, on a thread of its own
    GEN5_ROM_READY, // Extracted, and the Gen 5 code read it (the self-check)
    GEN5_ROM_FAILED, // The last import failed (Gen5Rom_Status says why); an earlier one may still be ready
};

// Starts importing the ROM at path; deleteAfter: remove that file when done (the picker's copy on Android)
void Gen5Rom_Import(const char *path, BOOL deleteAfter);

// Called every frame the settings show: finishes an import on the game's thread (the self-check)
void Gen5Rom_Update(void);

enum Gen5RomState Gen5Rom_State(void);

// The import's progress, its failure, or the self-check's result
const char *Gen5Rom_Status(void);

// "Black 2" or "White 2" once one is imported, else NULL
const char *Gen5Rom_GameName(void);

// The extracted files' folder (an absolute path), whether or not there are any
const char *Gen5Rom_Dir(void);

// Deletes the extracted files
BOOL Gen5Rom_Remove(void);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_GEN5_ROM_H

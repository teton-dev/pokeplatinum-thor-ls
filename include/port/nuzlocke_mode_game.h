#ifndef PORT_NUZLOCKE_MODE_GAME_H
#define PORT_NUZLOCKE_MODE_GAME_H

// What Nuzlocke mode's core (port/nuzlocke_mode.h) needs from a game. Platinum's are in
// src/port/nuzlocke_platinum.c. All called on the game thread.

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// The running save's identity (its trainer ID) and slot; FALSE when no game is running
BOOL NuzlockeModeGame_RunningSave(u32 *saveId, int *slot);

// Where the records go in the save image: count sectors from primary, mirrored from backup,
// none of them used by the game
void NuzlockeModeGame_RecordSectors(u32 *primary, u32 *backup, int *count, u32 *sectorSize);

// FALSE while the game is writing its save (the records wait for it to finish)
BOOL NuzlockeModeGame_CanWrite(void);

// The bag has a Poke Ball (any ball)
BOOL NuzlockeModeGame_HasPokeBalls(void);

BOOL NuzlockeModeGame_ReadFlash(u32 address, void *data, u32 size);
BOOL NuzlockeModeGame_WriteFlash(u32 address, void *data, u32 size);

#ifdef __cplusplus
}
#endif

#endif // PORT_NUZLOCKE_MODE_GAME_H

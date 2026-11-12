#ifndef POKEPLATINUM_PORT_GEN5_DATA_H
#define POKEPLATINUM_PORT_GEN5_DATA_H

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Before a Gen 5 battle: for a game whose Pokemon and move data isn't vanilla Platinum's (Renegade Platinum), makes
// the copies of BW2's data with the game's own base stats, types, abilities and move numbers (src/port/gen5_data.c)
// and has BW2 read them. TRUE when it does.
BOOL Gen5Data_Prepare(void);

// Before a Gen 5 battle: makes (once) the Team Galactic arena, BW2's Champion room with Crobat purple and Galactic
// blue streaks and gleaming stars, from the player's ROM into copies of two of BW2's archives in gen5_data/, and has
// BW2 read them (src/port/gen5_galactic.c). It is background 16, terrain 1 (the Champion's room, which BW2's
// terrain 0 is). TRUE when it is there.
BOOL Gen5Data_PrepareGalacticArena(void);

// A move of the game as BW2's number for it, and back: Renegade Platinum's replaced moves are BW2's (Brine is Scald)
// or stand for BW2's (Moonblast is Night Daze, named Moonblast); the same number in any other game
u16 Gen5Data_MoveToGen5(u16 move);
u16 Gen5Data_MoveFromGen5(u16 move);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_GEN5_DATA_H

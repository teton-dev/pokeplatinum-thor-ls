#ifndef POKEPLATINUM_PORT_GEN5_GALACTIC_H
#define POKEPLATINUM_PORT_GEN5_GALACTIC_H

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// The Team Galactic arena (src/port/gen5_galactic.c): changes, in place, a copy of BW2's Champion room's model resource
// (member 115 of archive 11, a BMD0 with its textures): the streaks in Crobat purple and Galactic blue, the sky with
// gleaming stars. FALSE if it isn't the resource it expects.
BOOL Gen5Galactic_Make(u8 *res, u32 size);

// The same for the platforms' model resource (member 38 of archive 11): dark indigo in place of gold and cream
BOOL Gen5Galactic_MakeStage(u8 *res, u32 size);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_GEN5_GALACTIC_H

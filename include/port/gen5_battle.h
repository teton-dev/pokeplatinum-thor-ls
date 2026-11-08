#ifndef POKEPLATINUM_PORT_GEN5_BATTLE_H
#define POKEPLATINUM_PORT_GEN5_BATTLE_H

// Battles in Black 2 and White 2's engine (teton-dev fork; docs/thor/dev/gen5-battles.md). Built with GEN5_BATTLES.
// For now (milestone 3) a debug entry starts a wild battle in it; FieldSystem_StartBattleProcess runs the Gen 5
// battle application in place of Platinum's when one is requested.

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#else
#include "overlay_manager.h" // C only: a field of its struct is named template

// Runs Platinum's battles in the Gen 5 engine; the DTO's results come back as Platinum's battle leaves them
extern const ApplicationManagerTemplate gGen5BattleApplicationTemplate;
#endif

// The next battle the field starts runs in the Gen 5 engine
void Gen5Battle_RequestNext(void);

// Whether the battle being started should run in the Gen 5 engine (and the request is used up)
BOOL Gen5Battle_TakeRequest(void);

// A debug wild battle against species at level in the Gen 5 engine, from the overworld. FALSE if it can't start
// now (no BW2 ROM imported, the player isn't free, an empty party...)
BOOL Gen5Battle_StartDebug(int species, int level);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_GEN5_BATTLE_H

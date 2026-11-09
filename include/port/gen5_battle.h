#ifndef POKEPLATINUM_PORT_GEN5_BATTLE_H
#define POKEPLATINUM_PORT_GEN5_BATTLE_H

// Battles in Black 2 and White 2's engine (teton-dev fork; docs/thor/dev/gen5-battles.md). Built with GEN5_BATTLES.
// FieldSystem_StartBattleProcess runs the Gen 5 battle application in place of Platinum's for the battles it can
// run (Gen5Battle_Wants), with Settings > Gameplay > Gen 5 battles on, and for the debug entry's.

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

// Whether the battle being started should run in the Gen 5 engine: a debug one requested (the request is used up),
// or, with the setting on and a ROM imported, one it can run. For now (milestone 4): single wild battles against
// a Pokémon the player found (grass, caves, water, honey trees), not scripted ones (legendaries, the first battle)
struct FieldBattleDTO;
BOOL Gen5Battle_Wants(const struct FieldBattleDTO *dto);

// A debug wild battle against species at level in the Gen 5 engine, from the overworld. FALSE if it can't start
// now (no BW2 ROM imported, the player isn't free, an empty party...)
BOOL Gen5Battle_StartDebug(int species, int level);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_GEN5_BATTLE_H

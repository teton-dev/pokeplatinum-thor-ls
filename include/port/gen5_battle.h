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

// A battle is running in the Gen 5 engine (not yet the catch's naming screen or an evolution after it):
// widescreen widens its camera (port/widescreen.h)
BOOL Gen5Battle_IsShown(void);
// A move's full-screen background is up in it (widescreen mirrors the sides)
BOOL Gen5Battle_MoveBgShown(void);
// After a battle in it: Platinum's Pokedex entry or naming screen (a catch) or evolution scene is shown (widescreen
// continues their sides)
BOOL Gen5Battle_PostBattleScreenShown(void);
// The Battle Tower's single battles in the Gen 5 engine (overlay104/frscrcmd_battle_tower.c): TRUE to run this one there
BOOL Gen5Battle_WantsFrontier(const struct FieldBattleDTO *dto);
// The last Frontier battle ran in the Gen 5 engine (set at each Tower battle, cleared when a facility starts a new
// recording): Platinum can't record it, so the Frontier's Battle Video question and Record menu entry are skipped
// (overlay104/frscrcmd.c, ov104_02231F74.c)
void Gen5Battle_SetFrontierUnrecorded(BOOL unrecorded);
BOOL Gen5Battle_FrontierUnrecorded(void);
// Of those, the evolution scene (its backdrop, BG3, is a gradient: widescreen mirrors it across the sides)
BOOL Gen5Battle_EvolutionShown(void);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_GEN5_BATTLE_H

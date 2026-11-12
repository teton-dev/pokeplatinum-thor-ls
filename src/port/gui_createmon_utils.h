#ifndef GUI_CREATEMON_UTILS_H
#define GUI_CREATEMON_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

// NOTE: You will need to free the pointer returned by this!
const char * GUI_CreateMon_GetSpeciesName(int speciesNo);
int GUI_CreateMon_GenPokemon(int speciesNo, int level);
// Starts a battle from the overworld: against a wild speciesNo at level, or against
// trainerID (0: wild) when it is set, as a double battle if doubles is set
int GUI_CreateMon_StartBattle(int speciesNo, int level, int trainerID, int doubles);
// A battle against two trainers (trainer1 and trainer2, as when both see the player), beside partner when it isn't 0,
// as Encounter_NewVsTrainer starts it
int GUI_CreateMon_StartTrainerBattle(int trainer1, int trainer2, int partner);
// A wild speciesNo at level met as in tall grass: the field's own wild encounter (its blackout, Poke Radar and the
// rest), which Settings > Gameplay > Gen 5 battles sends to the Gen 5 engine. FALSE if the player isn't free
int GUI_CreateMon_StartWildEncounter(int speciesNo, int level);

#ifdef __cplusplus
}
#endif


#endif
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
// The same, a shiny one when shiny is TRUE (the test command wild SPECIES LEVEL shiny)
int GUI_CreateMon_StartWildEncounterShiny(int speciesNo, int level, BOOL shiny);
// The same as the Safari Zone's: with balls Safari Balls and the Safari game flag on (the test command safari SPECIES
// LEVEL BALLS; the game is left on, as the Great Marsh's gate would have it)
int GUI_CreateMon_StartSafariEncounter(int speciesNo, int level, int balls);

#ifdef __cplusplus
}
#endif


#endif
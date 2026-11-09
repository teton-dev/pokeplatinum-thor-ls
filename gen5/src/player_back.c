// The port (teton-dev fork): the player's back sprite in Gen 5 battles, the owner's own art of Lucas and Dawn
// (gen5/include/gen5/player_back.h). The art's data is generated at build time from gen5/art by
// gen5/tools/player_back.py.

#include "types.h"
#include "constants/trainer_classes.h"
#include "gen5/player_back.h"

#include "gen5_player_back_data.h"

const Gen5BackSprite *PlayerBack_ForTrainerClass(u32 trainerClass) {
    switch (trainerClass) {
    // BW2's player classes: its own players (Nate, Rosa), their Pokéwood and Black/White versions. A battle the port
    // runs gives the player one of the last (a game other than BW2: btl_main.c's TrainerData_SetFromPlayer)
    case TRAINER_CLASS_PKMN_TRAINER_NATE:
    case TRAINER_CLASS_PKMN_TRAINER_M_180:
    case TRAINER_CLASS_PKMN_TRAINER_M_182:
    case TRAINER_CLASS_PKMN_TRAINER_M_184:
        return &sLucasBack;
    case TRAINER_CLASS_PKMN_TRAINER_ROSA:
    case TRAINER_CLASS_PKMN_TRAINER_F_181:
    case TRAINER_CLASS_PKMN_TRAINER_F_183:
    case TRAINER_CLASS_PKMN_TRAINER_F_185:
        return &sDawnBack;
    default:
        return NULL;
    }
}

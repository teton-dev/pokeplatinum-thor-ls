#include <nitro.h>


#include "gui_createmon_utils.h"

#include "party.h"
#include "pokemon.h"
#include "savedata.h"
#include "species.h"
#include "message_util.h"

#include "charcode_convert.h"

#include "constants/heap.h"
#include "encounter.h"
#include "field_battle_data_transfer.h"
#include "field/field_system.h"
#include "field_task.h"
#include "heap.h"
#include "port/debug_field.h"
#include "unk_020528D0.h"

#include <stdlib.h>


const char * GUI_CreateMon_GetSpeciesName(int speciesNo) {
    String * string = MessageUtil_SpeciesName(speciesNo, HEAP_ID_SYSTEM);
    char * nameBuf = malloc(sizeof(char) * string->size + 1);
    CharCode_ToAsciiString(string->data, nameBuf, string->size);
    nameBuf[string->size] = 0;
    String_Free(string);

    return nameBuf;
}

int GUI_CreateMon_GenPokemon(int speciesNo, int level) {
        Pokemon myPoke;

        Pokemon_InitWith(&myPoke, 
                         speciesNo, 
                         level, 
                         INIT_IVS_RANDOM, 
                         FALSE, 
                         0, 
                         OTID_NOT_SHINY, 
                         0);

        SaveData * mySaveData = SaveData_Ptr();
        Party * myParty = SaveData_GetParty(mySaveData);
        if(myParty->currentCount >= 6) {
            return FALSE;
        } else {
            Party_AddPokemon(myParty, &myPoke);
        }

        return TRUE;
        
}

// --- debug battles ---------------------------------------------------------------------

typedef struct DebugBattle {
    int state;
    int species;
    int level;
    int trainerID;
    BOOL doubles;
    int resultMask;
} DebugBattle;

static BOOL FieldTask_DebugBattle(FieldTask *task)
{
    DebugBattle *battle = FieldTask_GetEnv(task);

    switch (battle->state++) {
    case 0:
        if (battle->trainerID != 0) {
            Encounter_NewVsTrainer(task, battle->trainerID, battle->doubles ? battle->trainerID : 0, 0, HEAP_ID_FIELD2,
                &battle->resultMask);
        } else {
            Encounter_NewVsSpeciesAtLevel(task, battle->species, battle->level, &battle->resultMask, FALSE);
        }
        return FALSE;
    case 1:
        // These are the scripted encounters' calls, whose script blacks out after a loss (the encounter leaves the
        // map closed for it). Without one, the screen stayed black
        if (!CheckPlayerWonBattle(battle->resultMask)) {
            FieldTask_StartBlackOutFromBattle(task);
            return FALSE;
        }
        break;
    }

    Heap_Free(battle);
    return TRUE;
}

int GUI_CreateMon_StartBattle(int speciesNo, int level, int trainerID, int doubles)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || FieldSystem_IsRunningTask(fieldSystem)
        || Party_GetCurrentCount(SaveData_GetParty(fieldSystem->saveData)) == 0
        || (trainerID == 0 && (speciesNo <= 0 || speciesNo > MAX_SPECIES))) {
        return FALSE;
    }

    DebugBattle *battle = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(DebugBattle));
    battle->state = 0;
    battle->species = speciesNo;
    battle->level = level;
    battle->trainerID = trainerID;
    battle->doubles = doubles;
    FieldSystem_CreateTask(fieldSystem, FieldTask_DebugBattle, battle);
    return TRUE;
}

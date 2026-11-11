// The Pokemon summary screen in BW2's own; see include/port/gen5_summary.h
#include "port/gen5_summary.h"

#include <nitro.h>
#include <simulator/sim.h>
#include <stdio.h>
#include <string.h>

#include "applications/pokemon_summary_screen/main.h"
#include "charcode_convert.h"
#include "gen5.h"
#include "party.h"
#include "pokemon.h"
#include "port/gen5_rom.h"
#include "port/sim_config_prj.h"
#include "save_player.h"
#include "savedata.h"
#include "system.h"
#include "trainer_info.h"

// gen5_battle.c
void Gen5_TextToGen5(const charcode_t *src, int srcLength, unsigned short *dest, int destLength);
void Gen5_PokemonToGen5(Pokemon *mon, Gen5Pokemon *dest);

static Gen5Summary sSummary;
static BOOL sActive;
static BOOL sFailed;

static void VBlank(void *data)
{
    (void)data;
    Gen5_BattleVBlank();
}

BOOL Gen5Summary_IsActive(void)
{
    return sActive;
}

BOOL Gen5Summary_Begin(ApplicationManager *appMan)
{
    PokemonSummary *args = ApplicationManager_Args(appMan);

    if (sActive || !SIM_Config_prj_GetConfig()->gen5Screens || Gen5Rom_GameName() == NULL) {
        return FALSE;
    }
    // What BW2's screen can do for now: a Pokemon or a party, to look at or to choose a move to forget. The Poffin,
    // contest condition and box Pokemon cases stay Platinum's
    if ((args->mode != SUMMARY_MODE_NORMAL && args->mode != SUMMARY_MODE_SELECT_MOVE)
        || (args->dataType != SUMMARY_DATA_PARTY_MON && args->dataType != SUMMARY_DATA_MON)) {
        return FALSE;
    }

    memset(&sSummary, 0, sizeof(sSummary));
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(SaveData_Ptr());
    Gen5_TextToGen5(TrainerInfo_Name(trainerInfo), TRAINER_NAME_LEN + 1, sSummary.player.name, 8);
    sSummary.player.id = TrainerInfo_ID(trainerInfo);
    sSummary.player.gender = TrainerInfo_Gender(trainerInfo);
    if (args->dataType == SUMMARY_DATA_MON) {
        Gen5_PokemonToGen5(args->monData, &sSummary.party[0]);
        sSummary.partyCount = 1;
    } else if (args->monMax == 1) {
        // One Pokemon of the party (the move reminder's, the tutor's): its slot comes back as it went
        Gen5_PokemonToGen5(Party_GetPokemonBySlotIndex(args->monData, args->monIndex), &sSummary.party[0]);
        sSummary.partyCount = 1;
    } else {
        sSummary.partyCount = Party_GetCurrentCount(args->monData);
        if (sSummary.partyCount > GEN5_PARTY_MAX) {
            sSummary.partyCount = GEN5_PARTY_MAX;
        }
        for (int i = 0; i < sSummary.partyCount; i++) {
            Gen5_PokemonToGen5(Party_GetPokemonBySlotIndex(args->monData, i), &sSummary.party[i]);
        }
        sSummary.index = args->monIndex < sSummary.partyCount ? args->monIndex : 0;
    }
    if (args->mode == SUMMARY_MODE_SELECT_MOVE) {
        sSummary.mode = GEN5_SUMMARY_FORGET_MOVE;
        sSummary.move = args->move;
    }

    if (!Gen5_SummaryBegin(&sSummary, Gen5Rom_Dir())) {
        printf("Gen 5: the summary screen couldn't start\n");
        return FALSE;
    }
    SetVBlankCallback(VBlank, NULL);
    sActive = TRUE;
    sFailed = FALSE;
    return TRUE;
}

BOOL Gen5Summary_Main(ApplicationManager *appMan)
{
    (void)appMan;
    Gen5_SetInput(gSystem.heldKeysRaw, gSystem.touchHeld, gSystem.touchX, gSystem.touchY);
    Gen5_SetWidescreen(1.0f);
    return !Gen5_BattleFrame();
}

void Gen5Summary_Exit(ApplicationManager *appMan)
{
    PokemonSummary *args = ApplicationManager_Args(appMan);

    SetVBlankCallback(NULL, NULL);
    const enum Gen5SummaryResult result = Gen5_SummaryEnd(&sSummary);
    sActive = FALSE;
    if (result == GEN5_SUMMARY_FAILED || sFailed) {
        return;
    }
    // The Pokemon shown last, as the party menu's cursor goes back to it (a one-Pokemon summary keeps its slot)
    if (args->dataType == SUMMARY_DATA_PARTY_MON && args->monMax != 1) {
        args->monIndex = sSummary.index;
    }
    if (args->mode == SUMMARY_MODE_SELECT_MOVE) {
        args->selectedMoveSlot = sSummary.slot;
        args->returnMode = result == GEN5_SUMMARY_FORGET ? SUMMARY_RETURN_SELECT : SUMMARY_RETURN_CANCEL;
    }
}

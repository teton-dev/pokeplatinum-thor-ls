// The party list in BW2's own; see include/port/gen5_party.h
#include "port/gen5_party.h"

#include <nitro.h>
#include <simulator/sim.h>
#include <stdio.h>
#include <string.h>

#include "applications/party_menu/defs.h"
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

static Gen5Party sParty;
static BOOL sActive;

static void VBlank(void *data)
{
    (void)data;
    Gen5_BattleVBlank();
}

BOOL Gen5Party_IsActive(void)
{
    return sActive;
}

BOOL Gen5Party_Begin(ApplicationManager *appMan)
{
    PartyMenu *args = ApplicationManager_Args(appMan);

    if (sActive || !SIM_Config_prj_GetConfig()->gen5Screens || Gen5Rom_GameName() == NULL) {
        return FALSE;
    }
    // What the Gen 5 list does for now: look at the party from the start menu, and pick a Pokemon. The other modes
    // (items, moves, contests, the Battle Frontier...) stay Platinum's
    if (args->type != PARTY_MENU_TYPE_BASIC
        || (args->mode != PARTY_MENU_MODE_FIELD && args->mode != PARTY_MENU_MODE_SELECT_NO_PROMPT)) {
        return FALSE;
    }

    memset(&sParty, 0, sizeof(sParty));
    TrainerInfo *trainerInfo = SaveData_GetTrainerInfo(SaveData_Ptr());
    Gen5_TextToGen5(TrainerInfo_Name(trainerInfo), TRAINER_NAME_LEN + 1, sParty.player.name, 8);
    sParty.player.id = TrainerInfo_ID(trainerInfo);
    sParty.player.gender = TrainerInfo_Gender(trainerInfo);
    sParty.partyCount = Party_GetCurrentCount(args->party);
    if (sParty.partyCount > GEN5_PARTY_MAX) {
        sParty.partyCount = GEN5_PARTY_MAX;
    }
    for (int i = 0; i < sParty.partyCount; i++) {
        Gen5_PokemonToGen5(Party_GetPokemonBySlotIndex(args->party, i), &sParty.party[i]);
    }
    sParty.mode = args->mode == PARTY_MENU_MODE_FIELD ? GEN5_PARTY_FIELD : GEN5_PARTY_SELECT;
    sParty.cursor = args->selectedMonSlot < sParty.partyCount ? args->selectedMonSlot : 0;

    if (!Gen5_PartyBegin(&sParty, Gen5Rom_Dir())) {
        printf("Gen 5: the party list couldn't start\n");
        return FALSE;
    }
    SetVBlankCallback(VBlank, NULL);
    sActive = TRUE;
    return TRUE;
}

BOOL Gen5Party_Main(ApplicationManager *appMan)
{
    (void)appMan;
    Gen5_SetInput(gSystem.heldKeysRaw, gSystem.touchHeld, gSystem.touchX, gSystem.touchY);
    Gen5_SetWidescreen(1.0f);
    return !Gen5_BattleFrame();
}

void Gen5Party_Exit(ApplicationManager *appMan)
{
    PartyMenu *args = ApplicationManager_Args(appMan);

    SetVBlankCallback(NULL, NULL);
    const enum Gen5PartyResult result = Gen5_PartyEnd(&sParty);
    sActive = FALSE;
    if (result == GEN5_PARTY_FAILED) {
        return;
    }
    // The party's new order (switching two Pokemon): Platinum's party swapped into it
    int current[GEN5_PARTY_MAX];
    for (int i = 0; i < sParty.partyCount; i++) {
        current[i] = i;
    }
    for (int i = 0; i < sParty.partyCount; i++) {
        int from = i;
        while (current[from] != sParty.order[i]) {
            from++;
        }
        if (from != i) {
            Party_SwapSlots(args->party, i, from);
            const int moved = current[i];
            current[i] = current[from];
            current[from] = moved;
        }
    }
    args->menuSelectionResult = PARTY_MENU_EXIT_CODE_DONE;
    // The slot, as Platinum's list reports a pick (7: cancelled)
    args->selectedMonSlot = result == GEN5_PARTY_PICKED ? sParty.picked : 7;
}

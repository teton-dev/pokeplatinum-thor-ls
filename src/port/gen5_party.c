// The party list in BW2's own; see include/port/gen5_party.h
#include "port/gen5_party.h"

#include <nitro.h>
#include <simulator/sim.h>
#include <stdio.h>
#include <string.h>

#include "constants/moves.h"
#include "constants/pokemon.h"

#include "applications/party_menu/defs.h"
#include "field_move_tasks.h"
#include "charcode_convert.h"
#include "gen5.h"
#include "port/hm_free.h"
#include "unk_02054884.h"
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
static const FieldMoveContext *sMoveContext;

// BW2's field move kinds (gen5.h's Gen5HMCheck) as Platinum's; -1: Platinum has none (Dive)
static const int sPlatinumMove[16] = {
    FIELD_MOVE_CUT, FIELD_MOVE_SURF, FIELD_MOVE_WATERFALL, FIELD_MOVE_STRENGTH, FIELD_MOVE_FLY, FIELD_MOVE_FLASH,
    FIELD_MOVE_TELEPORT, FIELD_MOVE_DIG, FIELD_MOVE_SWEET_SCENT, FIELD_MOVE_CHATTER, -1, -1, -1,
    FIELD_MOVE_ROCK_SMASH, FIELD_MOVE_DEFOG, FIELD_MOVE_ROCK_CLIMB,
};

// Whether the move can be used here: Platinum's own check of it, whose answers (0 yes, 1 not here, 2 badge, 3 partner,
// 4 already surfing) are the ones BW2's list takes
static int HMCheck(int kind)
{
    if (sMoveContext == NULL || kind < 0 || kind >= 16 || sPlatinumMove[kind] < 0) {
        return 1;
    }
    FieldMoveErrContext check = (FieldMoveErrContext)FieldMove_GetTaskOrError(FIELD_MOVE_ERROR, sPlatinumMove[kind]);
    return check != NULL ? (int)check(sMoveContext) : 1;
}
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
    sMoveContext = args->mode == PARTY_MENU_MODE_FIELD ? args->fieldMoveContext : NULL;
    sParty.hmCheck = HMCheck;
    if (sMoveContext != NULL && HmFree_Enabled()) {
        // Field moves without HMs: the moves nobody knows that a Pokemon does for the party (Fly anywhere, the others
        // where they can be used right now), as Platinum's own list does it
        static const struct {
            u16 move;
            int fieldMove;
        } hmMoves[] = {
            { MOVE_FLY, FIELD_MOVE_FLY }, { MOVE_SURF, FIELD_MOVE_SURF }, { MOVE_CUT, FIELD_MOVE_CUT },
            { MOVE_STRENGTH, FIELD_MOVE_STRENGTH }, { MOVE_ROCK_SMASH, FIELD_MOVE_ROCK_SMASH },
            { MOVE_WATERFALL, FIELD_MOVE_WATERFALL }, { MOVE_ROCK_CLIMB, FIELD_MOVE_ROCK_CLIMB },
            { MOVE_DEFOG, FIELD_MOVE_DEFOG },
        };
        int lent[GEN5_PARTY_MAX] = { 0 };
        for (int i = 0; i < (int)NELEMS(hmMoves); i++) {
            if (hmMoves[i].move != MOVE_FLY && (sMoveContext->usableMoves & (1 << hmMoves[i].fieldMove)) == 0) {
                continue;
            }
            if (Party_HasMonWithMove(args->party, hmMoves[i].move) != PARTY_SLOT_NONE) {
                continue;
            }
            const int slot = HmFree_SubstituteSlot(args->party, hmMoves[i].move);
            if (slot >= 0 && slot < sParty.partyCount && lent[slot] < 4) {
                sParty.hmFree[slot][lent[slot]++] = hmMoves[i].move;
            }
        }
    }
    sParty.cursor =    sParty.cursor = args->selectedMonSlot < sParty.partyCount ? args->selectedMonSlot : 0;

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
    if (result == GEN5_PARTY_FIELD_MOVE && sPlatinumMove[sParty.fieldMove] >= 0) {
        // The start menu then runs the move for the Pokemon (as it does for Platinum's list)
        args->menuSelectionResult = PARTY_MENU_EXIT_CODE_CUT + sPlatinumMove[sParty.fieldMove];
        args->selectedMonSlot = sParty.picked;
        return;
    }
    // The slot, as Platinum's list reports a pick (7: cancelled)
    args->selectedMonSlot = result == GEN5_PARTY_PICKED ? sParty.picked : 7;
}

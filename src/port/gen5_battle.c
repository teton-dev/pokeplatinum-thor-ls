// Battles in the Gen 5 engine; see include/port/gen5_battle.h
#include "port/gen5_battle.h"

#include <nitro.h>
#include <stdio.h>

#include "constants/battle.h"
#include "constants/species.h"

#include "field_battle_data_transfer.h"
#include "gen5.h"
#include "party.h"
#include "pokemon.h"
#include "port/gen5_rom.h"
#include "port/test_script.h"
#include "system.h"

// gui_createmon_utils.c: a debug wild battle from the overworld
int GUI_CreateMon_StartBattle(int speciesNo, int level, int trainerID, int doubles);

static BOOL sRequested;
static BOOL sFailed;

void Gen5Battle_RequestNext(void)
{
    sRequested = TRUE;
}

BOOL Gen5Battle_TakeRequest(void)
{
    const BOOL requested = sRequested;
    sRequested = FALSE;
    return requested;
}

BOOL Gen5Battle_StartDebug(int species, int level)
{
    if (Gen5Rom_State() != GEN5_ROM_READY && Gen5Rom_GameName() == NULL) {
        printf("Gen 5: no Black 2 or White 2 ROM imported\n");
        return FALSE;
    }
    if (!GUI_CreateMon_StartBattle(species, level, 0, FALSE)) {
        return FALSE;
    }
    Gen5Battle_RequestNext();
    return TRUE;
}

static void VBlank(void *data)
{
    (void)data;
    Gen5_BattleVBlank();
}

static BOOL Init(ApplicationManager *appMan, int *state)
{
    (void)state;
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);
    Pokemon *player = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_PLAYER_1], 0);
    Pokemon *wild = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0);
    Gen5WildBattle battle = {
        .playerSpecies = Pokemon_GetValue(player, MON_DATA_SPECIES, NULL),
        .playerLevel = Pokemon_GetValue(player, MON_DATA_LEVEL, NULL),
        .wildSpecies = Pokemon_GetValue(wild, MON_DATA_SPECIES, NULL),
        .wildLevel = Pokemon_GetValue(wild, MON_DATA_LEVEL, NULL),
    };
    sFailed = !Gen5_BattleBegin(&battle, Gen5Rom_Dir());
    if (sFailed) {
        printf("Gen 5: the battle couldn't start\n");
    } else {
        SetVBlankCallback(VBlank, NULL);
    }
    return TRUE;
}

static BOOL Main(ApplicationManager *appMan, int *state)
{
    (void)appMan;
    (void)state;
    if (sFailed) {
        return TRUE;
    }
    // The keys and touch the game read this frame (the test scripts' among them), unchanged by the button mode
    Gen5_SetInput(gSystem.heldKeysRaw, gSystem.touchHeld, gSystem.touchX, gSystem.touchY);
    const BOOL done = !Gen5_BattleFrame();
    if (Gen5_TakeEvents() & GEN5_EVENT_COMMAND) {
        TestScript_OnEvent(TEST_EVENT_BATTLE_COMMAND);
    }
    return done;
}

static BOOL Exit(ApplicationManager *appMan, int *state)
{
    (void)state;
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);
    SetVBlankCallback(NULL, NULL);
    const enum Gen5BattleResult result = sFailed ? GEN5_BATTLE_FAILED : Gen5_BattleEnd();
    switch (result) {
    case GEN5_BATTLE_WON:
        dto->resultMask = BATTLE_RESULT_WIN;
        break;
    case GEN5_BATTLE_LOST:
        dto->resultMask = BATTLE_RESULT_LOSE;
        break;
    case GEN5_BATTLE_CAUGHT:
        dto->resultMask = BATTLE_RESULT_CAPTURED_MON;
        break;
    default:
        // Ran, or failed: back to the overworld as if the player ran
        dto->resultMask = BATTLE_RESULT_PLAYER_FLED;
        break;
    }
    return TRUE;
}

const ApplicationManagerTemplate gGen5BattleApplicationTemplate = {
    .init = Init,
    .main = Main,
    .exit = Exit,
    .overlayID = FS_OVERLAY_ID_NONE,
};

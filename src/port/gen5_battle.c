// Battles in the Gen 5 engine; see include/port/gen5_battle.h
#include "port/gen5_battle.h"

#include <nitro.h>
#include <stdio.h>
#include <string.h>

#include "constants/battle.h"
#include "constants/battle/condition.h"
#include "constants/charcode.h"
#include "constants/species.h"
#include "constants/string.h"

#include "field_battle_data_transfer.h"
#include "gen5.h"
#include "party.h"
#include "pokemon.h"
#include "charcode_convert.h"
#include "port/gen5_rom.h"
#include "port/test_script.h"
#include "system.h"
#include "trainer_info.h"

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

// The battle as handed to the Gen 5 code, and as it comes back
static Gen5WildBattle sBattle;

// Platinum's text to Unicode, for BW2: the letters, digits and punctuation the ASCII table knows, and the accented e
// of Pokémon. Other characters (Japanese, symbols) are left out for now
static void TextToGen5(const charcode_t *src, int srcLength, unsigned short *dest, int destLength)
{
    int n = 0;
    for (int i = 0; i < srcLength && src[i] != CHAR_EOS && n < destLength - 1; i++) {
        if (src[i] == CHAR_e_ACUTE) {
            dest[n++] = 0xE9;
            continue;
        }
        if (src[i] == CHAR_E_ACUTE) {
            dest[n++] = 0xC9;
            continue;
        }
        char c = '\0';
        CharCode_ToAsciiString((charcode_t *)&src[i], &c, 1);
        if (c != '\0') {
            dest[n++] = (unsigned char)c;
        }
    }
    dest[n] = 0xFFFF;
}

static unsigned StatusToGen5(u32 status)
{
    if (status & MON_CONDITION_SLEEP) {
        return GEN5_STATUS_SLEEP;
    }
    if (status & (MON_CONDITION_POISON | MON_CONDITION_TOXIC)) {
        return GEN5_STATUS_POISON;
    }
    if (status & MON_CONDITION_BURN) {
        return GEN5_STATUS_BURN;
    }
    if (status & MON_CONDITION_FREEZE) {
        return GEN5_STATUS_FREEZE;
    }
    if (status & MON_CONDITION_PARALYSIS) {
        return GEN5_STATUS_PARALYSIS;
    }
    return GEN5_STATUS_NONE;
}

// BW2 keeps no sleep counter or toxic flag outside a battle: a Pokémon asleep sleeps a few turns more, and one that
// was badly poisoned stays so if it is still poisoned
static u32 StatusFromGen5(unsigned status, u32 before)
{
    switch (status) {
    case GEN5_STATUS_SLEEP:
        return (before & MON_CONDITION_SLEEP) ? (before & MON_CONDITION_SLEEP) : 3;
    case GEN5_STATUS_POISON:
        return (before & MON_CONDITION_TOXIC) ? MON_CONDITION_TOXIC : MON_CONDITION_POISON;
    case GEN5_STATUS_BURN:
        return MON_CONDITION_BURN;
    case GEN5_STATUS_FREEZE:
        return MON_CONDITION_FREEZE;
    case GEN5_STATUS_PARALYSIS:
        return MON_CONDITION_PARALYSIS;
    default:
        return MON_CONDITION_NONE;
    }
}

static void PokemonToGen5(Pokemon *mon, Gen5Pokemon *dest)
{
    charcode_t name[MON_NAME_LEN + 1];
    charcode_t otName[TRAINER_NAME_LEN + 1];

    memset(dest, 0, sizeof(*dest));
    dest->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    dest->form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    dest->level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    dest->exp = Pokemon_GetValue(mon, MON_DATA_EXPERIENCE, NULL);
    dest->personality = Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL);
    dest->otId = Pokemon_GetValue(mon, MON_DATA_OT_ID, NULL);
    dest->heldItem = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);
    dest->ability = Pokemon_GetValue(mon, MON_DATA_ABILITY, NULL);
    dest->friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL);
    dest->gender = Pokemon_GetGender(mon);
    for (int i = 0; i < 4; i++) {
        dest->moves[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1 + i, NULL);
        dest->pp[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1_PP + i, NULL);
        dest->ppUps[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1_PP_UPS + i, NULL);
    }
    dest->hp = Pokemon_GetValue(mon, MON_DATA_HP, NULL);
    dest->status = StatusToGen5(Pokemon_GetValue(mon, MON_DATA_STATUS, NULL));
    for (int i = 0; i < 6; i++) {
        dest->ivs[i] = Pokemon_GetValue(mon, MON_DATA_HP_IV + i, NULL);
        dest->evs[i] = Pokemon_GetValue(mon, MON_DATA_HP_EV + i, NULL);
    }
    dest->ball = Pokemon_GetValue(mon, MON_DATA_POKEBALL, NULL);
    dest->pokerus = Pokemon_GetValue(mon, MON_DATA_POKERUS, NULL);
    dest->metLevel = Pokemon_GetValue(mon, MON_DATA_MET_LEVEL, NULL);
    dest->otGender = Pokemon_GetValue(mon, MON_DATA_OT_GENDER, NULL);
    dest->isEgg = Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL);
    dest->hasNickname = Pokemon_GetValue(mon, MON_DATA_HAS_NICKNAME, NULL);
    Pokemon_GetValue(mon, MON_DATA_NICKNAME, name);
    TextToGen5(name, MON_NAME_LEN + 1, dest->nickname, GEN5_NAME_LENGTH);
    Pokemon_GetValue(mon, MON_DATA_OT_NAME, otName);
    TextToGen5(otName, TRAINER_NAME_LEN + 1, dest->otName, 8);
}

// What the battle changed. TRUE if the Pokémon gained a level (for evolving after the battle)
static BOOL PokemonFromGen5(Pokemon *mon, const Gen5Pokemon *src)
{
    const u32 level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    const u32 status = Pokemon_GetValue(mon, MON_DATA_STATUS, NULL);
    u32 value;

    Pokemon_SetValue(mon, MON_DATA_EXPERIENCE, &src->exp);
    for (int i = 0; i < 4; i++) {
        Pokemon_SetValue(mon, MON_DATA_MOVE1 + i, &src->moves[i]);
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP_UPS + i, &src->ppUps[i]);
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP + i, &src->pp[i]);
    }
    for (int i = 0; i < 6; i++) {
        value = src->evs[i];
        Pokemon_SetValue(mon, MON_DATA_HP_EV + i, &value);
    }
    Pokemon_SetValue(mon, MON_DATA_FRIENDSHIP, &src->friendship);
    Pokemon_SetValue(mon, MON_DATA_HELD_ITEM, &src->heldItem);
    Pokemon_SetValue(mon, MON_DATA_POKERUS, &src->pokerus);
    Pokemon_CalcLevelAndStats(mon);
    Pokemon_SetValue(mon, MON_DATA_HP, &src->hp);
    value = StatusFromGen5(src->status, status);
    Pokemon_SetValue(mon, MON_DATA_STATUS, &value);
    return Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL) > level;
}

static BOOL Init(ApplicationManager *appMan, int *state)
{
    (void)state;
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);
    Party *party = dto->parties[BATTLER_PLAYER_1];
    TrainerInfo *trainerInfo = dto->trainerInfo[BATTLER_PLAYER_1];

    memset(&sBattle, 0, sizeof(sBattle));
    TextToGen5(TrainerInfo_Name(trainerInfo), TRAINER_NAME_LEN + 1, sBattle.player.name, 8);
    sBattle.player.id = TrainerInfo_ID(trainerInfo);
    sBattle.player.gender = TrainerInfo_Gender(trainerInfo);
    sBattle.player.money = TrainerInfo_Money(trainerInfo);
    sBattle.player.badges = TrainerInfo_BadgeCount(trainerInfo);
    sBattle.partyCount = Party_GetCurrentCount(party);
    if (sBattle.partyCount > GEN5_PARTY_MAX) {
        sBattle.partyCount = GEN5_PARTY_MAX;
    }
    for (int i = 0; i < sBattle.partyCount; i++) {
        PokemonToGen5(Party_GetPokemonBySlotIndex(party, i), &sBattle.party[i]);
    }
    PokemonToGen5(Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0), &sBattle.wild);

    sFailed = !Gen5_BattleBegin(&sBattle, Gen5Rom_Dir());
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
    const enum Gen5BattleResult result = sFailed ? GEN5_BATTLE_FAILED : Gen5_BattleEnd(&sBattle);

    if (!sFailed) {
        // The party as the battle left it, which Platinum copies into the save (FieldBattleDTO_UpdateFieldSystem)
        Party *party = dto->parties[BATTLER_PLAYER_1];
        for (int i = 0; i < sBattle.partyCount; i++) {
            Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);
            if (PokemonFromGen5(mon, &sBattle.party[i])) {
                dto->leveledUpMonsMask |= 1 << i;
            }
            printf("Gen 5: party %d after the battle: species %u, level %u, exp %u, HP %u/%u, status %u\n", i,
                (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL), (unsigned)Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL),
                (unsigned)Pokemon_GetValue(mon, MON_DATA_EXPERIENCE, NULL), (unsigned)Pokemon_GetValue(mon, MON_DATA_HP, NULL),
                (unsigned)Pokemon_GetValue(mon, MON_DATA_MAX_HP, NULL), (unsigned)Pokemon_GetValue(mon, MON_DATA_STATUS, NULL));
        }
        TrainerInfo *trainerInfo = dto->trainerInfo[BATTLER_PLAYER_1];
        if (sBattle.moneyChange > 0) {
            TrainerInfo_GiveMoney(trainerInfo, sBattle.moneyChange);
        } else if (sBattle.moneyChange < 0) {
            TrainerInfo_TakeMoney(trainerInfo, -sBattle.moneyChange);
        }
    }
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

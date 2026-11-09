// Battles in the Gen 5 engine; see include/port/gen5_battle.h
#include "port/gen5_battle.h"

#include <nitro.h>
#include <stdio.h>

#include "constants/battle.h"
#include "constants/charcode.h"
#include "constants/heap.h"
#include "constants/items.h"
#include "constants/species.h"
#include "constants/string.h"
#include "generated/game_records.h"

#include "applications/naming_screen.h"
#include "battle/battle_system.h"
#include "bag.h"
#include "evolution.h"
#include "field_battle_data_transfer.h"
#include "game_records.h"
#include "gen5.h"
#include "heap.h"
#include "party.h"
#include "pc_boxes.h"
#include "pokedex.h"
#include "pokemon.h"
#include "poketch.h"
#include "port/gen5_rom.h"
#include "port/sim_config_prj.h"
#include "port/test_script.h"
#include "rtc.h"
#include "string_gf.h"
#include "system.h"
#include "trainer_info.h"
#include "tv_segment.h"

#include "charcode_convert.h"

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

BOOL Gen5Battle_Handles(u32 battleType, u32 statusMask, int wildCount)
{
    // What the bridge doesn't carry over yet (milestone 6): BW2 would let the player run, or record the battle
    const u32 platinumOnly = BATTLE_STATUS_FIRST_BATTLE | BATTLE_STATUS_NO_RUNNING | BATTLE_STATUS_LEGENDARY
        | BATTLE_STATUS_RECORDING | BATTLE_STATUS_RECORDED | BATTLE_STATUS_GIRATINA | BATTLE_STATUS_DISTORTION;
    return SIM_Config_prj_GetConfig()->gen5Battles && Gen5Rom_GameName() != NULL && battleType == BATTLE_TYPE_WILD_MON
        && (statusMask & platinumOnly) == 0 && wildCount == 1;
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

// --- Into the Gen 5 battle -----------------------------------------------------------------------------------------

// One of the game's characters as Unicode: the Latin letters and what Western names use
static u16 CharToUnicode(charcode_t c)
{
    switch (c) {
    case CHAR_e_ACUTE:
        return 0xe9;
    case CHAR_MALE:
        return 0x2642;
    case CHAR_FEMALE:
        return 0x2640;
    }
    char ascii = '\0';
    CharCode_ToAsciiString(&c, &ascii, 1);
    return ascii != '\0' ? (u16)(unsigned char)ascii : '?';
}

// A name in the game's characters (ended by CHAR_EOS) as UTF-16 ended by 0xffff, at most length units with the end
static void NameToUnicode(const charcode_t *name, u16 *out, int length)
{
    int i = 0;
    for (; i < length - 1 && name[i] != CHAR_EOS; i++) {
        out[i] = CharToUnicode(name[i]);
    }
    out[i] = 0xffff;
}

static const enum PokemonDataParam sIvParams[GEN5_STAT_COUNT] = {
    MON_DATA_HP_IV, MON_DATA_ATK_IV, MON_DATA_DEF_IV, MON_DATA_SPEED_IV, MON_DATA_SPATK_IV, MON_DATA_SPDEF_IV,
};

static const enum PokemonDataParam sEvParams[GEN5_STAT_COUNT] = {
    MON_DATA_HP_EV, MON_DATA_ATK_EV, MON_DATA_DEF_EV, MON_DATA_SPEED_EV, MON_DATA_SPATK_EV, MON_DATA_SPDEF_EV,
};

static void MonToGen5(Pokemon *mon, Gen5Mon *out)
{
    memset(out, 0, sizeof(*out));
    out->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    out->form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    out->isEgg = Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL);
    out->personality = Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL);
    out->otId = Pokemon_GetValue(mon, MON_DATA_OT_ID, NULL);
    out->exp = Pokemon_GetValue(mon, MON_DATA_EXPERIENCE, NULL);
    out->level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    out->heldItem = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);
    out->ability = Pokemon_GetValue(mon, MON_DATA_ABILITY, NULL);
    out->nature = out->personality % 25;
    out->friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL);
    for (int i = 0; i < 4; i++) {
        out->moves[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1 + i, NULL);
        out->pp[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1_PP + i, NULL);
        out->ppUps[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1_PP_UPS + i, NULL);
    }
    for (int i = 0; i < GEN5_STAT_COUNT; i++) {
        out->ivs[i] = Pokemon_GetValue(mon, sIvParams[i], NULL);
        out->evs[i] = Pokemon_GetValue(mon, sEvParams[i], NULL);
    }
    out->hp = Pokemon_GetValue(mon, MON_DATA_HP, NULL);
    out->status = Pokemon_GetValue(mon, MON_DATA_STATUS, NULL);
    out->ball = Pokemon_GetValue(mon, MON_DATA_POKEBALL, NULL);
    out->metLevel = Pokemon_GetValue(mon, MON_DATA_MET_LEVEL, NULL);
    out->originGame = Pokemon_GetValue(mon, MON_DATA_MET_GAME, NULL);
    out->language = Pokemon_GetValue(mon, MON_DATA_LANGUAGE, NULL);
    out->otGender = Pokemon_GetValue(mon, MON_DATA_OT_GENDER, NULL);
    out->pokerus = Pokemon_GetValue(mon, MON_DATA_POKERUS, NULL);
    out->fatefulEncounter = Pokemon_GetValue(mon, MON_DATA_FATEFUL_ENCOUNTER, NULL);
    out->hasNickname = Pokemon_GetValue(mon, MON_DATA_HAS_NICKNAME, NULL);
    charcode_t name[MON_NAME_LEN + 1];
    Pokemon_GetValue(mon, MON_DATA_NICKNAME, name);
    name[MON_NAME_LEN] = CHAR_EOS;
    NameToUnicode(name, out->nickname, GEN5_NICKNAME_LENGTH);
    charcode_t otName[TRAINER_NAME_LEN + 1];
    Pokemon_GetValue(mon, MON_DATA_OT_NAME, otName);
    otName[TRAINER_NAME_LEN] = CHAR_EOS;
    NameToUnicode(otName, out->otName, GEN5_NAME_LENGTH);
}

// The bag's pockets a battle uses: not key items, TMs or mail
static void BagToGen5(Bag *bag, Gen5BattleSetup *setup)
{
    const struct {
        BagItem *items;
        int size;
    } battlePockets[] = {
        { bag->items, ITEM_POCKET_SIZE },
        { bag->medicine, MEDICINE_POCKET_SIZE },
        { bag->pokeballs, POKEBALL_POCKET_SIZE },
        { bag->berries, BERRY_POCKET_SIZE },
        { bag->battleItems, BATTLE_ITEM_POCKET_SIZE },
    };
    setup->itemCount = 0;
    for (int p = 0; p < (int)NELEMS(battlePockets); p++) {
        for (int i = 0; i < battlePockets[p].size && setup->itemCount < GEN5_ITEMS_MAX; i++) {
            const BagItem *slot = &battlePockets[p].items[i];
            if (slot->item != ITEM_NONE && slot->quantity > 0) {
                setup->items[setup->itemCount].item = slot->item;
                setup->items[setup->itemCount].count = slot->quantity;
                setup->itemCount++;
            }
        }
    }
}

// The setup is large: static
static Gen5BattleSetup sSetup;
static Gen5BattleResults sResults;

static void DTOToGen5(FieldBattleDTO *dto, Gen5BattleSetup *setup)
{
    memset(setup, 0, sizeof(*setup));
    Party *party = dto->parties[BATTLER_PLAYER_1];
    setup->partyCount = Party_GetCurrentCount(party);
    for (int i = 0; i < setup->partyCount; i++) {
        MonToGen5(Party_GetPokemonBySlotIndex(party, i), &setup->party[i]);
    }
    MonToGen5(Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0), &setup->wild);
    TrainerInfo *trainer = dto->trainerInfo[BATTLER_PLAYER_1];
    NameToUnicode(TrainerInfo_Name(trainer), setup->playerName, GEN5_NAME_LENGTH);
    setup->playerId = TrainerInfo_ID(trainer);
    setup->playerGender = TrainerInfo_Gender(trainer);
    setup->badges = TrainerInfo_BadgeCount(trainer);
    setup->money = TrainerInfo_Money(trainer);
    BagToGen5(dto->bag, setup);
    RTCTime time;
    RTC_GetCurrentTime(&time);
    setup->hour = time.hour;
    setup->minute = time.minute;
}

// --- Out of it -----------------------------------------------------------------------------------------------------

// What the battle changed in a Pokémon of the party; TRUE if it gained a level
static BOOL MonFromGen5(Pokemon *mon, const Gen5Mon *in)
{
    const int level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    u32 value = in->exp;
    Pokemon_SetValue(mon, MON_DATA_EXPERIENCE, &value);
    for (int i = 0; i < 4; i++) {
        value = in->moves[i];
        Pokemon_SetValue(mon, MON_DATA_MOVE1 + i, &value);
        value = in->ppUps[i];
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP_UPS + i, &value);
        value = in->pp[i];
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP + i, &value);
    }
    for (int i = 0; i < GEN5_STAT_COUNT; i++) {
        value = in->evs[i];
        Pokemon_SetValue(mon, sEvParams[i], &value);
    }
    value = in->friendship;
    Pokemon_SetValue(mon, MON_DATA_FRIENDSHIP, &value);
    value = in->heldItem;
    Pokemon_SetValue(mon, MON_DATA_HELD_ITEM, &value);
    value = in->pokerus;
    Pokemon_SetValue(mon, MON_DATA_POKERUS, &value);
    // The level and stats from the experience, then the HP and status it ended with
    Pokemon_CalcLevelAndStats(mon);
    value = in->hp;
    const u32 maxHp = Pokemon_GetValue(mon, MON_DATA_MAX_HP, NULL);
    if (value > maxHp) {
        value = maxHp;
    }
    Pokemon_SetValue(mon, MON_DATA_HP, &value);
    value = in->status;
    Pokemon_SetValue(mon, MON_DATA_STATUS, &value);
    return Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL) > level;
}

// The bag's counts after the battle: what was used, and what was found (Pickup, Thief...)
static void BagFromGen5(Bag *bag, const Gen5BattleResults *results)
{
    for (int i = 0; i < results->itemCount; i++) {
        const u16 item = results->items[i].item;
        const int before = Bag_GetItemQuantity(bag, item, HEAP_ID_FIELD2);
        const int after = results->items[i].count;
        if (after < before) {
            Bag_TryRemoveItem(bag, item, before - after, HEAP_ID_FIELD2);
        } else if (after > before) {
            Bag_TryAddItem(bag, item, after - before, HEAP_ID_FIELD2);
        }
    }
}

// The caught Pokémon (the wild one in the DTO) as it ended the battle, with its catch data
static void UpdateCaughtMon(FieldBattleDTO *dto, const Gen5Mon *caught)
{
    Pokemon *mon = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0);
    u32 value = caught->hp;
    Pokemon_SetValue(mon, MON_DATA_HP, &value);
    value = caught->status;
    Pokemon_SetValue(mon, MON_DATA_STATUS, &value);
    Pokemon_SetCatchData(mon, dto->trainerInfo[BATTLER_PLAYER_1], caught->ball, dto->mapHeaderID, dto->terrain,
        HEAP_ID_FIELD2);
}

static void NameCaughtMon(FieldBattleDTO *dto, String *name)
{
    Pokemon_SetValue(Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0), MON_DATA_NICKNAME_STRING_AND_FLAG,
        name);
    GameRecords_IncrementRecordValue(dto->records, RECORD_POKEMON_NICKNAMED);
}

// The caught Pokémon, named, kept as Platinum's battle keeps it (battle_script.c's catch sequence): the Pokédex,
// then the party, or the first box with room
static void KeepCaughtMon(FieldBattleDTO *dto)
{
    Pokemon *mon = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0);
    Pokedex_Capture(dto->pokedex, mon);
    if (dto->poketch != NULL) {
        Poketch_PokemonHistoryEnqueue(dto->poketch, Pokemon_GetBoxPokemon(mon));
    }
    if (dto->captureAttempt != NULL) {
        CaptureAttempt_Init(dto->captureAttempt, mon, BATTLE_RESULT_CAPTURED_MON, 1, HEAP_ID_FIELD2);
    }
    GameRecords_IncrementRecordValue(dto->records, RECORD_CAUGHT_POKEMON);
    dto->caughtBattlerIdx = BATTLER_ENEMY_1;

    if (Party_AddPokemon(dto->parties[BATTLER_PLAYER_1], mon)) {
        return;
    }
    for (int i = 0; i < LEARNED_MOVES_MAX; i++) {
        int pp = Pokemon_GetValue(mon, MON_DATA_MOVE1_MAX_PP + i, NULL);
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP + i, &pp);
    }
    Pokemon_SetGiratinaFormByHeldItem(mon);
    const u32 box = PCBoxes_FirstEmptyBox(dto->pcBoxes);
    PCBoxes_SetCurrentBox(dto->pcBoxes, box);
    PCBoxes_TryStoreBoxMonInBox(dto->pcBoxes, box, Pokemon_GetBoxPokemon(mon));
}

static void ResultsToDTO(FieldBattleDTO *dto, const Gen5BattleResults *results)
{
    switch (results->result) {
    case GEN5_BATTLE_WON:
        dto->resultMask = BATTLE_RESULT_WIN;
        break;
    case GEN5_BATTLE_LOST:
        dto->resultMask = BATTLE_RESULT_LOSE;
        break;
    case GEN5_BATTLE_CAUGHT:
        dto->resultMask = BATTLE_RESULT_CAPTURED_MON;
        break;
    case GEN5_BATTLE_RAN:
        dto->resultMask = BATTLE_RESULT_PLAYER_FLED;
        break;
    default:
        // It couldn't run: back to the overworld as if the player ran, with nothing changed
        dto->resultMask = BATTLE_RESULT_PLAYER_FLED;
        return;
    }
    Party *party = dto->parties[BATTLER_PLAYER_1];
    dto->leveledUpMonsMask = 0;
    for (int i = 0; i < results->partyCount && i < Party_GetCurrentCount(party); i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);
        // BW2 keeps the party's order; a mismatch would mean it didn't
        if (Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL) != results->party[i].personality) {
            printf("Gen 5: party slot %d came back as another Pokémon\n", i);
            continue;
        }
        if (MonFromGen5(mon, &results->party[i])) {
            dto->leveledUpMonsMask |= 1 << i;
        }
    }
    BagFromGen5(dto->bag, results);
    TrainerInfo *trainer = dto->trainerInfo[BATTLER_PLAYER_1];
    if (results->moneyChange > 0) {
        TrainerInfo_GiveMoney(trainer, results->moneyChange);
    } else if (results->moneyChange < 0) {
        TrainerInfo_TakeMoney(trainer, -results->moneyChange);
    }
    if (results->result == GEN5_BATTLE_CAUGHT) {
        UpdateCaughtMon(dto, &results->wild);
    }
}

// --- The application -----------------------------------------------------------------------------------------------

enum {
    STATE_BATTLE,
    STATE_NICKNAME,
    STATE_WAIT_NICKNAME,
    STATE_CHECK_EVOLUTION,
    STATE_WAIT_EVOLUTION,
    STATE_DONE,
};

static ApplicationManager *sSubApp;
static NamingScreenArgs *sNamingArgs;
static EvolutionData *sEvolution;

static void VBlank(void *data)
{
    (void)data;
    Gen5_BattleVBlank();
}

static BOOL Init(ApplicationManager *appMan, int *state)
{
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);
    DTOToGen5(dto, &sSetup);
    // Seen, as Platinum's battle marks it when the wild Pokémon appears
    Pokedex_Encounter(dto->pokedex, Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0));
    sFailed = !Gen5_BattleBegin(&sSetup, Gen5Rom_Dir());
    if (sFailed) {
        printf("Gen 5: the battle couldn't start\n");
    } else {
        SetVBlankCallback(VBlank, NULL);
    }
    *state = STATE_BATTLE;
    return TRUE;
}

static BOOL Main(ApplicationManager *appMan, int *state)
{
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);
    switch (*state) {
    case STATE_BATTLE:
        if (!sFailed) {
            // The keys and touch the game read this frame (the test scripts' among them), as read, before the
            // button mode
            Gen5_SetInput(gSystem.heldKeysRaw, gSystem.touchHeld, gSystem.touchX, gSystem.touchY);
            const BOOL running = Gen5_BattleFrame();
            if (Gen5_TakeEvents() & GEN5_EVENT_COMMAND) {
                TestScript_OnEvent(TEST_EVENT_BATTLE_COMMAND);
            }
            if (running) {
                return FALSE;
            }
            SetVBlankCallback(NULL, NULL);
            Gen5_BattleEnd(&sResults);
        } else {
            memset(&sResults, 0, sizeof(sResults));
            sResults.result = GEN5_BATTLE_FAILED;
        }
        ResultsToDTO(dto, &sResults);
        *state = sResults.result == GEN5_BATTLE_CAUGHT ? STATE_NICKNAME : STATE_CHECK_EVOLUTION;
        return FALSE;
    case STATE_NICKNAME:
        // Platinum's battle asks first; here the naming screen comes straight away (an empty name keeps the
        // species name)
        sNamingArgs = NamingScreenArgs_Init(HEAP_ID_FIELD2, NAMING_SCREEN_TYPE_POKEMON,
            Pokemon_GetValue(Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0), MON_DATA_SPECIES, NULL),
            MON_NAME_LEN, dto->options);
        sSubApp = ApplicationManager_New(&gNamingScreenAppTemplate, sNamingArgs, HEAP_ID_FIELD2);
        *state = STATE_WAIT_NICKNAME;
        return FALSE;
    case STATE_WAIT_NICKNAME:
        if (!ApplicationManager_Exec(sSubApp)) {
            return FALSE;
        }
        if (sNamingArgs->returnCode == NAMING_SCREEN_CODE_OK) {
            NameCaughtMon(dto, sNamingArgs->textInputStr);
        }
        NamingScreenArgs_Free(sNamingArgs);
        ApplicationManager_Free(sSubApp);
        sSubApp = NULL;
        KeepCaughtMon(dto);
        *state = STATE_CHECK_EVOLUTION;
        return FALSE;
    case STATE_CHECK_EVOLUTION: {
        // As Platinum's battle does after it ends (battle_main.c)
        int slot;
        int evoType;
        const int target = Battle_FindEvolvingPartyMember(dto, &slot, &evoType);
        if (target == SPECIES_NONE) {
            *state = STATE_DONE;
            return FALSE;
        }
        Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_EVOLUTION, HEAP_SIZE_EVOLUTION);
        Pokemon *mon = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_PLAYER_1], slot);
        sEvolution = Evolution_Begin(dto->parties[BATTLER_PLAYER_1], mon, target, dto->options,
            dto->visitedContestHall, dto->pokedex, dto->bag, dto->records, dto->poketch, evoType, 0x1 | 0x2,
            HEAP_ID_EVOLUTION);
        *state = STATE_WAIT_EVOLUTION;
        return FALSE;
    }
    case STATE_WAIT_EVOLUTION:
        if (Evolution_IsDone(sEvolution)) {
            Evolution_Free(sEvolution);
            sEvolution = NULL;
            Heap_Destroy(HEAP_ID_EVOLUTION);
            *state = STATE_CHECK_EVOLUTION;
        }
        return FALSE;
    default:
        return TRUE;
    }
}

static BOOL Exit(ApplicationManager *appMan, int *state)
{
    (void)appMan;
    (void)state;
    return TRUE;
}

const ApplicationManagerTemplate gGen5BattleApplicationTemplate = {
    .init = Init,
    .main = Main,
    .exit = Exit,
    .overlayID = FS_OVERLAY_ID_NONE,
};

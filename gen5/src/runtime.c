// BW2's systems run inside the port, and a battle on them (Gen 5 battles, milestone 3). Platinum's Gen 5 battle
// application (src/port/gen5_battle.c) calls this through gen5/api/gen5.h.

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle/battle_proc.h"
#include "battle/btl_result.h"
#include "battle/btl_setup.h"
#include "constants/sound.h"
#include "field/zone.h"
#include "gfl/g3d.h"
#include "gfl/heap.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/ui.h"
#include "gen5.h"
#include "pml/poke_party.h"
#include "pml/species_names.h"
#include "save/bag.h"
#include "save/config.h"
#include "save/high_link.h"
#include "save/join_avenue.h"
#include "save/player_info.h"
#include "save/pokedex_savedata.h"
#include "save/records.h"
#include "save/save_control.h"
#include "save/trainer_card.h"
#include "save/trainer_game_info.h"
#include "system/brightness.h"
#include "system/gfl_use.h"
#include "system/printsys.h"
#include "system/wipe.h"

// The block GFL's heaps are made in: far more than a DS's 1.7 MB, as pointers and the structures holding them are
// twice as large here
#define MEMORY_SIZE (24 * 1024 * 1024)
#define HEAP_SYSTEM_SIZE 0x20000
#define HEAP_USER_SIZE (20 * 1024 * 1024)
// The battle's heap: what BW2's field allocates the setup from
#define HEAPID_PORT_BATTLE 2
#define HEAP_PORT_BATTLE_SIZE (512 * 1024)
#define VRAM_TRANSFER_TASKS 48

// The objects of BW2's game data the battle reads (gen5/src/game_data.c)
void Gen5_SetBattleGameData(PokeParty *party, PlayerInfo *info, Config *config, BagSave *bag, BagCursor *bagCursor,
    PokeDexSave *pokedex, GameRecords *records);
GameData *Gen5_GetGameData(void);

static void *sMemory;
static BOOL sStarted;
static NNSGfdVramTransferTask sVramTransferTasks[VRAM_TRANSFER_TASKS];
static BtlSetup *sSetup;
static BOOL sRunning;

void Gen5_MemInit(void)
{
    // GFL makes its heaps from the main arena (GFL_HeapMgrInit), which is Platinum's: it gets a block of the port's
    // own for the time, and Platinum's arena back after
    void *lo = OS_GetArenaLo(OS_ARENA_MAIN);
    void *hi = OS_GetArenaHi(OS_ARENA_MAIN);
    OS_SetArenaLo(OS_ARENA_MAIN, sMemory);
    OS_SetArenaHi(OS_ARENA_MAIN, (u8 *)sMemory + MEMORY_SIZE);

    static const HeapDef heaps[] = {
        { HEAP_SYSTEM_SIZE, 0 },
        { HEAP_USER_SIZE, 0 },
    };
    GFL_MemInit(heaps, 2, 193, 0);
    GFL_HeapCreateChild(HEAPID_USER, 7, 0x27000 * 2);
    GFL_HeapCreateChild(HEAPID_USER, 9, 0x3f00 * 2);

    OS_SetArenaLo(OS_ARENA_MAIN, lo);
    OS_SetArenaHi(OS_ARENA_MAIN, hi);
}

// BW2's main and GameInit, as far as the battle needs them
// Pokémon cries, silent for now: no wave, which PokeVoice never plays. BW2's sound (its sound data, music, sound
// effects and cries) isn't run yet: Platinum's sound system has NitroSystem's sound library (sound.c)
static void VoiceResID(u32 species, u32 form, u32 *waveArc)
{
    (void)species;
    (void)form;
    *waveArc = 0;
}

static BOOL VoiceWaveInfo(u32 species, u32 form, const PokeVoiceChatterInfo *chatterInfo, void **samples, u32 *size,
    u32 *sampleRate, u32 *speed, s8 *volume)
{
    (void)species;
    (void)form;
    (void)chatterInfo;
    (void)samples;
    (void)sampleRate;
    (void)speed;
    (void)volume;
    *size = 0;
    return TRUE;
}

static BOOL Start(const char *gen5Dir)
{
    if (sStarted) {
        return TRUE;
    }
    sMemory = calloc(1, MEMORY_SIZE);
    if (sMemory == NULL) {
        return FALSE;
    }
    GFLAppInit();
    if (!Gen5_FilesInit(gen5Dir)) {
        return FALSE;
    }
    GFL_TextRndInit(HEAPID_SYSTEM);
    PML_SpeciesNamesResidentInit(HEAPID_SYSTEM);
    PML_PersonalInit(HEAPID_SYSTEM);
    // The zones' tables: the battle's lights and seasons look the zone up
    InitZoneDataSystem(HEAPID_SYSTEM);
    PokeVoice_Init(1, VoiceResID, VoiceWaveInfo);
    // No Pass Powers (C-Gear boosts) on: zeroed, they would scale experience and prize money to nothing
    PassPower_Init();
    // Join Avenue's blocks of the save, which a catch records an activity in (as BW2's GameInit)
    setImportantJABlockAddresses(SaveControl_GetInstance());
    sStarted = TRUE;
    printf("Gen 5: BW2's systems are up\n");
    return TRUE;
}

// The battle's own blocks in its heap, freed by Gen5_BattleEnd before the heap is deleted (BW2 checks that a deleted
// heap is empty)
#define BATTLE_BLOCKS_MAX 16
static void *sBattleBlocks[BATTLE_BLOCKS_MAX];
static int sBattleBlockCount;

static void *Keep(void *block)
{
    if (sBattleBlockCount < BATTLE_BLOCKS_MAX) {
        sBattleBlocks[sBattleBlockCount++] = block;
    }
    return block;
}

// gen5/src/bridge.c
PartyPkm *Gen5_NewPkm(const Gen5Mon *mon, HeapID heapId);
void Gen5_ReadPkm(PartyPkm *pkm, Gen5Mon *mon);

// What the battle was given, for its results
static Gen5BattleSetup sGiven;

static PokeParty *Party(const Gen5Mon *mons, int count)
{
    PokeParty *party = Keep(PokeParty_Create(HEAPID_PORT_BATTLE));
    for (int i = 0; i < count; i++) {
        PartyPkm *pkm = Gen5_NewPkm(&mons[i], HEAPID_PORT_BATTLE);
        PokeParty_AddPkm(party, pkm);
        GFL_HeapFree(pkm);
    }
    return party;
}

// The player's cash and badges where the battle reads them (the trainer block of the port's save)
static void SetTrainer(const Gen5BattleSetup *setup)
{
    TrainerGameInfoSave *trainer = getTrainerGameInfoAddress(SaveControl_GetInstance());
    subCashFromTotal(trainer, getCash(trainer));
    addCashToTotal(trainer, setup->money);
    for (int i = 0; i < setup->badges && i < 8; i++) {
        addBadge((TrainerCardSave *)trainer, i);
    }
}

int Gen5_BattleBegin(const Gen5BattleSetup *given, const char *gen5Dir)
{
    if (sRunning || given->partyCount < 1 || !Start(gen5Dir)) {
        return FALSE;
    }
    sGiven = *given;
    GFL_HeapCreateChild(HEAPID_USER, HEAPID_PORT_BATTLE, HEAP_PORT_BATTLE_SIZE);
    sBattleBlockCount = 0;

    // What BW2's field would take from its game data (BtlSetup_LoadGameData), from Platinum's
    PlayerInfo *info = Keep(PlayerInfo_Create(HEAPID_PORT_BATTLE));
    copyTrainerName(info, given->playerName);
    setIDAsUInt(info, given->playerId);
    setTrainerGender(info, given->playerGender);
    Config *config = Keep(Config_Create(HEAPID_PORT_BATTLE));
    BagSave *bag = Keep(GFL_HeapAllocate(HEAPID_PORT_BATTLE, BagSave_GetBlockSize(), TRUE, __FILE__, __LINE__));
    BagSave_Init(bag);
    for (int i = 0; i < given->itemCount; i++) {
        if (!BagSave_AddItem(bag, given->items[i].item, given->items[i].count, HEAPID_PORT_BATTLE)) {
            printf("Gen 5: item %d isn't in BW2's bag\n", given->items[i].item);
        }
    }
    BagCursor *bagCursor = Keep(BagCursor_Create(HEAPID_PORT_BATTLE));
    PokeDexSave *pokedex =
        Keep(GFL_HeapAllocate(HEAPID_PORT_BATTLE, getSizeofPokedexData(), TRUE, __FILE__, __LINE__));
    initPokedex(pokedex);
    GameRecords *records =
        Keep(GFL_HeapAllocate(HEAPID_PORT_BATTLE, GameRecords_GetSize(), TRUE, __FILE__, __LINE__));
    PokeParty *party = Party(given->party, given->partyCount);
    Gen5_SetBattleGameData(party, info, config, bag, bagCursor, pokedex, records);
    SetTrainer(given);

    sSetup = Keep(BtlSetup_Create(HEAPID_PORT_BATTLE));
    BtlSetup_Reset(sSetup);
    sSetup->battleType = 0;
    sSetup->battleStyle = 0;
    sSetup->netClientMask = 0xffff;
    sSetup->gameData = Gen5_GetGameData();
    sSetup->playerInfo[0] = info;
    sSetup->config = config;
    sSetup->bag = bag;
    sSetup->bagCursor = bagCursor;
    sSetup->pokedex = pokedex;
    sSetup->records = records;
    sSetup->badgeCount = given->badges;
    sSetup->shooterSettings.launcher = 1;
    sSetup->fieldSituation.bgm = SEQ_BGM_VS_NORAPOKE;
    sSetup->fieldSituation.winBgm = SEQ_BGM_WIN1;
    sSetup->fieldSituation.env.hour = given->hour;
    sSetup->fieldSituation.env.minute = given->minute;
    sSetup->result = BTL_RESULT_VICTORY;
    sSetup->party[0] = Keep(PokeParty_Create(HEAPID_PORT_BATTLE));
    PokeParty_Copy(party, sSetup->party[0]);
    sSetup->party[1] = Party(&given->wild, 1);

    printf("Gen 5: a wild battle, %d Pokémon (species %d first, level %d) against species %d (level %d)\n",
        given->partyCount, given->party[0].species, given->party[0].level, given->wild.species, given->wild.level);
    NNS_GfdInitVramTransferManager(sVramTransferTasks, VRAM_TRANSFER_TASKS);
    GCTX_ProcMgrQueueProc(OVERLAY_NONE, &BATTLE_PROC_FUNCTIONS, sSetup);
    sRunning = TRUE;
    return TRUE;
}

static struct {
    u16 keys;
    BOOL touch;
    u16 x;
    u16 y;
} sInput;

void Gen5_SetInput(unsigned keys, int touch, int x, int y)
{
    sInput.keys = (u16)keys;
    sInput.touch = touch;
    sInput.x = (u16)x;
    sInput.y = (u16)y;
}

u16 Gen5_PadRead(void)
{
    return sInput.keys;
}

void Gen5_TouchRead(TPData *touch)
{
    touch->x = sInput.x;
    touch->y = sInput.y;
    touch->touch = sInput.touch ? TP_TOUCH_ON : TP_TOUCH_OFF;
    touch->validity = TP_VALIDITY_VALID;
}

static u32 sEvents;

void Gen5_Event(u32 event)
{
    sEvents |= event;
}

unsigned Gen5_TakeEvents(void)
{
    const u32 events = sEvents;
    sEvents = 0;
    return events;
}

int Gen5_BattleFrame(void)
{
    if (!sRunning) {
        return FALSE;
    }
    // BW2's main loop: GFLAppUpdate and GameUpdate, then the 3D's swap
    GCTX_HIDUpdate();
    const u32 state = GCTX_ProcMgrUpdate();
    BrightnessController_Update();
    GFL_WipeMain();
    Gen5_SoundUpdate();
    PokeVoice_Update();
    GFL_G3DSysCheckSwapBuffers();
    if (state == PROCMGR_DONE) {
        sRunning = FALSE;
    }
    return sRunning;
}

void Gen5_BattleVBlank(void)
{
    // BW2's VBlank interrupt and the work after it (GFLAppIRQVBlank, GameUpdateVBlank). libntr's VCOUNT stays 0; on
    // the DS it reads 192 when the VBlank starts, and BW2's VBlank tasks check it (the Pokémon sprites' loads wait for
    // lines 192 to 200)
    const u16 vcount = reg_GX_VCOUNT;
    reg_GX_VCOUNT = 192;
    GFL_VBlankExecCallback();
    NNS_GfdDoVramTransfer();
    GFL_VBlankUpdate();
    reg_GX_VCOUNT = vcount;
}

// The battle's Pokémon in party, read back into the given ones they came from (BW2 keeps the party's order)
static int ReadParty(PokeParty *party, const Gen5Mon *given, int count, Gen5Mon *out)
{
    const int n = PokeParty_GetPkmCount(party);
    for (int i = 0; i < count; i++) {
        out[i] = given[i];
        if (i < n) {
            Gen5_ReadPkm(PokeParty_GetPkm(party, i), &out[i]);
        }
    }
    return count;
}

static enum Gen5BattleResult Result(u32 result)
{
    switch (result) {
    case BTL_RESULT_VICTORY:
        return GEN5_BATTLE_WON;
    case BTL_RESULT_CAPTURE:
        return GEN5_BATTLE_CAUGHT;
    case BTL_RESULT_FLEE:
    case BTL_RESULT_FLEE_FOE:
        return GEN5_BATTLE_RAN;
    default:
        return GEN5_BATTLE_LOST;
    }
}

void Gen5_BattleEnd(Gen5BattleResults *results)
{
    memset(results, 0, sizeof(*results));
    if (sSetup == NULL) {
        results->result = GEN5_BATTLE_FAILED;
        return;
    }
    printf("Gen 5: the battle ended (BW2's result %lu)\n", (unsigned long)sSetup->result);
    results->result = Result(sSetup->result);
    results->partyCount = ReadParty(sSetup->party[0], sGiven.party, sGiven.partyCount, results->party);
    const u32 wildSlot = sSetup->capturedSlot < PokeParty_GetPkmCount(sSetup->party[1]) ? sSetup->capturedSlot : 0;
    results->wild = sGiven.wild;
    Gen5_ReadPkm(PokeParty_GetPkm(sSetup->party[1], wildSlot), &results->wild);
    results->itemCount = sGiven.itemCount;
    for (int i = 0; i < sGiven.itemCount; i++) {
        results->items[i].item = sGiven.items[i].item;
        results->items[i].count = BagSave_GetItemCountByID(sSetup->bag, sGiven.items[i].item, HEAPID_PORT_BATTLE);
    }
    results->moneyChange = (s32)sSetup->moneyChange;

    sSetup = NULL;
    while (sBattleBlockCount > 0) {
        GFL_HeapFree(sBattleBlocks[--sBattleBlockCount]);
    }
    GFL_HeapDelete(HEAPID_PORT_BATTLE);
}

// BW2's systems run inside the port, and a battle on them (Gen 5 battles, milestone 3). Platinum's Gen 5 battle
// application (src/port/gen5_battle.c) calls this through gen5/api/gen5.h.

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/p_status.h"
#include "battle/battle_proc.h"
#include "battle/btl_result.h"
#include "battle/btl_setup.h"
#include "constants/battle.h"
#include "constants/pokemon.h"
#include "constants/sound.h"
#include "constants/version.h"
#include "field/zone.h"
#include "gfl/g3d.h"
#include "gfl/heap.h"
#include "gfl/str.h"
#include "gfl/proc.h"
#include "gfl/sound.h"
#include "gfl/ui.h"
#include "gen5.h"
#include "gen5/player_back.h"
#include "pml/poke_party.h"
#include "pml/species_names.h"
#include "save/bag.h"
#include "save/config.h"
#include "save/high_link.h"
#include "save/player_info.h"
#include "save/pokedex_savedata.h"
#include "save/records.h"
#include "save/save_control.h"
#include "save/trainer_data_savedata.h"
#include "save/trainer_game_info.h"
#include "system/brightness.h"
#include "system/gfl_use.h"
#include "system/printsys.h"
#include "system/wipe.h"

// Platinum's trainer classes as BW2's (trainers.c)
u32 Gen5_TrainerClass(u32 platinumClass);
u32 Gen5_TrainerAIFlags(u32 platinumFlags);

// Platinum's trainers go into BW2's battle with IDs from here: BW2 then takes their win and loss lines from the setup
// (BtlString_LoadTrainerWinLoseMsg), and its data reads stay in the Battle Institute's range (TrainerData_GetParam)
#define PORT_TRAINER_ID_START 0x800

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
static BagSave *sBag;
static BOOL sItemInBag[GEN5_ITEMS_MAX];
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
    // BW2's sound effects and cries (sound.c). Without its sound archive the battle is silent but runs
    if (!Gen5_SoundInit()) {
        printf("Gen 5: no sound: BW2's sound data didn't open\n");
    }
    // No Pass Power on. Its "none" isn't 0: left zeroed, every power is on at 0 %, and a battle gives no experience
    // or prize money and no Poké Ball catches
    PassPower_Init();
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

// A Platinum Pokémon in BW2's format (Gen5Pokemon has what both keep). The PID is the encryption key, so it is
// given at creation; the rest is set field by field, then the stats are computed from it
static PartyPkm *NewPkm(const Gen5Pokemon *src)
{
    PartyPkm *pkm = PokeParty_NewPkm(src->species, src->level, src->otId, 0, src->personality, HEAPID_PORT_BATTLE);

    PokeParty_SetParam(pkm, PKM_PARAM_FORM, src->form);
    PokeParty_SetParam(pkm, PKM_PARAM_EXP, src->exp);
    PokeParty_SetParam(pkm, PKM_PARAM_ITEM, src->heldItem);
    PokeParty_SetParam(pkm, PKM_PARAM_ABILITY, src->ability);
    PokeParty_SetParam(pkm, PKM_PARAM_HIDDEN_ABILITY, 0);
    PokeParty_SetParam(pkm, PKM_PARAM_HAPPINESS, src->friendship);
    PokeParty_SetParam(pkm, PKM_PARAM_SEX, src->gender);
    // Before Gen 5 the nature is the PID's; Gen 5 keeps it in a field of its own
    PokeParty_SetParam(pkm, PKM_PARAM_NATURE, src->personality % 25);
    // Pokémon from Platinum: their ability slot comes from the PID's lowest bit, as for any Gen 4 Pokémon
    PokeParty_SetParam(pkm, PKM_PARAM_ORIGIN_GAME, VERSION_PLATINUM);
    for (int i = 0; i < 4; i++) {
        PokeParty_SetParam(pkm, PKM_PARAM_MOVE1 + i, src->moves[i]);
        PokeParty_SetParam(pkm, PKM_PARAM_MOVE1_PP_UP + i, src->ppUps[i]);
        PokeParty_SetParam(pkm, PKM_PARAM_MOVE1_PP + i, src->pp[i]);
    }
    for (int i = 0; i < 6; i++) {
        PokeParty_SetParam(pkm, PKM_PARAM_IV_HP + i, src->ivs[i]);
        PokeParty_SetParam(pkm, PKM_PARAM_EV_HP + i, src->evs[i]);
    }
    PokeParty_SetParam(pkm, PKM_PARAM_POKEBALL, src->ball);
    PokeParty_SetParam(pkm, PKM_PARAM_POKERUS, src->pokerus);
    PokeParty_SetParam(pkm, PKM_PARAM_MET_LEVEL, src->metLevel);
    PokeParty_SetParam(pkm, PKM_PARAM_OT_GENDER, src->otGender);
    PokeParty_SetParam(pkm, PKM_PARAM_IS_EGG, src->isEgg);
    PokeParty_SetParam(pkm, PKM_PARAM_OT_NAME_RAW, (uptr)src->otName);
    // A Pokémon without a nickname keeps BW2's name for its species
    if (src->hasNickname) {
        PokeParty_SetParam(pkm, PKM_PARAM_NICKNAME_RAW, (uptr)src->nickname);
    }
    PokeParty_RecalcStats(pkm);
    PokeParty_SetParam(pkm, PKM_PARAM_HP, src->hp);
    PokeParty_SetStatusCond(pkm, src->status);
    return pkm;
}

// What a battle changes in a Pokémon, back into the Gen5Pokemon it was made from
static void ReadPkm(PartyPkm *pkm, Gen5Pokemon *dest)
{
    dest->ball = PokeParty_GetParam(pkm, PKM_PARAM_POKEBALL, NULL);
    dest->form = PokeParty_GetParam(pkm, PKM_PARAM_FORM, NULL);
    dest->exp = PokeParty_GetParam(pkm, PKM_PARAM_EXP, NULL);
    dest->level = PokeParty_GetParam(pkm, PKM_PARAM_LEVEL, NULL);
    dest->heldItem = PokeParty_GetParam(pkm, PKM_PARAM_ITEM, NULL);
    dest->friendship = PokeParty_GetParam(pkm, PKM_PARAM_HAPPINESS, NULL);
    for (int i = 0; i < 4; i++) {
        dest->moves[i] = PokeParty_GetParam(pkm, PKM_PARAM_MOVE1 + i, NULL);
        dest->pp[i] = PokeParty_GetParam(pkm, PKM_PARAM_MOVE1_PP + i, NULL);
        dest->ppUps[i] = PokeParty_GetParam(pkm, PKM_PARAM_MOVE1_PP_UP + i, NULL);
    }
    for (int i = 0; i < 6; i++) {
        dest->evs[i] = PokeParty_GetParam(pkm, PKM_PARAM_EV_HP + i, NULL);
    }
    dest->pokerus = PokeParty_GetParam(pkm, PKM_PARAM_POKERUS, NULL);
    dest->hp = PokeParty_GetParam(pkm, PKM_PARAM_HP, NULL);
    dest->status = GetStatusCond(pkm);
}

static PokeParty *NewParty(const Gen5Pokemon *mons, int count)
{
    PokeParty *party = Keep(PokeParty_Create(HEAPID_PORT_BATTLE));
    for (int i = 0; i < count; i++) {
        PartyPkm *pkm = NewPkm(&mons[i]);
        PokeParty_AddPkm(party, pkm);
        GFL_HeapFree(pkm);
    }
    return party;
}

// The trainers of the battle by BW2 client (1 and 3 the opponents, 2 the partner): their prize and their defeat line
static const Gen5Trainer *sTrainers[4];

// A Platinum trainer as BW2 client `client`, with its party, and its art in art slot `artSlot` (-1: none)
static void SetupTrainer(u32 client, const Gen5Trainer *src, int artSlot)
{
    BtlSetupTrainer *trainer = Keep(TrainerBattleSetup_Create(HEAPID_PORT_BATTLE));
    Keep(trainer->name);
    trainer->trainerId = PORT_TRAINER_ID_START + src->id;
    trainer->trainerClass = Gen5_TrainerClass(src->platinumClass);
    trainer->aiFlags = Gen5_TrainerAIFlags(src->aiFlags);
    for (int i = 0; i < 4; i++) {
        trainer->items[i] = src->items[i];
    }
    GFL_StrBufLoadString(trainer->name, src->name);
    if (artSlot >= 0 && src->hasArt) {
        Gen5_SetTrainerArt(artSlot, trainer->trainerClass, src);
    }
    sSetup->trainers[client] = trainer;
    // Platinum's class name in BW2's messages, in place of the class's
    sSetup->trainerNames[client] = Keep(GFL_StrBufCreate(32, HEAPID_PORT_BATTLE));
    GFL_StrBufLoadString(sSetup->trainerNames[client], src->className);
    sSetup->party[client] = NewParty(src->party, src->partyCount);
    sTrainers[client] = src;
}

static const Gen5Trainer *TrainerById(u32 trainerId)
{
    for (int i = 0; i < 4; i++) {
        if (sTrainers[i] != NULL && PORT_TRAINER_ID_START + sTrainers[i]->id == trainerId) {
            return sTrainers[i];
        }
    }
    return NULL;
}

int Gen5_BattleBegin(const Gen5Battle *battle, const char *gen5Dir)
{
    if (sRunning || !Start(gen5Dir)) {
        return FALSE;
    }
    GFL_HeapCreateChild(HEAPID_USER, HEAPID_PORT_BATTLE, HEAP_PORT_BATTLE_SIZE);
    sBattleBlockCount = 0;

    // What BW2's field would take from its game data (BtlSetup_LoadGameData). The player is Platinum's; the bag,
    // Pokédex and records are empty for now (milestone 4)
    PlayerInfo *info = Keep(PlayerInfo_Create(HEAPID_PORT_BATTLE));
    copyTrainerName(info, battle->player.name);
    setIDAsUInt(info, battle->player.id);
    setTrainerGender(info, battle->player.gender);
    PlayerInfo_SetVersion(info, VERSION_PLATINUM);
    // The money a loss takes is checked against what the player has (CalcLossMoney)
    getTrainerGameInfoAddress(SaveControl_GetInstance())->cash = battle->player.money;
    Config *config = Keep(Config_Create(HEAPID_PORT_BATTLE));
    Config_SetBattleStyle(config, battle->setStyle);
    // The battle's messages take their speed from this config, and the text printing's own waits from the save's
    Config_SetTextSpeed(config, battle->textSpeed);
    Config_SetTextSpeed(&getTrainerDataBlkAddress(SaveControl_GetInstance())->config, battle->textSpeed);
    BagSave *bag = Keep(GFL_HeapAllocate(HEAPID_PORT_BATTLE, BagSave_GetBlockSize(), TRUE, __FILE__, __LINE__));
    BagSave_Init(bag);
    // The player's items the battle can use; one BW2 doesn't have stays out (and comes back as it was)
    for (int i = 0; i < battle->itemCount; i++) {
        sItemInBag[i] = BagSave_AddItem(bag, battle->items[i].item, battle->items[i].count, HEAPID_PORT_BATTLE);
    }
    sBag = bag;
    BagCursor *bagCursor = Keep(BagCursor_Create(HEAPID_PORT_BATTLE));
    PokeDexSave *pokedex =
        Keep(GFL_HeapAllocate(HEAPID_PORT_BATTLE, getSizeofPokedexData(), TRUE, __FILE__, __LINE__));
    initPokedex(pokedex);
    GameRecords *records =
        Keep(GFL_HeapAllocate(HEAPID_PORT_BATTLE, GameRecords_GetSize(), TRUE, __FILE__, __LINE__));
    PokeParty *party = NewParty(battle->party, battle->partyCount);
    Gen5_SetBattleGameData(party, info, config, bag, bagCursor, pokedex, records);

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
    sSetup->shooterSettings.launcher = 1;
    sSetup->fieldSituation.bgm = SEQ_BGM_VS_NORAPOKE;
    sSetup->fieldSituation.winBgm = SEQ_BGM_WIN1;
    sSetup->fieldSituation.env.hour = battle->hour;
    sSetup->fieldSituation.env.minute = battle->minute;
    sSetup->fieldSituation.env.zoneId = battle->zone;
    sSetup->fieldSituation.env.bgType = battle->arenaBg;
    sSetup->fieldSituation.env.terrain = battle->arenaTerrain;
    sSetup->unkDD_6 = battle->arenaAlt;
    sSetup->badgeCount = battle->player.badges;
    sSetup->result = BTL_RESULT_VICTORY;
    sSetup->party[0] = Keep(PokeParty_Create(HEAPID_PORT_BATTLE));
    PokeParty_Copy(party, sSetup->party[0]);
    for (int i = 0; i < 4; i++) {
        sTrainers[i] = NULL;
    }
    Gen5_SetTrainerArt(0, 0, NULL);
    Gen5_SetTrainerArt(1, 0, NULL);
    // Two Pokémon a side: one trainer's two, two trainers' one each (BW2's multi mode 4, the second trainer as client
    // 3), or with a partner (mode 3 against two trainers, 5 in the wild: the partner is client 2)
    sSetup->battleStyle = battle->doubles ? BTL_STYLE_DOUBLE : BTL_STYLE_SINGLE;
    if (battle->isTrainer) {
        SetupTrainer(1, &battle->trainer, 0);
        if (battle->hasTrainer2) {
            SetupTrainer(3, &battle->trainer2, 1);
            sSetup->fieldSituation.multiMode = 4;
        }
        if (battle->hasPartner) {
            SetupTrainer(2, &battle->partner, -1);
            sSetup->fieldSituation.multiMode = battle->hasTrainer2 ? 3 : 5;
        }
        // The first trainer's defeat line, for BW2's code that reads it from here (btl_string.c); the port's patch of
        // btlv_core.c takes each trainer's (Gen5_PortTrainerDefeatText)
        sSetup->trainerLoseMsg = Keep(GFL_StrBufCreate(GEN5_TEXT_LENGTH, HEAPID_PORT_BATTLE));
        GFL_StrBufLoadString(sSetup->trainerLoseMsg, battle->trainer.defeatText);
        // Platinum's trainers say nothing when they win
        sSetup->trainerWinMsg = Keep(GFL_StrBufCreate(4, HEAPID_PORT_BATTLE));
        sSetup->battleType = BTL_TYPE_TRAINER;
        sSetup->fieldSituation.bgm = SEQ_BGM_VS_TRAINER;
        sSetup->fieldSituation.winBgm = SEQ_BGM_WIN2;
        printf("Gen 5: a %s trainer battle against trainer %u (class %u, BW2's %lu), %d Pokémon", battle->doubles ? "double" : "single",
            battle->trainer.id, battle->trainer.platinumClass, (unsigned long)sSetup->trainers[1]->trainerClass,
            battle->trainer.partyCount);
        if (battle->hasTrainer2) {
            printf(", and trainer %u (class %u), %d Pokémon", battle->trainer2.id, battle->trainer2.platinumClass,
                battle->trainer2.partyCount);
        }
        if (battle->hasPartner) {
            printf(", beside trainer %u (class %u), %d Pokémon", battle->partner.id, battle->partner.platinumClass,
                battle->partner.partyCount);
        }
        printf("\n");
    } else {
        Gen5Pokemon wild[2] = { battle->wild, battle->wild2 };
        sSetup->party[1] = NewParty(wild, battle->hasWild2 ? 2 : 1);
        if (battle->hasPartner) {
            SetupTrainer(2, &battle->partner, -1);
            sSetup->fieldSituation.multiMode = 5;
        }
    }

    if (!battle->isTrainer) {
        printf("Gen 5: a wild battle, %d Pokémon (species %u, level %u first) against species %u (level %u)\n",
            battle->partyCount, battle->party[0].species, battle->party[0].level, battle->wild.species,
            battle->wild.level);
    }
    NNS_GfdInitVramTransferManager(sVramTransferTasks, VRAM_TRANSFER_TASKS);
    Gen5_SoundBattleBegin();
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

// Widescreen: how many times as wide as the DS's the 3D is drawn
static int sMoveBgShown;

void Gen5_SetMoveBgShown(int shown)
{
    sMoveBgShown = shown;
}

int Gen5_MoveBgShown(void)
{
    return sMoveBgShown;
}

static float sWideRatio = 1.0f;

fx32 Gen5_Widen(fx32 width)
{
    return sWideRatio == 1.0f ? width : (fx32)((float)width * sWideRatio);
}

void Gen5_SetWidescreen(float ratio)
{
    if (ratio == sWideRatio) {
        return;
    }
    sWideRatio = ratio;
    // The camera's projection again, widened or not (BW2 sets it only when the camera moves). The MCSS sprites'
    // own projection is set each time they are drawn
    Gen5_G3DReproject();
}

int Gen5_BattleFrame(void)
{
    if (!sRunning) {
        return FALSE;
    }
    // BW2's main loop: GFLAppUpdate and GameUpdate, then the 3D's swap. Its sound calls use its own sound archive
    Gen5_SoundEnter();
    GCTX_HIDUpdate();
    const u32 state = GCTX_ProcMgrUpdate();
    BrightnessController_Update();
    GFL_WipeMain();
    Gen5_SoundUpdate();
    PokeVoice_Update();
    GFL_G3DSysCheckSwapBuffers();
    Gen5_SoundLeave();
    if (state == PROCMGR_DONE) {
        sRunning = FALSE;
    }
    return sRunning;
}

static u32 sArtTicks;

u32 Gen5_ArtTicks(void)
{
    return sArtTicks;
}

void Gen5_BattleVBlank(void)
{
    sArtTicks++;
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

enum Gen5BattleResult Gen5_BattleEnd(Gen5Battle *battle)
{
    if (sSetup == NULL) {
        return GEN5_BATTLE_FAILED;
    }
    const u32 result = sSetup->result;
    Gen5_SoundBattleEnd();
    // The party as the battle left it, in its original order
    PokeParty *party = sSetup->party[0];
    for (int i = 0; i < battle->partyCount && i < PokeParty_GetPkmCount(party); i++) {
        ReadPkm(PokeParty_GetPkm(party, i), &battle->party[i]);
    }
    battle->moneyChange = (s32)sSetup->moneyChange;
    for (int i = 0; i < battle->itemCount; i++) {
        // Items BW2 doesn't have were never in its bag: they keep their count
        if (sItemInBag[i]) {
            battle->items[i].count = BagSave_GetItemCountByID(sBag, battle->items[i].item, HEAPID_PORT_BATTLE);
        }
    }
    // A caught Pokémon is in the wild party (BtlMain_StorePartiesToSetup), with the ball it was caught in
    battle->caught = result == BTL_RESULT_CAPTURE && sSetup->capturedSlot < PokeParty_GetPkmCount(sSetup->party[1]);
    if (battle->caught) {
        battle->caughtWild2 = sSetup->capturedSlot == 1;
        battle->caughtMon = battle->caughtWild2 ? battle->wild2 : battle->wild;
        ReadPkm(PokeParty_GetPkm(sSetup->party[1], sSetup->capturedSlot), &battle->caughtMon);
    }
    printf("Gen 5: the battle ended (BW2's result %lu, money %+d)\n", (unsigned long)result, battle->moneyChange);
    sSetup = NULL;
    while (sBattleBlockCount > 0) {
        GFL_HeapFree(sBattleBlocks[--sBattleBlockCount]);
    }
    GFL_HeapDelete(HEAPID_PORT_BATTLE);
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

// The prize money's base for a Platinum trainer (btl_calc.c's patch): Platinum's class multiplier
u32 Gen5_PortTrainerPrizeBase(u32 trainerId)
{
    const Gen5Trainer *trainer = TrainerById(trainerId);
    return trainer != NULL ? trainer->prizeBase : 0;
}

// The line a Platinum trainer says when beaten (btlv_core.c's patch), into strbuf: FALSE if it has none
BOOL Gen5_PortTrainerDefeatText(StrBuf *strbuf, u32 trainerId)
{
    const Gen5Trainer *trainer = TrainerById(trainerId);
    if (trainer == NULL || trainer->defeatText[0] == 0xFFFF) {
        return FALSE;
    }
    GFL_StrBufLoadString(strbuf, trainer->defeatText);
    printf("Gen 5: trainer %u says its defeat line (%lu characters)\n", trainer->id,
        (unsigned long)GFL_StrBufGetCharCount(strbuf));
    return TRUE;
}

// The summary screen (BW2's overlay 207) on the same systems: a proc of its own in place of the battle's
static PStatusParam sSummaryParam;
static PokeParty *sSummaryParty;
static BOOL sSummaryRunning;

int Gen5_SummaryBegin(const Gen5Summary *summary, const char *gen5Dir)
{
    if (sRunning || !Start(gen5Dir) || summary->partyCount < 1) {
        return FALSE;
    }
    GFL_HeapCreateChild(HEAPID_USER, HEAPID_PORT_BATTLE, HEAP_PORT_BATTLE_SIZE);
    sBattleBlockCount = 0;

    PlayerInfo *info = Keep(PlayerInfo_Create(HEAPID_PORT_BATTLE));
    copyTrainerName(info, summary->player.name);
    setIDAsUInt(info, summary->player.id);
    setTrainerGender(info, summary->player.gender);
    PlayerInfo_SetVersion(info, VERSION_PLATINUM);
    Config *config = Keep(Config_Create(HEAPID_PORT_BATTLE));
    Config_SetTextSpeed(config, 1);
    Config_SetTextSpeed(&getTrainerDataBlkAddress(SaveControl_GetInstance())->config, 1);
    sSummaryParty = NewParty(summary->party, summary->partyCount);
    Gen5_SetBattleGameData(sSummaryParty, info, config, NULL, NULL, NULL, NULL);

    memset(&sSummaryParam, 0, sizeof(sSummaryParam));
    sSummaryParam.party = sSummaryParty;
    sSummaryParam.dataType = PSTATUS_DATA_PARTY;
    sSummaryParam.partyCount = summary->partyCount;
    sSummaryParam.partyIndex = summary->index;
    sSummaryParam.trainerData = getTrainerDataBlkAddress(SaveControl_GetInstance());
    sSummaryParam.gameData = Gen5_GetGameData();
    sSummaryParam.isNationalDex = TRUE;
    sSummaryParam.fromFieldMenu = FALSE;
    sSummaryParam.page = PSTATUS_PAGE_INFO;
    sSummaryParam.result = PSTATUS_RESULT_BACK;
    if (summary->mode == GEN5_SUMMARY_FORGET_MOVE) {
        sSummaryParam.mode = PSTATUS_MODE_FORGET_MOVE;
        sSummaryParam.move = summary->move;
        sSummaryParam.page = PSTATUS_PAGE_SKILL;
    }
    NNS_GfdInitVramTransferManager(sVramTransferTasks, VRAM_TRANSFER_TASKS);
    Gen5_SoundBattleBegin();
    GCTX_ProcMgrQueueProc(OVERLAY_NONE, &PSTATUS_PROC_FUNCTIONS, &sSummaryParam);
    sRunning = TRUE;
    sSummaryRunning = TRUE;
    printf("Gen 5: the summary screen, %d Pokémon, from %d\n", summary->partyCount, summary->index);
    return TRUE;
}

enum Gen5SummaryResult Gen5_SummaryEnd(Gen5Summary *summary)
{
    if (!sSummaryRunning) {
        return GEN5_SUMMARY_FAILED;
    }
    sSummaryRunning = FALSE;
    Gen5_SoundBattleEnd();
    summary->index = sSummaryParam.partyIndex;
    summary->slot = sSummaryParam.slot;
    printf("Gen 5: the summary screen ended (result %d, Pokémon %d, slot %d)\n", sSummaryParam.result,
        summary->index, summary->slot);
    const u8 result = sSummaryParam.result;
    sSummaryParty = NULL;
    while (sBattleBlockCount > 0) {
        GFL_HeapFree(sBattleBlocks[--sBattleBlockCount]);
    }
    GFL_HeapDelete(HEAPID_PORT_BATTLE);
    switch (result) {
    case PSTATUS_RESULT_FORGET:
        return GEN5_SUMMARY_FORGET;
    case PSTATUS_RESULT_CLOSE:
        return GEN5_SUMMARY_CLOSE;
    default:
        return GEN5_SUMMARY_BACK;
    }
}

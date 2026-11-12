// BW2's GameData and SaveControl for the port (Gen 5 battles), in place of pokebw2's src/system/game_data.c and
// save_control.c, which bring BW2's whole field and save with them. The battle reads the player's party, bag,
// records and so on through these: the objects runtime.c makes for each battle (Gen5_SetBattleGameData). For now
// they're new and empty; milestone 4 (the bridge) fills them from Platinum's save.

#include "types.h"

#include <stdio.h>
#include <stdlib.h>

#include "gfl/savedata.h"
#include "pml/mail.h"
#include "save/adventure.h"
#include "save/box.h"
#include "save/chatter.h"
#include "save/join_avenue.h"
#include "save/key_data_savedata.h"
#include "save/medal_box.h"
#include "save/misc_savedata.h"
#include "save/musical_savedata.h"
#include "save/pokedex_savedata.h"
#include "save/records.h"
#include "save/save_control.h"
#include "save/survey_savedata.h"
#include "save/tournament_block.h"
#include "save/trainer_data_savedata.h"
#include "save/trainer_game_info.h"
#include "system/game_data.h"
#include "system/pms_word.h"

static struct {
    PokeParty *party;
    PlayerInfo *info;
    Config *config;
    BagSave *bag;
    BagCursor *bagCursor;
    PokeDexSave *pokedex;
    GameRecords *records;
} sGameData;

void Gen5_SetBattleGameData(PokeParty *party, PlayerInfo *info, Config *config, BagSave *bag, BagCursor *bagCursor,
    PokeDexSave *pokedex, GameRecords *records)
{
    sGameData.party = party;
    sGameData.info = info;
    sGameData.config = config;
    sGameData.bag = bag;
    sGameData.bagCursor = bagCursor;
    sGameData.pokedex = pokedex;
    sGameData.records = records;
}

// BW2's GameData is opaque to the battle: this one is the port's
GameData *Gen5_GetGameData(void)
{
    return (GameData *)&sGameData;
}

PokeParty *GameData_GetParty(GameData *gameData) {
    (void)gameData;
    return sGameData.party;
}

BagSave *GameData_GetBag(GameData *gameData) {
    (void)gameData;
    return sGameData.bag;
}

BagCursor *GameData_GetBagCursor(GameData *gameData) {
    (void)gameData;
    return sGameData.bagCursor;
}

BoxSaveAccessor *GameData_GetBoxSaveAccessor(GameData *gameData) {
    (void)gameData;
    return NULL;
}

CommPlayerSupport *GameData_GetCommPlayerSupport(GameData *gameData) {
    (void)gameData;
    return NULL;
}

EventWork *GameData_GetEventWork(GameData *gameData) {
    (void)gameData;
    return NULL;
}

GameRecords *GameData_GetRecords(GameData *gameData) {
    (void)gameData;
    return sGameData.records;
}

JoinAvenuePersonList **GameData_GetJoinAvenuePersonListPtr(GameData *gameData) {
    (void)gameData;
    return NULL;
}

PlayTime *GameData_GetPlayTime(GameData *gameData) {
    (void)gameData;
    return NULL;
}

PlayerState *GameData_GetPlayerState(GameData *gameData) {
    (void)gameData;
    return NULL;
}

ResortWork *GameData_GetResortWork(GameData *gameData) {
    (void)gameData;
    return NULL;
}

WifiList *GameData_GetWifiList(GameData *gameData) {
    (void)gameData;
    return NULL;
}

PlayerInfo *GetGameDataPlayerInfo(GameData *gameData) {
    (void)gameData;
    return sGameData.info;
}

// The trainer info block of the port's save, as in BW2. A lost battle reads the money from it (CalcLossMoney): 0
// until milestone 4 brings Platinum's over
TrainerCardSave *getTrainerCardDataBlkAddress(GameData *gameData) {
    (void)gameData;
    return getTrainerGameInfoAddress(SaveControl_GetInstance());
}

// Platinum has no C-Gear
BOOL GameData_IsCGearOn(GameData *gameData) {
    (void)gameData;
    return FALSE;
}

// Spring: Platinum has no seasons
u8 GameData_GetSeason(GameData *gameData) {
    (void)gameData;
    return 0;
}

// Whether the battle menu's cursor was last moved with the keys (rather than touch)
static u8 sBattleKeyCursorFlag;

u8 GameData_GetBattleKeyCursorFlag(GameData *gameData) {
    (void)gameData;
    return sBattleKeyCursorFlag;
}

void GameData_SetBattleKeyCursorFlag(GameData *gameData, u8 value) {
    (void)gameData;
    sBattleKeyCursorFlag = value;
}

// The port's save: the blocks of BW2's main save that the battle code reads (from save_table.c, whose other blocks'
// code isn't built), each allocated and initialized the first time it's asked for, and kept for the session. Battles
// don't write the save; milestone 4 fills the blocks that matter from Platinum's.
typedef void (*SaveBlockInit)(void *block);

static const SaveBlockDef sBlocks[] = {
    { SAVE_BLOCK_BOX_MAIN, BoxMainSave_GetBlockSize, (SaveBlockInit)BoxMainSave_InitBlock },
    { SAVE_BLOCK_BOX_TRAY, getSizeofBox, initFullBox },
    { SAVE_BLOCK_TRAINER_DATA, getSizeofTrainerData, (SaveBlockInit)TrainerDataSave_Init },
    { SAVE_BLOCK_CHATTER, PerapVoice_GetSize, (SaveBlockInit)initChatotChatter },
    { SAVE_BLOCK_ADVENTURE, getSizeofAdventureDataBlk, (SaveBlockInit)initAdventureDataBlk },
    { SAVE_BLOCK_RECORDS, GameRecords_GetSize, (SaveBlockInit)initTrainerCardInfoBlk },
    { SAVE_BLOCK_PMS, PMSWordSave_GetSize, (SaveBlockInit)PMSWordSave_Init },
    { SAVE_BLOCK_MAIL, MailBox_GetSize, (SaveBlockInit)MailBox_Clear },
    { SAVE_BLOCK_MUSICAL, MusicalSave_GetSize, (SaveBlockInit)MusicalSave_Init },
    { SAVE_BLOCK_TOURNAMENT, getSizeofTourneyBlock, clearTournamentBlock },
    { SAVE_BLOCK_TRAINER_GAME_INFO, TrainerGameInfo_GetSize, (SaveBlockInit)TrainerGameInfo_Init },
    { SAVE_BLOCK_POKEDEX, getSizeofPokedexData, (SaveBlockInit)initPokedex },
    { SAVE_BLOCK_SURVEY, SurveySave_GetSize, (SaveBlockInit)SurveySave_Init },
    { SAVE_BLOCK_MISC, getSizeofHollow_RivalData, (SaveBlockInit)clearHollowRivalData },
    { SAVE_BLOCK_JOIN_AVENUE, JoinAvenueSave_GetSize, (SaveBlockInit)JoinAvenueSave_Init },
    { SAVE_BLOCK_MEDAL_BOX, MedalBox_GetBlockSize, (SaveBlockInit)MedalBox_Init },
    { SAVE_BLOCK_KEY_DATA, KeyDataSave_GetSize, (SaveBlockInit)KeyDataSave_Init },
};

static void *sBlockData[SAVE_BLOCK_MAX];

// Never dereferenced: the battle only passes it back
static u32 sSaveControl;

SaveControl *SaveControl_GetInstance(void) {
    return (SaveControl *)&sSaveControl;
}

SaveControl *GameData_GetSaveControl(GameData *gameData) {
    (void)gameData;
    return SaveControl_GetInstance();
}

void *SaveControl_GetBlockPtr(SaveControl *save, u32 block) {
    (void)save;
    if (block >= SAVE_BLOCK_MAX) {
        return NULL;
    }
    if (sBlockData[block] == NULL) {
        // The box trays share one definition
        u32 id = block > SAVE_BLOCK_BOX_TRAY && block < SAVE_BLOCK_BAG ? SAVE_BLOCK_BOX_TRAY : block;
        for (u32 i = 0; i < NELEMS(sBlocks); i++) {
            if (sBlocks[i].gmdataID == id) {
                void *data = calloc(1, sBlocks[i].getSize());
                sBlocks[i].init(data);
                sBlockData[block] = data;
                break;
            }
        }
        if (sBlockData[block] == NULL) {
            fprintf(stderr, "gen5: no port save block %u\n", (unsigned)block);
        }
    }
    return sBlockData[block];
}

void SaveControl_Free(void) {
}

// The summary screen's (overlay 207) own GameData: the same one, and no shortcut menu
GameData *GameData_Create(HeapID heapId) {
    (void)heapId;
    return Gen5_GetGameData();
}

void GameData_Free(GameData *gameData) {
    (void)gameData;
}

BOOL GameData_IsShortcutRegistered(GameData *gameData, u32 item) {
    (void)gameData;
    (void)item;
    return FALSE;
}

void GameData_SetKeyItemRegistration(GameData *gameData, u32 item, BOOL registered) {
    (void)gameData;
    (void)item;
    (void)registered;
}

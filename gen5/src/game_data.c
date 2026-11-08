// BW2's GameData and SaveControl for the port (Gen 5 battles), in place of pokebw2's src/system/game_data.c and
// save_control.c, which bring BW2's whole field and save with them. The battle reads the player's party, bag,
// records and so on through these. Milestone 4 (the bridge) answers them from Platinum's save; until then a battle
// has none of it.

#include "types.h"

#include "save/save_control.h"
#include "system/game_data.h"

PokeParty *GameData_GetParty(GameData *gameData) {
    (void)gameData;
    return NULL;
}

BagSave *GameData_GetBag(GameData *gameData) {
    (void)gameData;
    return NULL;
}

BagCursor *GameData_GetBagCursor(GameData *gameData) {
    (void)gameData;
    return NULL;
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
    return NULL;
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

SaveControl *GameData_GetSaveControl(GameData *gameData) {
    (void)gameData;
    return NULL;
}

WifiList *GameData_GetWifiList(GameData *gameData) {
    (void)gameData;
    return NULL;
}

PlayerInfo *GetGameDataPlayerInfo(GameData *gameData) {
    (void)gameData;
    return NULL;
}

TrainerCardSave *getTrainerCardDataBlkAddress(GameData *gameData) {
    (void)gameData;
    return NULL;
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

SaveControl *SaveControl_GetInstance(void) {
    return NULL;
}

void *SaveControl_GetBlockPtr(SaveControl *save, u32 block) {
    (void)save;
    (void)block;
    return NULL;
}

void SaveControl_Free(void) {
}

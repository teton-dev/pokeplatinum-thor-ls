// What BW2's battle code calls in BW2's field, network and DSi systems, for the port (Gen 5 battles). A battle run
// from Platinum has none of these: no beacons, Join Avenue, Festival Missions, Pokémon World Tournament, seasons,
// wireless or infrared link.

#include "types.h"

#include "field/fest_mission_data.h"
#include "field/wbt.h"
#include "gfl/net.h"
#include "gfl/wm_icon.h"
#include "pml/poke_party.h"
#include "system/game_beacon.h"
#include "system/ir_check.h"
#include "system/machine_use.h"
#include "system/net_define.h"

// Game beacons (src/system/game_beacon.c): what the player does, sent to nearby games

void GameBeaconSys_SendCapture(u16 species, BOOL a1, BOOL a2) {
    (void)species;
    (void)a1;
    (void)a2;
}

void GameBeaconSys_SendDefeatMission(u16 species, BOOL a1, BOOL a2) {
    (void)species;
    (void)a1;
    (void)a2;
}

void GameBeaconSys_SendNickname(const StrBuf *nickname) {
    (void)nickname;
}

int GameBeaconSys_GetNextNew(int *index) {
    (void)index;
    return 0;
}

BOOL GameBeacon_IsMissionType(const GameBeacon *beacon) {
    (void)beacon;
    return FALSE;
}

// Festival Missions (overlay 27), which the word sets can name

FestivalText *getTextFileForFestMissions(HeapID heapId) {
    (void)heapId;
    return NULL;
}

void FestMission_GetMessage(FestivalText *text, StrBuf *dest, const FestMissionConfig *mission, HeapID heapId) {
    (void)text;
    (void)dest;
    (void)mission;
    (void)heapId;
}

void FestMission_Load(ArcTool *arc, u8 index, u32 level, FestMissionConfig *mission) {
    (void)arc;
    (void)index;
    (void)level;
    (void)mission;
}

void FestivalText_Free(FestivalText *text) {
    (void)text;
}

// The Pokémon World Tournament's names (overlay 36), which the word sets can name
void LoadPWTTournamentTypeText(HeapID heapId, u32 tournament, StrBuf *strbuf) {
    (void)heapId;
    (void)tournament;
    (void)strbuf;
}

// Seasonal forms (overlay 12): Deerling and Sawsbuck keep theirs
void TransformVsPokePartyBySeason(GameData *gameData, PokeParty *party, u8 season) {
    (void)gameData;
    (void)party;
    (void)season;
}

// The infrared check of a genuine card (overlay 338)
BOOL IrCheck_IsGenuineCard(void) {
    return TRUE;
}

// The DSi's request to quit to its menu
void Machine_ProcessExitRequest(void) {
}

// Network devices (src/system/net_define.c, overlays 11, 30, 31): none

BOOL GflNet_IsWifiGameKind(void) {
    return FALSE;
}

const GFLNetDevTable *NetDefine_LoadDevice(int type) {
    (void)type;
    return NULL;
}

void NetDefine_UnloadDevice(int type) {
    (void)type;
}

void NetDefine_Update(void) {
}

void NetBgmVolume_Set(u16 trackMask, s32 volume) {
    (void)trackMask;
    (void)volume;
}

u32 WihDwc_GetStatus(WifiList *wifiList) {
    (void)wifiList;
    return 0;
}

// The wireless icon's graphics (src/system/net_define.c)

void WmIconFiles_Init(WmIconFiles *files) {
    (void)files;
}

u32 getWifiIconNarcIdxNum(void) {
    return 0;
}

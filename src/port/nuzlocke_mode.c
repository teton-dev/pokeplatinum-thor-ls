// Nuzlocke mode's state. See include/port/nuzlocke_mode.h
#include "port/nuzlocke_mode.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "port/nuzlocke_mode_game.h"

#define RECORD_MAGIC 0x4C5A554E // "NUZL"
#define RECORD_VERSION 2
#define RECORD_V1_SIZE 64
#define MAX_RECORD_SECTORS 8
#define MAX_AREAS 160

enum AreaResult {
    AREA_IN_BATTLE = 0, // its first encounter is going on (or the game stopped during it: counts as missed)
    AREA_CAUGHT,
    AREA_MISSED,        // fainted, fled, or the player ran: only that species may be caught there
};

// An area (a location name, as the map's sign shows it) whose encounter is used
typedef struct NuzlockeArea {
    u16 location;
    u16 species;
    u8 result; // AreaResult
    u8 unused;
} NuzlockeArea;

// One per save (trainer ID), at the start of its sector, written to both halves of the image. Version 1 was the
// first 60 bytes and a checksum (64 bytes); version 2 adds the areas. The checksum is always the last 4 bytes of
// `size`.
typedef struct NuzlockeRecord {
    u32 magic;
    u16 version;
    u16 size;
    u32 counter; // higher is newer (across all records, so the oldest can be reused)
    u32 saveId;
    u8 level;
    u8 critGuard;
    u8 dropped;
    u8 ballsReceived; // Normal and Hard's catch rule starts with the first Poke Balls
    u8 reserved[40];
    // version 2
    u16 areaCount;
    u16 reserved2;
    NuzlockeArea areas[MAX_AREAS];
    u32 checksum;
} NuzlockeRecord;

_Static_assert(offsetof(NuzlockeRecord, areaCount) == RECORD_V1_SIZE - 4, "version 1's fields stay where they were");
_Static_assert(sizeof(NuzlockeRecord) == 1028, "NuzlockeRecord's size is part of the save format");

static struct {
    BOOL loaded;
    int slot;
    u32 saveId;
    int index; // the record's sector, -1 while it has none
    NuzlockeRecord record;
    u32 maxCounter;
    BOOL dirty; // changed, waiting for the game's save to finish before it is written
} sState;

// What the GUI asked for, as the whole state it wants (the game thread is paused while the settings are open, so
// the GUI shows the change at once and the game thread writes it when it runs again)
static volatile struct {
    BOOL pending;
    u32 saveId;
    u8 level;
    u8 critGuard;
    u8 dropped;
} sWanted;
static void CheckBalls(void);
static NuzlockeModeView sView;
static u32 sViewSaveId;
static BOOL sStorageError;

// CRC-32 of the record's first `size` bytes but the last 4 (its checksum)
static u32 Checksum(const void *data, u32 size)
{
    const u8 *bytes = data;
    u32 crc = 0xFFFFFFFF;
    for (u32 i = 0; i + 4 < size; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// A version 1 or 2 record, read as version 2 (a version 1 one has no areas)
static BOOL IsValid(NuzlockeRecord *record)
{
    if (record->magic != RECORD_MAGIC) {
        return FALSE;
    }
    u32 stored;
    if (record->version == 1 && record->size == RECORD_V1_SIZE) {
        memcpy(&stored, (const u8 *)record + RECORD_V1_SIZE - 4, 4);
        if (stored != Checksum(record, RECORD_V1_SIZE)) {
            return FALSE;
        }
        record->areaCount = 0;
        record->reserved2 = 0;
        memset(record->areas, 0, sizeof(record->areas));
        return TRUE;
    }
    if (record->version >= 2 && record->size == sizeof(NuzlockeRecord)) {
        return record->checksum == Checksum(record, sizeof(NuzlockeRecord)) && record->areaCount <= MAX_AREAS;
    }
    return FALSE;
}

// The newer valid copy of record index (primary or backup half); FALSE if neither is valid
static BOOL ReadRecord(int index, NuzlockeRecord *out)
{
    u32 primary, backup, sectorSize;
    int count;
    NuzlockeModeGame_RecordSectors(&primary, &backup, &count, &sectorSize);

    NuzlockeRecord copies[2];
    const u32 starts[2] = { primary, backup };
    BOOL valid[2];
    for (int i = 0; i < 2; i++) {
        // The header first: a port save file may end right after a version 1 record
        const u32 address = (starts[i] + index) * sectorSize;
        memset(&copies[i], 0, sizeof(copies[i]));
        valid[i] = NuzlockeModeGame_ReadFlash(address, &copies[i], RECORD_V1_SIZE);
        if (valid[i] && copies[i].magic == RECORD_MAGIC && copies[i].size == sizeof(NuzlockeRecord)) {
            valid[i] = NuzlockeModeGame_ReadFlash(address, &copies[i], sizeof(copies[i]));
        }
        valid[i] = valid[i] && IsValid(&copies[i]);
    }
    if (!valid[0] && !valid[1]) {
        return FALSE;
    }
    const int best = !valid[1] ? 0 : !valid[0] ? 1 : copies[1].counter > copies[0].counter ? 1 : 0;
    *out = copies[best];
    return TRUE;
}

static int RecordCount(void)
{
    u32 primary, backup, sectorSize;
    int count;
    NuzlockeModeGame_RecordSectors(&primary, &backup, &count, &sectorSize);
    return count < MAX_RECORD_SECTORS ? count : MAX_RECORD_SECTORS;
}

// Finds the running save's record, or leaves the defaults (off) with no record yet
static void Load(int slot, u32 saveId)
{
    memset(&sState, 0, sizeof(sState));
    sState.loaded = TRUE;
    sState.slot = slot;
    sState.saveId = saveId;
    sState.index = -1;

    for (int i = 0; i < RecordCount(); i++) {
        NuzlockeRecord record;
        if (!ReadRecord(i, &record)) {
            continue;
        }
        if (record.counter > sState.maxCounter) {
            sState.maxCounter = record.counter;
        }
        if (record.saveId == saveId && (sState.index < 0 || record.counter > sState.record.counter)) {
            sState.index = i;
            sState.record = record;
        }
    }
    if (sState.index < 0) {
        sState.record.magic = RECORD_MAGIC;
        sState.record.saveId = saveId;
    }
    if (sState.record.level >= NUZLOCKE_LEVEL_COUNT || sState.record.dropped) {
        sState.record.level = NUZLOCKE_OFF;
    }
    sStorageError = FALSE;
}

// A sector for a save that has no record yet: an empty one, or the oldest
static int PickSector(void)
{
    int oldest = 0;
    u32 oldestCounter = 0xFFFFFFFF;
    for (int i = 0; i < RecordCount(); i++) {
        NuzlockeRecord record;
        if (!ReadRecord(i, &record)) {
            return i;
        }
        if (record.counter < oldestCounter) {
            oldestCounter = record.counter;
            oldest = i;
        }
    }
    return oldest;
}

static void WriteNow(void)
{
    sState.dirty = FALSE;
    if (sState.index < 0) {
        sState.index = PickSector();
    }
    NuzlockeRecord *record = &sState.record;
    record->magic = RECORD_MAGIC;
    record->version = RECORD_VERSION;
    record->size = sizeof(*record);
    record->saveId = sState.saveId;
    record->counter = ++sState.maxCounter;
    record->checksum = Checksum(record, sizeof(*record));

    u32 primary, backup, sectorSize;
    int count;
    NuzlockeModeGame_RecordSectors(&primary, &backup, &count, &sectorSize);
    // The primary first: should the backup's write fail, the primary is the newer one
    BOOL ok = NuzlockeModeGame_WriteFlash((primary + sState.index) * sectorSize, record, sizeof(*record));
    ok = NuzlockeModeGame_WriteFlash((backup + sState.index) * sectorSize, record, sizeof(*record)) && ok;
    sStorageError = !ok;
    printf("Nuzlocke mode: %s%s%s for save %08X (record %d)%s\n", NuzlockeMode_LevelName(record->level),
        record->critGuard ? ", crit guard" : "", record->dropped ? ", dropped" : "", (unsigned)sState.saveId,
        sState.index, ok ? "" : ": WRITE FAILED");
}

static void Write(void)
{
    sState.dirty = TRUE;
    if (NuzlockeModeGame_CanWrite()) {
        WriteNow();
    }
}

// The rules of a change: OFF drops the run for good, Hard has no crit guard, a dropped save stays as it is
static void ApplyChange(u8 *level, u8 *critGuard, u8 *dropped, int newLevel, int newCritGuard)
{
    if (*dropped) {
        return;
    }
    if (newLevel >= 0 && newLevel < NUZLOCKE_LEVEL_COUNT && newLevel != *level) {
        *level = (u8)newLevel;
        if (newLevel == NUZLOCKE_OFF) {
            *dropped = TRUE;
            *critGuard = FALSE;
        } else if (newLevel == NUZLOCKE_HARD) {
            *critGuard = FALSE;
        }
    }
    if (newCritGuard >= 0 && *level != NUZLOCKE_OFF && *level != NUZLOCKE_HARD) {
        *critGuard = newCritGuard ? TRUE : FALSE;
    }
}

static void ApplyRequests(void)
{
    if (!sWanted.pending) {
        return;
    }
    sWanted.pending = FALSE;
    NuzlockeRecord *record = &sState.record;
    if (sWanted.saveId != sState.saveId || record->dropped) {
        return;
    }
    if (sWanted.level != record->level || sWanted.critGuard != record->critGuard || sWanted.dropped != record->dropped) {
        record->level = sWanted.level;
        record->critGuard = sWanted.critGuard;
        record->dropped = sWanted.dropped;
        Write();
    }
}

void NuzlockeMode_Update(void)
{
    u32 saveId;
    int slot;
    NuzlockeModeView view;
    memset(&view, 0, sizeof(view));

    if (!NuzlockeModeGame_RunningSave(&saveId, &slot)) {
        // Read again when a game runs: the slot may change, or its save be replaced, meanwhile
        sState.loaded = FALSE;
        sWanted.pending = FALSE;
        sView = view;
        return;
    }
    if (!sState.loaded || sState.slot != slot || sState.saveId != saveId) {
        Load(slot, saveId);
    }
    ApplyRequests();
    // The first Poke Balls start the catch rule (looked for twice a second)
    static u32 sFrames;
    if ((++sFrames & 31) == 0) {
        CheckBalls();
    }
    if (sState.dirty && NuzlockeModeGame_CanWrite()) {
        WriteNow();
    }

    view.hasSave = TRUE;
    view.level = sState.record.level;
    view.critGuard = sState.record.critGuard;
    view.dropped = sState.record.dropped;
    view.storageError = sStorageError;
    sView = view;
    sViewSaveId = sState.saveId;
}

int NuzlockeMode_Level(void)
{
    return sState.loaded ? sState.record.level : NUZLOCKE_OFF;
}

BOOL NuzlockeMode_On(void)
{
    return NuzlockeMode_Level() != NUZLOCKE_OFF;
}

BOOL NuzlockeMode_CritGuard(void)
{
    return NuzlockeMode_On() && NuzlockeMode_Level() != NUZLOCKE_HARD && sState.record.critGuard;
}

BOOL NuzlockeMode_WasDropped(void)
{
    return sState.loaded && sState.record.dropped;
}

void NuzlockeMode_Drop(void)
{
    if (!NuzlockeMode_On()) {
        return;
    }
    sState.record.level = NUZLOCKE_OFF;
    sState.record.critGuard = FALSE;
    sState.record.dropped = TRUE;
    Write();
}

int NuzlockeMode_LevelOfSave(u32 saveId)
{
    NuzlockeRecord record, best;
    BOOL found = FALSE;
    for (int i = 0; i < RecordCount(); i++) {
        if (ReadRecord(i, &record) && record.saveId == saveId && (!found || record.counter > best.counter)) {
            best = record;
            found = TRUE;
        }
    }
    if (!found || best.dropped || best.level >= NUZLOCKE_LEVEL_COUNT) {
        return NUZLOCKE_OFF;
    }
    return best.level;
}

void NuzlockeMode_GetView(NuzlockeModeView *out)
{
    *out = sView;
}

static void Request(int level, int critGuard)
{
    if (!sView.hasSave) {
        return;
    }
    u8 newLevel = sView.level;
    u8 newCritGuard = (u8)sView.critGuard;
    u8 newDropped = (u8)sView.dropped;
    ApplyChange(&newLevel, &newCritGuard, &newDropped, level, critGuard);
    sView.level = newLevel;
    sView.critGuard = newCritGuard;
    sView.dropped = newDropped;
    sWanted.saveId = sViewSaveId;
    sWanted.level = newLevel;
    sWanted.critGuard = newCritGuard;
    sWanted.dropped = newDropped;
    sWanted.pending = TRUE;
}

void NuzlockeMode_RequestLevel(int level)
{
    Request(level, -1);
}

void NuzlockeMode_RequestCritGuard(BOOL on)
{
    Request(-1, on ? 1 : 0);
}

const char *NuzlockeMode_LevelName(int level)
{
    static const char *const kNames[NUZLOCKE_LEVEL_COUNT] = { "Off", "Simple", "Normal", "Hard" };
    return level >= 0 && level < NUZLOCKE_LEVEL_COUNT ? kNames[level] : "?";
}

// --- the catch rule (Normal and Hard) ------------------------------------------------------

// The wild battle going on
static struct {
    BOOL active;     // the rule applies to it
    BOOL exempt;     // a shiny, or a static encounter in Normal: may be caught, uses nothing
    u16 location;
    BOOL nextStatic; // NuzlockeMode_MarkStatic: the next battle is a scripted one
} sBattle;

static BOOL CatchRuleOn(void)
{
    return NuzlockeMode_Level() >= NUZLOCKE_NORMAL;
}

static NuzlockeArea *FindArea(u16 location)
{
    for (int i = 0; i < sState.record.areaCount; i++) {
        if (sState.record.areas[i].location == location) {
            return &sState.record.areas[i];
        }
    }
    return NULL;
}

static void CheckBalls(void)
{
    if (CatchRuleOn() && !sState.record.ballsReceived && NuzlockeModeGame_HasPokeBalls()) {
        sState.record.ballsReceived = TRUE;
        Write();
    }
}

void NuzlockeMode_MarkStatic(void)
{
    sBattle.nextStatic = TRUE;
}

void NuzlockeMode_OnWildBattleStart(u16 location, u16 species, BOOL shiny)
{
    const BOOL isStatic = sBattle.nextStatic;
    memset(&sBattle, 0, sizeof(sBattle));
    CheckBalls();
    if (!CatchRuleOn() || !sState.record.ballsReceived) {
        return;
    }
    sBattle.active = TRUE;
    sBattle.location = location;
    // Shinies never count; static encounters count only with Hard
    sBattle.exempt = shiny || (isStatic && NuzlockeMode_Level() != NUZLOCKE_HARD);
    if (sBattle.exempt || FindArea(location) != NULL || sState.record.areaCount >= MAX_AREAS) {
        return;
    }
    // The area's first encounter: written now, so that stopping the game during it doesn't give it back
    NuzlockeArea *area = &sState.record.areas[sState.record.areaCount++];
    area->location = location;
    area->species = species;
    area->result = AREA_IN_BATTLE;
    area->unused = 0;
    Write();
}

BOOL NuzlockeMode_CatchAllowed(u16 species, BOOL shiny)
{
    if (!sBattle.active || sBattle.exempt || shiny || !CatchRuleOn()) {
        return TRUE;
    }
    const NuzlockeArea *area = FindArea(sBattle.location);
    if (area == NULL) {
        return TRUE; // no room left in the books
    }
    return area->result != AREA_CAUGHT && area->species == species;
}

void NuzlockeMode_OnWildBattleEnd(BOOL captured)
{
    const BOOL active = sBattle.active && !sBattle.exempt;
    const u16 location = sBattle.location;
    memset(&sBattle, 0, sizeof(sBattle));
    if (!active || !CatchRuleOn()) {
        return;
    }
    NuzlockeArea *area = FindArea(location);
    if (area == NULL || area->result == AREA_CAUGHT) {
        return;
    }
    const u8 result = captured ? AREA_CAUGHT : AREA_MISSED;
    if (area->result != result) {
        area->result = result;
        Write();
    }
}

BOOL NuzlockeMode_BallsReceived(void)
{
    return sState.loaded && sState.record.ballsReceived;
}

int NuzlockeMode_AreaState(u16 location, u16 *species)
{
    const NuzlockeArea *area = sState.loaded ? FindArea(location) : NULL;
    if (area == NULL) {
        return NUZLOCKE_AREA_OPEN;
    }
    if (species != NULL) {
        *species = area->species;
    }
    return area->result == AREA_CAUGHT ? NUZLOCKE_AREA_CAUGHT : NUZLOCKE_AREA_MISSED;
}

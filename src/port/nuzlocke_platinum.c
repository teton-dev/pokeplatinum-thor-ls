// Platinum's side of Nuzlocke mode (port/nuzlocke_mode_game.h): the running save, where the
// records go in the save image, and the dead flag on a Pokemon.
#include <nitro.h>

#include "constants/savedata/savedata.h"
#include "constants/savedata/save_table.h"

#include "field/field_system.h"
#include "bag.h"
#include "constants/items.h"
#include "item.h"
#include "port/debug_field.h"
#include "port/nuzlocke_mode.h"
#include "port/nuzlocke_mode_game.h"
#include "port/save_slots.h"
#include "pokemon.h"
#include "save_player.h"
#include "savedata.h"
#include "system.h"
#include "trainer_info.h"

// Each half of the 512 KB image is 64 sectors: the save body in 0-31 (SAVE_PAGE_MAX), the extra
// saves (Hall of Fame, Battle Hall, recordings) in 32-43 (gExtraSaveTable). Platinum never
// touches 44-63, only erases them with the rest; the records take the last four.
#define RECORD_SECTOR_FIRST 60
#define RECORD_SECTOR_COUNT 4

_Static_assert(RECORD_SECTOR_FIRST + RECORD_SECTOR_COUNT <= BACKUP_SECTOR_START - PRIMARY_SECTOR_START,
    "Nuzlocke records must stay inside the primary half");

// Pokemon data's block B, 0x1A (MON_DATA_UNUSED_114): unused in Platinum and HGSS. Bit 0 is the
// Nuzlocke dead flag; the other bits are free.
#define PORT_MON_FLAGS MON_DATA_UNUSED_114
#define PORT_MON_FLAG_NUZLOCKE_DEAD 0x0001

BOOL NuzlockeModeGame_RunningSave(u32 *saveId, int *slot)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || fieldSystem->saveData == NULL) {
        return FALSE;
    }
    *saveId = TrainerInfo_ID(SaveData_GetTrainerInfo(fieldSystem->saveData));
    *slot = SaveSlots_Current();
    return TRUE;
}

void NuzlockeModeGame_RecordSectors(u32 *primary, u32 *backup, int *count, u32 *sectorSize)
{
    *primary = PRIMARY_SECTOR_START + RECORD_SECTOR_FIRST;
    *backup = BACKUP_SECTOR_START + RECORD_SECTOR_FIRST;
    *count = RECORD_SECTOR_COUNT;
    *sectorSize = SAVE_SECTOR_SIZE;
}

BOOL NuzlockeModeGame_CanWrite(void)
{
    // Set while the game saves (as for the soft reset)
    return gSystem.inhibitReset == 0;
}

BOOL NuzlockeModeGame_HasPokeBalls(void)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || fieldSystem->saveData == NULL) {
        return FALSE;
    }
    Bag *bag = SaveData_GetBag(fieldSystem->saveData);
    for (u16 slot = 0;; slot++) {
        BagItem *item = Bag_GetItemSlot(bag, POCKET_BALLS, slot);
        if (item == NULL) {
            return FALSE;
        }
        if (item->item != ITEM_NONE && item->quantity > 0) {
            return TRUE;
        }
    }
}

BOOL NuzlockeModeGame_ReadFlash(u32 address, void *data, u32 size)
{
    return SaveData_CardLoad(address, data, size);
}

BOOL NuzlockeModeGame_WriteFlash(u32 address, void *data, u32 size)
{
    return SaveData_CardSave(address, data, size);
}

BOOL NuzlockeMode_BoxIsDead(BoxPokemon *boxMon)
{
    return (BoxPokemon_GetValue(boxMon, PORT_MON_FLAGS, NULL) & PORT_MON_FLAG_NUZLOCKE_DEAD) != 0;
}

BOOL NuzlockeMode_IsDead(Pokemon *mon)
{
    return (Pokemon_GetValue(mon, PORT_MON_FLAGS, NULL) & PORT_MON_FLAG_NUZLOCKE_DEAD) != 0;
}

void NuzlockeMode_SetDead(Pokemon *mon, BOOL dead)
{
    u16 flags = (u16)Pokemon_GetValue(mon, PORT_MON_FLAGS, NULL);
    flags = dead ? (u16)(flags | PORT_MON_FLAG_NUZLOCKE_DEAD) : (u16)(flags & ~PORT_MON_FLAG_NUZLOCKE_DEAD);
    Pokemon_SetValue(mon, PORT_MON_FLAGS, &flags);
}

// --- the rules -------------------------------------------------------------------------

int Gear_LevelCap(void);
int Gear_LevelCapLow(void);

int NuzlockeMode_LevelCap(void)
{
    if (!NuzlockeMode_On()) {
        return 0;
    }
    const int cap = NuzlockeMode_Level() == NUZLOCKE_HARD ? Gear_LevelCapLow() : Gear_LevelCap();
    return cap > 0 && cap <= MAX_POKEMON_LEVEL ? cap : 0;
}

static BOOL IsRealMon(Pokemon *mon)
{
    return Pokemon_GetValue(mon, MON_DATA_SPECIES_EXISTS, NULL) && !Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL);
}

u32 NuzlockeMode_FilterHP(Pokemon *mon, u32 hp)
{
    if (hp == 0 || !NuzlockeMode_On()) {
        return hp;
    }
    return NuzlockeMode_IsDead(mon) ? 0 : hp;
}

void NuzlockeMode_OnHPSet(Pokemon *mon, u32 hp)
{
    // Fainting: HP set to 0 on a Pokemon with stats (a new one's HP is set by its stats, never to 0)
    if (hp != 0 || !NuzlockeMode_On() || Pokemon_GetValue(mon, MON_DATA_MAX_HP, NULL) == 0 || !IsRealMon(mon)
        || NuzlockeMode_IsDead(mon)) {
        return;
    }
    NuzlockeMode_SetDead(mon, TRUE);
}

BOOL NuzlockeMode_RefusesItem(Pokemon *mon, u16 item, int heapID)
{
    if (!NuzlockeMode_On()) {
        return FALSE;
    }
    if (NuzlockeMode_IsDead(mon)) {
        return TRUE;
    }
    const int cap = NuzlockeMode_LevelCap();
    return cap > 0 && Item_LoadParam(item, ITEM_PARAM_LEVEL_UP, heapID)
        && (int)Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL) >= cap;
}

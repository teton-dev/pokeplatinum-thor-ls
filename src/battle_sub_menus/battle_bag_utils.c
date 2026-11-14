#include "battle_sub_menus/battle_bag_utils.h"

#include "battle_sub_menus/battle_bag.h"

#include "bag.h"
#include "item.h"

#ifdef SDK_PORT
#include "constants/items.h"

#include "port/nuzlocke_mode.h"

// Nuzlocke mode: Poke Balls are the only items used in battle
static s32 PortBattlePocketMask(u16 item, s32 mask)
{
    return NuzlockeMode_On() ? (mask & BATTLE_POCKET_MASK_POKE_BALLS) : mask;
}
#endif

// Order is dictated by the bit shifts associated with each pocket. See items.h
static const u8 sBattlePocketIndexes[] = {
    BATTLE_POCKET_INDEX_POKE_BALLS,
    BATTLE_POCKET_INDEX_BATTLE_ITEMS,
    BATTLE_POCKET_INDEX_RECOVER_HP_PP,
    BATTLE_POCKET_INDEX_RECOVER_STATUS,
    BATTLE_POCKET_INDEX_RECOVER_HP_PP,
};

BOOL BattleBag_CanUseLastUsedItem(BattleBag *battleBag)
{
    if (battleBag->context->lastUsedItem == ITEM_NONE) {
        return FALSE;
    }

    if (Bag_CanRemoveItem(battleBag->context->bag, battleBag->context->lastUsedItem, 1, battleBag->context->heapID) == FALSE
#ifdef SDK_PORT
        || PortBattlePocketMask(battleBag->context->lastUsedItem, Item_LoadParam(battleBag->context->lastUsedItem, ITEM_PARAM_BATTLE_POCKET, battleBag->context->heapID)) == 0
#endif
    ) {
        battleBag->context->lastUsedItem = ITEM_NONE;
        battleBag->context->lastUsedItemPocket = ITEM_BATTLE_CATEGORY_RECOVER_HP;
        return FALSE;
    }

    return TRUE;
}

void BattleBag_SetLastUsedPocket(BattleBag *battleBag)
{
    for (u32 i = 0; i < BATTLE_POCKET_SIZE; i++) {
        if (battleBag->context->lastUsedItem == battleBag->items[battleBag->currentBattlePocket][i].item) {
            battleBag->context->pocketCurrentPagePositions[battleBag->currentBattlePocket] = i % BATTLE_POCKET_ITEMS_PER_PAGE;
            battleBag->context->pocketCurrentPages[battleBag->currentBattlePocket] = i / BATTLE_POCKET_ITEMS_PER_PAGE;
            break;
        }
    }
}

void BattleBag_Init(BattleBag *battleBag)
{
    for (u32 i = 0; i < POCKET_MAX; i++) {
        u32 currentPocketSlot = 0;

        while (TRUE) {
            BagItem *bagItem = Bag_GetItemSlot(battleBag->context->bag, i, currentPocketSlot);

            if (bagItem == NULL) {
                break;
            }

            if (!(bagItem->item == ITEM_NONE || bagItem->quantity == 0)) {
                s32 bagItemBattlePocketMask = Item_LoadParam(bagItem->item, ITEM_PARAM_BATTLE_POCKET, battleBag->context->heapID);
#ifdef SDK_PORT
                bagItemBattlePocketMask = PortBattlePocketMask(bagItem->item, bagItemBattlePocketMask);
#endif

                for (u32 l = 0; l < BATTLE_POCKET_MAX; l++) {
                    if ((bagItemBattlePocketMask & (1 << l)) == FALSE) {
                        continue;
                    }

                    battleBag->items[sBattlePocketIndexes[l]][battleBag->numBattlePocketItems[sBattlePocketIndexes[l]]] = *bagItem;
                    battleBag->numBattlePocketItems[sBattlePocketIndexes[l]]++;
                }
            }

            currentPocketSlot++;
        }
    }

    for (u32 i = 0; i < BATTLE_POCKET_MAX; i++) {
        if (battleBag->numBattlePocketItems[i] == 0) {
            battleBag->numBattlePocketPages[i] = 0;
        } else {
            battleBag->numBattlePocketPages[i] = (battleBag->numBattlePocketItems[i] - 1) / BATTLE_POCKET_ITEMS_PER_PAGE;
        }

        if (battleBag->numBattlePocketPages[i] < battleBag->context->pocketCurrentPages[i]) {
            battleBag->context->pocketCurrentPages[i] = battleBag->numBattlePocketPages[i];
        }
    }
}

u16 BattleBag_GetItem(BattleBag *battleBag, u32 pagePosition)
{
    if (battleBag->items[battleBag->currentBattlePocket][battleBag->context->pocketCurrentPages[battleBag->currentBattlePocket] * BATTLE_POCKET_ITEMS_PER_PAGE + pagePosition].item != ITEM_NONE && battleBag->items[battleBag->currentBattlePocket][battleBag->context->pocketCurrentPages[battleBag->currentBattlePocket] * BATTLE_POCKET_ITEMS_PER_PAGE + pagePosition].quantity != 0) {
        return battleBag->items[battleBag->currentBattlePocket][battleBag->context->pocketCurrentPages[battleBag->currentBattlePocket] * BATTLE_POCKET_ITEMS_PER_PAGE + pagePosition].item;
    }

    return ITEM_NONE;
}

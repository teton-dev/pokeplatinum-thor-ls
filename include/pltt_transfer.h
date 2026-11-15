#ifndef POKEPLATINUM_PLTT_TRANSFER_H
#define POKEPLATINUM_PLTT_TRANSFER_H

#include <nnsys.h>

#include "constants/heap.h"

typedef struct PlttTransferTaskTemplate {
    NNSG2dPaletteData *data;
    NNS_G2D_VRAM_TYPE vramType;
    u32 plttIndex;
    u32 resourceID;
} PlttTransferTaskTemplate;

void PlttTransfer_Init(int capacity, enum HeapID heapID);
void PlttTransfer_MarkReservedSlots(u16 reservedMask, NNS_G2D_VRAM_TYPE vramType);
void PlttTransfer_Free(void);
#ifdef SDK_PORT
// count free palette slots in a row (from the top, away from the ones the screen's own sprites take first), marked
// used; the first, or -1 when there is no room
int PlttTransfer_PortReserveFreeSlots(int count, NNS_G2D_VRAM_TYPE vramType);
#endif
void PlttTransfer_Clear(void);
BOOL PlttTransfer_RequestWholeRange(const PlttTransferTaskTemplate *template);
BOOL PlttTransfer_RequestFreeSpace(const PlttTransferTaskTemplate *template);
void PlttTransfer_ReplacePlttData(int resourceID, NNSG2dPaletteData *data);
BOOL PlttTransfer_HasTask(int resourceID);
void PlttTransfer_ResetTask(int resourceID);
void PlttTransfer_ResetAllTasks(void);
NNSG2dImagePaletteProxy *PlttTransfer_GetPaletteProxy(int resourceID);
NNSG2dImagePaletteProxy *PlttTransfer_ToggleExtPalette(int resourceID, NNSG2dImageProxy *imageProxy);
u32 PlttTransfer_GetPlttOffset(const NNSG2dImagePaletteProxy *paletteProxy, NNS_G2D_VRAM_TYPE vramType);

#endif // POKEPLATINUM_PLTT_TRANSFER_H

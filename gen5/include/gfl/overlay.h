#ifndef POKEBW2_GFL_OVERLAY_H
#define POKEBW2_GFL_OVERLAY_H

// pokebw2's gfl/overlay.h for the port (Gen 5 battles): its code is all in the app, so there is nothing to load.
// An overlay's ID is its number instead of the address of its linker symbol (which a u32 can't hold here).

#include "types.h"

#include "gfl/heap.h"

#define OVERLAY_ID(n) ((u32)(n))
#define OVERLAY_NONE (-1)

void GFL_OvlManagerInit(HeapID heapId, u32 mainCount, u32 itcmCount, u32 dtcmCount);
BOOL GFL_OvlLoad(u32 overlayId);
void GFL_OvlUnload(u32 overlayId);

#endif // POKEBW2_GFL_OVERLAY_H

// GFL's overlay manager for the port (Gen 5 battles), in place of pokebw2's src/gfl/gf_overlay.c: BW2's code is
// compiled into the app, so an overlay is always loaded.

#include "types.h"

#include "gfl/overlay.h"

void GFL_OvlManagerInit(HeapID heapId, u32 mainCount, u32 itcmCount, u32 dtcmCount) {
    (void)heapId;
    (void)mainCount;
    (void)itcmCount;
    (void)dtcmCount;
}

BOOL GFL_OvlLoad(u32 overlayId) {
    (void)overlayId;
    return TRUE;
}

void GFL_OvlUnload(u32 overlayId) {
    (void)overlayId;
}

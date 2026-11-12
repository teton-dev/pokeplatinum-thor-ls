// What pokebw2's code calls in its own copy of the SDKs and libntr doesn't have (Gen 5 battles)

#include "types.h"

// The DS's extra 4 MB of debug RAM, which a retail DS doesn't have either: no heap. Milestone 3 gives BW2's GFL its
// memory from Platinum's
OSHeapHandle OS_CreateExtraHeap(OSArenaId arena) {
    (void)arena;
    return OS_HEAP_INVALID;
}

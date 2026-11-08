// Pointers through 32-bit values, for the Gen 5 battles on 64 bits
//
// BW2 passes some pointers through 32-bit slots that don't change type easily: the battle's event variables (s32, a
// handler's string), NitroSystem's animation callbacks (a u32 parameter) and the like. The patches give those slots
// a handle from here instead, and turn it back into the pointer where it's read. Handles are never 0, so NULL stays 0.

#include "types.h"

#include <stdio.h>

#define HANDLE_MAX 1024

static void *sPointers[HANDLE_MAX];
static u32 sNext;

u32 Gen5_PtrToU32(const void *pointer)
{
    if (pointer == NULL) {
        return 0;
    }
    for (u32 i = 0; i < HANDLE_MAX; i++) {
        if (sPointers[i] == pointer) {
            return i + 1;
        }
    }
    // A new one, over the oldest when they're all taken (handles live for a battle at most, and a battle uses few)
    const u32 i = sNext;
    sNext = (sNext + 1) % HANDLE_MAX;
    sPointers[i] = (void *)pointer;
    return i + 1;
}

void *Gen5_U32ToPtr(u32 handle)
{
    if (handle == 0) {
        return NULL;
    }
    if (handle > HANDLE_MAX) {
        printf("Gen 5: %#lx isn't a pointer's handle\n", (unsigned long)handle);
        return NULL;
    }
    return sPointers[handle - 1];
}

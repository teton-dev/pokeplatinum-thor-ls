#ifndef POKEPLATINUM_STRUCT_OV19_021DCD18_H
#define POKEPLATINUM_STRUCT_OV19_021DCD18_H

#include <nnsys.h>

#include "sprite.h"

typedef struct {
    Sprite *unk_00;
    NNSG2dImageProxy unk_04;
    u32 unk_28;
    u16 unk_2C;
    u16 unk_2E;
    u16 unk_30;
#ifdef SDK_PORT
    u8 portDead; // Nuzlocke mode: drawn in gray (its tiles remapped to the gray ramp palette)
    u8 padding_33;
#else
    u8 padding_32[2];
#endif
} UnkStruct_ov19_021DCD18;

#endif // POKEPLATINUM_STRUCT_OV19_021DCD18_H

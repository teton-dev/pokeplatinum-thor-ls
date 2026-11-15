// Gray icons for dead Pokémon. See include/port/nuzlocke_gray.h
#include "port/nuzlocke_gray.h"

#include "port/nuzlocke_mode.h"

#include "hardware_palette.h"
#include "pltt_transfer.h"

static int Gray(u16 color);

BOOL NuzlockeGray_IsDead(struct Pokemon *mon)
{
    return mon != NULL && NuzlockeMode_On() && NuzlockeMode_IsDead(mon);
}

BOOL NuzlockeGray_BoxIsDead(struct BoxPokemon *boxMon)
{
    return boxMon != NULL && NuzlockeMode_On() && NuzlockeMode_BoxIsDead(boxMon);
}

void NuzlockeGray_Colors(u16 *colors, int count)
{
    for (int i = 0; i < count; i++) {
        if (i % 16 == 0) {
            continue;
        }
        const int y = Gray(colors[i]);
        colors[i] = (colors[i] & 0x8000) | GX_RGB(y, y, y);
    }
}

void NuzlockeGray_IconPalettesTo(BOOL sub, int iconSlot, int graySlot)
{
    u16 *palettes = sub ? GetHardwareSubObjPaletteAddress() : GetHardwareMainObjPaletteAddress();
    u16 colors[16 * NUZLOCKE_GRAY_ICON_PALETTES];
    MI_CpuCopy16(palettes + iconSlot * 16, colors, sizeof(colors));
    NuzlockeGray_Colors(colors, 16 * NUZLOCKE_GRAY_ICON_PALETTES);
    MI_CpuCopy16(colors, palettes + graySlot * 16, sizeof(colors));
}

int NuzlockeGray_IconPalettes(BOOL sub, int iconSlot)
{
    const int graySlot = PlttTransfer_PortReserveFreeSlots(NUZLOCKE_GRAY_ICON_PALETTES, sub ? NNS_G2D_VRAM_TYPE_2DSUB : NNS_G2D_VRAM_TYPE_2DMAIN);
    if (graySlot >= 0) {
        NuzlockeGray_IconPalettesTo(sub, iconSlot, graySlot);
    }
    return graySlot;
}

// The gray a color becomes (0 to 31): its luminance, a little darker and flatter
static int Gray(u16 color)
{
    const int r = color & 0x1F, g = (color >> 5) & 0x1F, b = (color >> 10) & 0x1F;
    return 4 + ((r * 77 + g * 151 + b * 28) >> 8) * 3 / 4;
}

void NuzlockeGray_RampPalette(BOOL sub, int slot)
{
    u16 *palette = (sub ? GetHardwareSubObjPaletteAddress() : GetHardwareMainObjPaletteAddress()) + slot * 16;
    palette[0] = 0;
    for (int k = 1; k < 16; k++) {
        const int y = 4 + ((k - 1) * 31 / 14) * 3 / 4;
        palette[k] = GX_RGB(y, y, y);
    }
}

void NuzlockeGray_RemapTiles(u8 *tiles, u32 size, const u16 *colors)
{
    u8 map[16];
    map[0] = 0;
    for (int i = 1; i < 16; i++) {
        // Back from Gray() to the ramp's step: 4 + y * 3 / 4 for y = (k - 1) * 31 / 14
        const int y = (Gray(colors[i]) - 4) * 4 / 3;
        int k = 1 + (y * 14 + 15) / 31;
        map[i] = k < 1 ? 1 : k > 15 ? 15 : k;
    }
    for (u32 i = 0; i < size; i++) {
        tiles[i] = map[tiles[i] & 0xF] | (map[tiles[i] >> 4] << 4);
    }
}

const u16 *NuzlockeGray_ObjPalette(BOOL sub, int slot)
{
    return (sub ? GetHardwareSubObjPaletteAddress() : GetHardwareMainObjPaletteAddress()) + slot * 16;
}

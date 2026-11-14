#ifndef PORT_NUZLOCKE_GRAY_H
#define PORT_NUZLOCKE_GRAY_H

// Nuzlocke mode: dead Pokémon are drawn in gray. Pokémon icons share three palettes on a screen, so a screen gets
// gray copies of them in free palette slots, and a dead Pokémon's icon uses those.

#include <nitro.h>

#define NUZLOCKE_GRAY_ICON_PALETTES 3

struct Pokemon;
struct BoxPokemon;
// Shown gray: dead, while the save is a Nuzlocke run (a dropped run's dead flags no longer count)
BOOL NuzlockeGray_IsDead(struct Pokemon *mon);
BOOL NuzlockeGray_BoxIsDead(struct BoxPokemon *boxMon);

// count colors made gray in place (color 0 of each 16, the transparent one, as it is)
void NuzlockeGray_Colors(u16 *colors, int count);

// Gray copies of the icon palettes at OBJ palette slot iconSlot (three in a row) of the main (or sub) screen, into
// three free slots from the palette manager. The first gray slot, or -1 (no room: the icons stay in color).
int NuzlockeGray_IconPalettes(BOOL sub, int iconSlot);

// Gray copies into slots the screen chose itself
void NuzlockeGray_IconPalettesTo(BOOL sub, int iconSlot, int graySlot);

// A screen with a single free palette slot: a ramp of 15 grays there, and a dead Pokémon's 4bpp tiles remapped to it
// (each pixel to the gray of its color's brightness, colors being the 16 of the palette it was drawn for)
void NuzlockeGray_RampPalette(BOOL sub, int slot);
void NuzlockeGray_RemapTiles(u8 *tiles, u32 size, const u16 *colors);
// The colors of an OBJ palette slot
const u16 *NuzlockeGray_ObjPalette(BOOL sub, int slot);

#endif // PORT_NUZLOCKE_GRAY_H

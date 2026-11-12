// The Team Galactic arena for the Gen 5 battles (teton-dev fork): BW2's Champion room (field 18, the dark sky with the
// light streaks and the gold rings) with the streaks in Crobat purple and Team Galactic blue only, and a field of
// gleaming stars in the sky. A Galactic battle is in it (src/port/gen5_battle.c, ArenaFromDTO).
//
// Nothing of BW2's is kept in the repository: the arena is made from the player's own ROM at the first battle (see
// Gen5Data_PrepareGalacticArena in gen5_data.c), by changing a copy of the room's model resource (a BMD0 with its
// textures). The room's moving streaks are the A3I5 texture "batt_field21n" (white shapes, the alpha makes the glow)
// that BW2's animation scrolls around the room, on ribbons (the shapes "polygon3" and "polygon4": strips of triangles
// with a dark edge and a coloured one) whose vertex colours give each streak its rainbow; the sky has "batt_sky21n",
// a few dim dots. Here:
//   - the streaks' colour: each ribbon (a BEGIN_VTXS to END_VTXS of the display list) gets Crobat purple or Team
//     Galactic blue, alternating, in place of its colours; each vertex keeps its brightness, so the fade along it stays;
//   - the stars: the sky's texture is drawn again from nothing, with sparkles (a bright centre and four arms that
//     fade out, some with faint diagonals) and a scatter of fainter points, in colours 3 (ice) and 4 (white) of the
//     palette the two textures share ("batt_field21n_pl"; colour 1, the streaks' white, is kept).

#include "port/gen5_galactic.h"

#include <string.h>

// 15-bit colours: the stars' ice and white (the ribbons' purple and blue are in RecolorRibbons, 5 bits a channel:
// Crobat's 0x8E 0x4F 0xC4 and Team Galactic's hair 0x4F 0xB4 0xEC)
#define RGB15(r, g, b) ((u16)((((r) >> 3) & 31) | ((((g) >> 3) & 31) << 5) | ((((b) >> 3) & 31) << 10)))
#define COLOR_ICE    RGB15(0xE4, 0xF2, 0xFF)
#define COLOR_WHITE  RGB15(0xFF, 0xFF, 0xFF)

// The planets (Jupiter and Saturn, after the Commanders), colours 2, 5, 6 and 7 of the stars' palette
#define COLOR_JUPITER_BASE  RGB15(0xDA, 0x9A, 0x5C)
#define COLOR_JUPITER_BAND  RGB15(0x9C, 0x5A, 0x3A)
#define COLOR_CREAM         RGB15(0xF4, 0xE2, 0xBA)
#define COLOR_SATURN        RGB15(0xE6, 0xCB, 0x82)

#define TEX_WIDTH  128
#define TEX_HEIGHT 64

static u16 Read16(const u8 *p)
{
    return p[0] | (p[1] << 8);
}

static u32 Read32(const u8 *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

// An entry of a NITRO dictionary at dict (from the start of the section), by name: its data (entrySize bytes). The
// dictionary is a header of 8 bytes, a tree of count + 1 nodes of 4 bytes, the entries' size and their block's, the
// entries, then their names in 16 bytes each
static const u8 *DictFind(const u8 *section, u32 sectionSize, u32 dict, const char *name, u32 *entrySize)
{
    if (dict + 8 > sectionSize) {
        return NULL;
    }
    const u32 count = section[dict + 1];
    const u32 sizeAt = dict + 8 + 4 * (count + 1);
    if (sizeAt + 4 > sectionSize) {
        return NULL;
    }
    const u32 size = Read16(section + sizeAt);
    const u32 entries = sizeAt + 4;
    const u32 names = entries + count * size;
    if (names + count * 16 > sectionSize) {
        return NULL;
    }
    for (u32 i = 0; i < count; i++) {
        if (strncmp((const char *)section + names + 16 * i, name, 16) == 0) {
            *entrySize = size;
            return section + entries + i * size;
        }
    }
    return NULL;
}

// The texture's pixels (A3I5, TEX_WIDTH x TEX_HEIGHT) and the palette (8 colours) a model resource has for them
static BOOL Locate(u8 *res, u32 size, const char *texture, const char *palette, u8 **pixels, u16 **colors)
{
    if (size < 0x20 || memcmp(res, "BMD0", 4) != 0) {
        return FALSE;
    }
    const u32 blocks = Read16(res + 0x0E);
    u8 *tex = NULL;
    u32 texSize = 0;
    for (u32 i = 0; i < blocks && 0x10 + 4 * i + 4 <= size; i++) {
        const u32 offset = Read32(res + 0x10 + 4 * i);
        if (offset + 0x40 <= size && memcmp(res + offset, "TEX0", 4) == 0) {
            tex = res + offset;
            texSize = size - offset;
            if (Read32(tex + 4) < texSize) {
                texSize = Read32(tex + 4);
            }
        }
    }
    if (tex == NULL || texSize < 0x40) {
        return FALSE;
    }
    u32 entrySize = 0;
    const u8 *info = DictFind(tex, texSize, Read16(tex + 0x0E), texture, &entrySize);
    if (info == NULL || entrySize != 8) {
        return FALSE;
    }
    const u32 param = Read16(info + 2);
    // A3I5 (format 1), 128 x 64
    if (((param >> 10) & 7) != 1 || (8u << ((param >> 4) & 7)) != TEX_WIDTH || (8u << ((param >> 7) & 7)) != TEX_HEIGHT) {
        return FALSE;
    }
    const u32 pixelsAt = Read32(tex + 0x14) + ((u32)Read16(info) << 3);
    const u8 *palInfo = DictFind(tex, texSize, Read32(tex + 0x34), palette, &entrySize);
    if (palInfo == NULL || entrySize != 4) {
        return FALSE;
    }
    const u32 colorsAt = Read32(tex + 0x38) + ((u32)Read16(palInfo) << 3);
    if (pixelsAt + TEX_WIDTH * TEX_HEIGHT > texSize || colorsAt + 16 > texSize) {
        return FALSE;
    }
    *pixels = tex + pixelsAt;
    *colors = (u16 *)(tex + colorsAt);
    return TRUE;
}

static void Put16(u16 *where, u16 value)
{
    u8 *p = (u8 *)where;
    p[0] = value & 0xFF;
    p[1] = value >> 8;
}

// The display lists' commands (GX geometry commands) and how many words of parameters each takes
static int CommandWords(u8 command)
{
    switch (command) {
    case 0x10: case 0x12: case 0x13: case 0x14: case 0x20: case 0x21: case 0x22: case 0x24: case 0x25: case 0x26:
    case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x30: case 0x31: case 0x32: case 0x33: case 0x40:
    case 0x50: case 0x60: case 0x72:
        return 1;
    case 0x23: case 0x71:
        return 2;
    case 0x70:
        return 3;
    case 0x11: case 0x15: case 0x41:
        return 0;
    case 0x16:
        return 16;
    case 0x17:
        return 12;
    case 0x18: case 0x19:
        return 48;
    case 0x1A:
        return 36;
    case 0x1B: case 0x1C:
        return 12;
    case 0x34:
        return 32;
    default:
        return -1;
    }
}

// The ribbons of a shape's display list: the vertex colours (command 0x20) in each BEGIN_VTXS (0x40) to END_VTXS (0x41)
// become Crobat purple or Galactic blue (by the ribbon's number, from *ribbon on), at the brightness they had. The dark
// edge's colour (all its channels under 8) is kept
static void RecolorRibbons(u8 *list, u32 size, int *ribbon)
{
    static const int hues[2][3] = { { 17, 9, 24 }, { 9, 22, 29 } }; // purple, blue: 5 bits a channel
    u32 at = 0;
    while (at + 4 <= size) {
        u8 commands[4];
        memcpy(commands, list + at, 4);
        at += 4;
        for (int c = 0; c < 4; c++) {
            if (commands[c] == 0) {
                continue;
            }
            const int words = CommandWords(commands[c]);
            if (words < 0 || at + 4 * words > size) {
                return;
            }
            if (commands[c] == 0x40) {
                (*ribbon)++;
            } else if (commands[c] == 0x20) {
                const u32 color = Read32(list + at);
                const int r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
                int bright = r > g ? r : g;
                bright = bright > b ? bright : b;
                if (bright >= 8) {
                    // A little brighter than it was, up to the hue's own
                    int gain = bright * 13 / 10;
                    gain = gain > 31 ? 31 : gain;
                    const int *hue = hues[*ribbon & 1];
                    const u32 out = (hue[0] * gain / 31) | ((hue[1] * gain / 31) << 5) | ((hue[2] * gain / 31) << 10);
                    list[at] = out & 0xFF;
                    list[at + 1] = (color & 0x8000) ? ((out >> 8) | 0x80) : (out >> 8);
                }
            }
            at += 4 * words;
        }
    }
}

// The sky's stars' shape: its vertex colours (blues, dim toward one edge) become grey of a higher brightness, so the stars
// and planets keep their own colours and shine
static void WhitenSky(u8 *list, u32 size)
{
    u32 at = 0;
    while (at + 4 <= size) {
        u8 commands[4];
        memcpy(commands, list + at, 4);
        at += 4;
        for (int c = 0; c < 4; c++) {
            if (commands[c] == 0) {
                continue;
            }
            const int words = CommandWords(commands[c]);
            if (words < 0 || at + 4 * words > size) {
                return;
            }
            if (commands[c] == 0x20) {
                const u32 color = Read32(list + at);
                const int r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
                int bright = r > g ? r : g;
                bright = bright > b ? bright : b;
                int gain = bright * 2 + 4;
                gain = gain > 31 ? 31 : gain;
                const u32 out = gain | (gain << 5) | (gain << 10);
                list[at] = out & 0xFF;
                list[at + 1] = out >> 8;
            }
            at += 4 * words;
        }
    }
}

// The model's shape named name: its display list
static BOOL FindShape(u8 *res, u32 size, const char *name, u8 **list, u32 *listSize)
{
    const u32 blocks = Read16(res + 0x0E);
    for (u32 i = 0; i < blocks && 0x10 + 4 * i + 4 <= size; i++) {
        const u32 offset = Read32(res + 0x10 + 4 * i);
        if (offset + 0x20 > size || memcmp(res + offset, "MDL0", 4) != 0) {
            continue;
        }
        u8 *mdl = res + offset;
        const u32 mdlSize = size - offset;
        u32 entrySize = 0;
        const u8 *model = mdlSize > 0x40 ? mdl + 8 : NULL;
        if (model == NULL || model[1] < 1) {
            return FALSE;
        }
        // The first (only) model: the dictionary's first entry, then its shapes' dictionary
        const u32 count = model[1];
        const u32 entries = 8 + 8 + 4 * (count + 1) + 4; // the dictionary starts at 8
        if (entries + 4 > mdlSize) {
            return FALSE;
        }
        const u32 start = Read32(mdl + entries);
        if (start + 16 > mdlSize) {
            return FALSE;
        }
        const u32 shapes = start + Read32(mdl + start + 12);
        if (shapes + 8 > mdlSize) {
            return FALSE;
        }
        const u8 *shape = DictFind(mdl, mdlSize, shapes, name, &entrySize);
        if (shape == NULL || entrySize != 4) {
            return FALSE;
        }
        const u32 header = shapes + Read32(shape);
        if (header + 16 > mdlSize) {
            return FALSE;
        }
        const u32 listAt = header + Read32(mdl + header + 8);
        const u32 length = Read32(mdl + header + 12);
        if (listAt + length > mdlSize) {
            return FALSE;
        }
        *list = mdl + listAt;
        *listSize = length;
        return TRUE;
    }
    return FALSE;
}

static void Plot(u8 *pixels, int x, int y, int color, int alpha)
{
    x = ((x % TEX_WIDTH) + TEX_WIDTH) % TEX_WIDTH; // the sky wraps around the room
    if (y < 0 || y >= TEX_HEIGHT || alpha <= 0) {
        return;
    }
    u8 *p = &pixels[y * TEX_WIDTH + x];
    if ((*p >> 5) >= alpha) {
        return;
    }
    *p = (u8)((alpha << 5) | color);
}

// A sparkle: a bright centre and four arms that fade out (armX across, armY up and down), faint diagonals when big
static void Sparkle(u8 *pixels, int cx, int cy, int armX, int armY, BOOL diagonals)
{
    Plot(pixels, cx, cy, 4, 7);
    for (int k = 1; k <= armX; k++) {
        const int alpha = 7 - (4 * (k - 1)) / armX;
        Plot(pixels, cx - k, cy, k == 1 ? 4 : 3, alpha);
        Plot(pixels, cx + k, cy, k == 1 ? 4 : 3, alpha);
    }
    for (int k = 1; k <= armY; k++) {
        const int alpha = 7 - (4 * (k - 1)) / armY;
        Plot(pixels, cx, cy - k, k == 1 ? 4 : 3, alpha);
        Plot(pixels, cx, cy + k, k == 1 ? 4 : 3, alpha);
    }
    if (diagonals) {
        Plot(pixels, cx - 1, cy - 1, 3, 5);
        Plot(pixels, cx + 1, cy - 1, 3, 5);
        Plot(pixels, cx - 1, cy + 1, 3, 5);
        Plot(pixels, cx + 1, cy + 1, 3, 5);
    }
}

static void Set(u8 *pixels, int x, int y, int color, int alpha)
{
    x = ((x % TEX_WIDTH) + TEX_WIDTH) % TEX_WIDTH;
    if (y >= 0 && y < TEX_HEIGHT) {
        pixels[y * TEX_WIDTH + x] = (u8)((alpha << 5) | color);
    }
}

// The sky texture is stretched about 1.8 times wider than it is high around the room, so the planets are drawn narrow
// (about 0.55 wide to 1 high) to look round
// Jupiter: bands (orange, brown, cream) across a disc 7 wide and 11 high, edge pixels dimmer, the Great Red Spot low
static void Jupiter(u8 *pixels, int cx, int cy)
{
    static const u8 bands[11] = { 6, 2, 5, 2, 6, 2, 5, 2, 6, 2, 5 };
    for (int y = -5; y <= 5; y++) {
        for (int x = -3; x <= 3; x++) {
            const int d = x * x * 10 + y * y * 4;
            if (d > 100) {
                continue;
            }
            int color = bands[y + 5];
            if (x >= 0 && x <= 1 && y >= 2 && y <= 3) {
                color = 5; // the Spot
            }
            Set(pixels, cx + x, cy + y, color, d > 80 ? 5 : 7);
        }
    }
}

// Saturn: a disc 5 wide and 7 high with a ring across it, 11 wide: behind the disc at the sides, in front of it below
static void Saturn(u8 *pixels, int cx, int cy)
{
    for (int y = -3; y <= 3; y++) {
        for (int x = -2; x <= 2; x++) {
            if (x * x * 25 + y * y * 11 <= 100) {
                Set(pixels, cx + x, cy + y, (y == 0 || y == 2) ? 6 : 7, 7);
            }
        }
    }
    for (int x = -5; x <= 5; x++) {
        Set(pixels, cx + x, cy + 1, 6, 7);
        if (x <= -3 || x >= 3) {
            Set(pixels, cx + x, cy, 5, 7);
        }
    }
}

// The stars: sparkles of a few sizes at fixed places (so every battle has the same sky), and a scatter of points
static void DrawStars(u8 *pixels)
{
    memset(pixels, 0, TEX_WIDTH * TEX_HEIGHT);
    static const struct {
        u8 x, y, armX, armY, diagonals;
    } sparkles[] = {
        { 6, 9, 5, 4, 1 },
        { 22, 31, 3, 3, 0 },
        { 54, 5, 4, 3, 1 },
        { 47, 44, 3, 2, 0 },
        { 58, 22, 6, 5, 1 },
        { 71, 8, 3, 3, 0 },
        { 80, 36, 5, 4, 1 },
        { 106, 9, 3, 2, 0 },
        { 104, 49, 4, 3, 1 },
        { 116, 26, 6, 5, 1 },
        { 120, 6, 3, 3, 0 },
        { 14, 53, 3, 2, 0 },
        { 64, 56, 4, 3, 0 },
        { 88, 3, 2, 2, 0 },
    };
    // Points first, so the sparkles are drawn over them
    u32 seed = 0x5EED5EEDu;
    for (int i = 0; i < 70; i++) {
        seed = seed * 1664525u + 1013904223u;
        const int x = (seed >> 8) % TEX_WIDTH;
        seed = seed * 1664525u + 1013904223u;
        const int y = (seed >> 8) % (TEX_HEIGHT - 4);
        seed = seed * 1664525u + 1013904223u;
        Plot(pixels, x, y, (seed >> 20) & 1 ? 4 : 3, 3 + (int)((seed >> 12) % 4));
    }
    for (u32 i = 0; i < sizeof(sparkles) / sizeof(sparkles[0]); i++) {
        Sparkle(pixels, sparkles[i].x, sparkles[i].y, sparkles[i].armX, sparkles[i].armY, sparkles[i].diagonals);
    }
    // A clear patch for each planet, then the planets
    Jupiter(pixels, 40, 14);
    Saturn(pixels, 92, 14);
}

// The colours of a palette of the model's textures by name, up to the end of the texture block
static BOOL FindPalette(u8 *res, u32 size, const char *name, u16 **colors, u32 *count)
{
    const u32 blocks = Read16(res + 0x0E);
    for (u32 i = 0; i < blocks && 0x10 + 4 * i + 4 <= size; i++) {
        const u32 offset = Read32(res + 0x10 + 4 * i);
        if (offset + 0x40 > size || memcmp(res + offset, "TEX0", 4) != 0) {
            continue;
        }
        u8 *tex = res + offset;
        u32 texSize = size - offset;
        u32 entrySize = 0;
        const u8 *info = DictFind(tex, texSize, Read32(tex + 0x34), name, &entrySize);
        if (info == NULL || entrySize != 4) {
            return FALSE;
        }
        const u32 at = Read32(tex + 0x38) + ((u32)Read16(info) << 3);
        if (at >= texSize) {
            return FALSE;
        }
        *colors = (u16 *)(tex + at);
        *count = (texSize - at) / 2;
        return TRUE;
    }
    return FALSE;
}

// The background's levels (the sky's and the ground's 256-colour textures): each channel's darkest value over both
// palettes becomes black and the brightest stays, the ones between fall off with the square (darker mids). The room's darkest part (the top of
// the sky) reaches black and the brightest is as it was
static BOOL DarkenBackground(u8 *res, u32 size)
{
    static const char *const names[2] = { "batt_field21_pl", "batt_sky21_pl" };
    u16 *palettes[2];
    u32 counts[2];
    int low[3] = { 31, 31, 31 };
    int high[3] = { 0, 0, 0 };
    for (int p = 0; p < 2; p++) {
        if (!FindPalette(res, size, names[p], &palettes[p], &counts[p])) {
            return FALSE;
        }
        if (p == 0 && counts[0] > 80) {
            counts[0] = 80; // the ground's palette is 160 bytes, the sky's follows it
        }
        for (u32 i = 0; i < counts[p]; i++) {
            const u16 c = Read16((const u8 *)&palettes[p][i]);
            for (int ch = 0; ch < 3; ch++) {
                const int v = (c >> (5 * ch)) & 31;
                low[ch] = v < low[ch] ? v : low[ch];
                high[ch] = v > high[ch] ? v : high[ch];
            }
        }
    }
    for (int p = 0; p < 2; p++) {
        for (u32 i = 0; i < counts[p]; i++) {
            const u16 c = Read16((const u8 *)&palettes[p][i]);
            u16 out = 0;
            for (int ch = 0; ch < 3; ch++) {
                const int v = (c >> (5 * ch)) & 31;
                const int range = high[ch] - low[ch];
                // Stretched to 0..range, then squared: the mid colours fall toward black, the brightest stays
                const int t = range > 0 ? v - low[ch] : 0;
                const int n = range > 0 ? (t * t * high[ch] + range * range / 2) / (range * range) : v;
                out |= (u16)(n << (5 * ch));
            }
            Put16(&palettes[p][i], out);
        }
    }
    return TRUE;
}

BOOL Gen5Galactic_Make(u8 *res, u32 size)
{
    u8 *streaks, *stars;
    u16 *colors, *skyColors;
    if (!Locate(res, size, "batt_field21n", "batt_field21n_pl", &streaks, &colors)
        || !Locate(res, size, "batt_sky21n", "batt_sky21n_pl", &stars, &skyColors) || colors != skyColors) {
        return FALSE;
    }
    u8 *list;
    u32 listSize;
    int ribbon = 0;
    if (!FindShape(res, size, "polygon3", &list, &listSize)) {
        return FALSE;
    }
    RecolorRibbons(list, listSize, &ribbon);
    if (!FindShape(res, size, "polygon4", &list, &listSize)) {
        return FALSE;
    }
    RecolorRibbons(list, listSize, &ribbon);
    (void)streaks;
    if (!FindShape(res, size, "polygon2", &list, &listSize)) {
        return FALSE;
    }
    WhitenSky(list, listSize);
    if (!DarkenBackground(res, size)) {
        return FALSE;
    }
    DrawStars(stars);
    Put16(&colors[0], 0);
    Put16(&colors[1], COLOR_WHITE); // the streaks', as BW2's: the ribbons' vertex colours give their hue
    Put16(&colors[2], COLOR_JUPITER_BASE);
    Put16(&colors[3], COLOR_ICE);
    Put16(&colors[4], COLOR_WHITE);
    Put16(&colors[5], COLOR_JUPITER_BAND);
    Put16(&colors[6], COLOR_CREAM);
    Put16(&colors[7], COLOR_SATURN);
    return TRUE;
}

// The platforms: BW2's Champion room's two grounds are one model ("batt_stage21": a gold ring on cream). Its palette
// (all but the 4 colours of the shadow's "batt_bk21_pl") becomes dark indigo-steel: each colour's brightness, squared,
// times a dark tint
BOOL Gen5Galactic_MakeStage(u8 *res, u32 size)
{
    u16 *colors;
    u32 count;
    if (!FindPalette(res, size, "batt_stage21_pl", &colors, &count)) {
        return FALSE;
    }
    static const int tint[3] = { 92, 94, 140 }; // the brightest colour's: dark indigo
    for (u32 i = 0; i < count && i < 256; i++) {
        const u16 c = Read16((const u8 *)&colors[i]);
        const int r = (c & 31) * 255 / 31, g = ((c >> 5) & 31) * 255 / 31, b = ((c >> 10) & 31) * 255 / 31;
        const int luma = (r * 3 + g * 6 + b) / 10; // 0..255
        const int level = luma * luma / 255;
        Put16(&colors[i], RGB15(tint[0] * level / 255, tint[1] * level / 255, tint[2] * level / 255));
    }
    return TRUE;
}

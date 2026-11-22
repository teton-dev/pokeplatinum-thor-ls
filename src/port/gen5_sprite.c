// Gen 5 pictures for the Gear's Nuzlocke status (port-common's PortGame_GearGen5* hooks, nuzlocke.h): a Pokémon's
// front sprite from the player's Black 2 or White 2 ROM, once it is imported (Settings > Battles) and Gen 5 battles are
// on. They are decoded from the imported files (a/0/0/4, BW2's Pokémon graphics archive) by plain file access, as
// src/port/gear_sprites.c does for the game's own: a species' files are at species * 20, the front characters in file 0
// (the female's in 1, when it has any), the palettes at 18 (normal) and 19 (shiny).
//   - the characters are LZ11-compressed (an NCGR; its 0x1200 bytes of 4-bit pixels are four OBJs' characters one
//     after the other), made one image 12 characters wide as pokebw2's PokeGra_CellCharsToImage does
//   - the palette is an NCLR of 16 colors, color 0 transparent
// The picture is 96x96 RGBA (R, G, B, A in byte order). Called from the Gear's thread only.
#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/gen5_rom.h"
#include "port/nuzlocke.h"
#include "port/sim_config_prj.h"

#define POKEGRA_PATH "/a/0/0/4"
#define POKEGRA_FILE_COUNT 20
#define POKEGRA_FILE_PALETTE 18
#define POKEGRA_CHARS_SIZE 0x1200
#define NCGR_DATA 0x30 // an NCGR's characters start here (its header, then RAHC's, 0x18 bytes)
#define NCLR_DATA 0x28
#define FRONT_SIZE 96
#define SPECIES_MAX 493

typedef struct Narc {
    FILE *file;
    char dir[512];
    u32 count;
    u32 *start; // of each member, from the file's start
    u32 *end;
} Narc;

static Narc sNarc;

static u32 Le32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void CloseNarc(void)
{
    if (sNarc.file != NULL) {
        fclose(sNarc.file);
    }
    free(sNarc.start);
    free(sNarc.end);
    memset(&sNarc, 0, sizeof(sNarc));
}

// The archive of the imported ROM's folder (re-opened when the folder changes)
static BOOL OpenNarc(void)
{
    const char *dir = Gen5Rom_Dir();
    if (sNarc.file != NULL && strcmp(sNarc.dir, dir) == 0) {
        return TRUE;
    }
    CloseNarc();
    char path[600];
    snprintf(path, sizeof(path), "%s" POKEGRA_PATH, dir);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return FALSE;
    }
    u8 header[0x1C];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, "NARC", 4) != 0
        || memcmp(header + 0x10, "BTAF", 4) != 0) {
        fclose(file);
        return FALSE;
    }
    const u32 tableSize = Le32(header + 0x14);
    const u32 count = Le32(header + 0x18);
    u8 *table = malloc((size_t)count * 8);
    u32 *start = malloc(sizeof(u32) * count);
    u32 *end = malloc(sizeof(u32) * count);
    u8 sectionHeader[8];
    // BTAF, then BTNF (size at +4), then GMIF whose data starts 8 bytes in
    u32 offset = 0x10 + tableSize;
    BOOL ok = table != NULL && start != NULL && end != NULL
        && fseek(file, 0x1C, SEEK_SET) == 0 && fread(table, 1, (size_t)count * 8, file) == (size_t)count * 8
        && fseek(file, (long)offset, SEEK_SET) == 0 && fread(sectionHeader, 1, 8, file) == 8
        && memcmp(sectionHeader, "BTNF", 4) == 0;
    if (ok) {
        offset += Le32(sectionHeader + 4);
        ok = fseek(file, (long)offset, SEEK_SET) == 0 && fread(sectionHeader, 1, 8, file) == 8
            && memcmp(sectionHeader, "GMIF", 4) == 0;
    }
    if (!ok) {
        free(table);
        free(start);
        free(end);
        fclose(file);
        return FALSE;
    }
    for (u32 i = 0; i < count; i++) {
        start[i] = offset + 8 + Le32(table + i * 8);
        end[i] = offset + 8 + Le32(table + i * 8 + 4);
    }
    free(table);
    sNarc.file = file;
    sNarc.count = count;
    sNarc.start = start;
    sNarc.end = end;
    snprintf(sNarc.dir, sizeof(sNarc.dir), "%s", dir);
    return TRUE;
}

// A member's bytes (malloc'd), or NULL
static u8 *ReadMember(u32 index, u32 *size)
{
    if (index >= sNarc.count || sNarc.end[index] <= sNarc.start[index]) {
        return NULL;
    }
    *size = sNarc.end[index] - sNarc.start[index];
    u8 *data = malloc(*size);
    if (data == NULL) {
        return NULL;
    }
    if (fseek(sNarc.file, (long)sNarc.start[index], SEEK_SET) != 0 || fread(data, 1, *size, sNarc.file) != *size) {
        free(data);
        return NULL;
    }
    return data;
}

// LZ11 (the DS's extended LZ77: type 0x11, 24-bit size); into out of outSize bytes
static BOOL Lz11(const u8 *src, u32 srcSize, u8 *out, u32 outSize)
{
    if (srcSize < 4 || src[0] != 0x11) {
        return FALSE;
    }
    const u32 size = (u32)src[1] | ((u32)src[2] << 8) | ((u32)src[3] << 16);
    if (size < outSize) {
        return FALSE;
    }
    u32 in = 4, produced = 0;
    while (produced < outSize && in < srcSize) {
        const u8 flags = src[in++];
        for (int bit = 0; bit < 8 && produced < outSize; bit++) {
            if (!(flags & (0x80 >> bit))) {
                if (in >= srcSize) {
                    return FALSE;
                }
                out[produced++] = src[in++];
                continue;
            }
            if (in + 2 > srcSize) {
                return FALSE;
            }
            const u8 b0 = src[in];
            u32 length, distance;
            switch (b0 >> 4) {
            case 0:
                if (in + 3 > srcSize) {
                    return FALSE;
                }
                length = ((((u32)b0 & 0xF) << 4) | (src[in + 1] >> 4)) + 0x11;
                distance = ((((u32)src[in + 1] & 0xF) << 8) | src[in + 2]) + 1;
                in += 3;
                break;
            case 1:
                if (in + 4 > srcSize) {
                    return FALSE;
                }
                length = ((((u32)b0 & 0xF) << 12) | ((u32)src[in + 1] << 4) | (src[in + 2] >> 4)) + 0x111;
                distance = ((((u32)src[in + 2] & 0xF) << 8) | src[in + 3]) + 1;
                in += 4;
                break;
            default:
                length = (b0 >> 4) + 1;
                distance = ((((u32)b0 & 0xF) << 8) | src[in + 1]) + 1;
                in += 2;
                break;
            }
            if (distance > produced) {
                return FALSE;
            }
            for (u32 i = 0; i < length && produced < outSize; i++) {
                out[produced] = out[produced - distance];
                produced++;
            }
        }
    }
    return produced == outSize;
}

BOOL PortGame_GearGen5Sprites(void)
{
    return SIM_Config_prj_GetConfig()->gen5Battles && Gen5Rom_GameName() != NULL;
}

BOOL PortGame_GearGen5Front(u16 species, u8 form, BOOL female, BOOL shiny, u32 *rgba)
{
    (void)form; // the forms' own sprites aren't looked for: a form gets the species'
    if (species == 0 || species > SPECIES_MAX || !PortGame_GearGen5Sprites() || !OpenNarc()) {
        return FALSE;
    }
    const u32 base = (u32)species * POKEGRA_FILE_COUNT;
    u32 size = 0;
    u8 *chars = female ? ReadMember(base + 1, &size) : NULL;
    if (chars == NULL) {
        chars = ReadMember(base, &size);
    }
    u32 paletteSize = 0;
    u8 *palette = ReadMember(base + POKEGRA_FILE_PALETTE + (shiny ? 1 : 0), &paletteSize);
    static u8 raw[NCGR_DATA + POKEGRA_CHARS_SIZE];
    u8 image[POKEGRA_CHARS_SIZE];
    BOOL ok = chars != NULL && palette != NULL && paletteSize >= NCLR_DATA + 32
        && Lz11(chars, size, raw, sizeof(raw)) && memcmp(raw, "RGCN", 4) == 0;
    free(chars);
    if (!ok) {
        free(palette);
        return FALSE;
    }

    // The four OBJs' characters one after the other become one image, 12 characters wide (PokeGra_CellCharsToImage)
    const u8 *source = raw + NCGR_DATA;
    u32 offset = 0;
    memcpy(image, source, sizeof(image));
    for (int i = 0; i < 8; i++, offset += 0x100) {
        memcpy(image + i * 0x180, source + offset, 0x100);
    }
    for (int i = 0; i < 8; i++, offset += 0x80) {
        memcpy(image + (i * 12 + 8) * 32, source + offset, 0x80);
    }
    for (int i = 0; i < 4; i++, offset += 0x100) {
        memcpy(image + i * 0x180 + 0xC00, source + offset, 0x100);
    }
    for (int i = 0; i < 4; i++, offset += 0x80) {
        memcpy(image + (i * 12 + 104) * 32, source + offset, 0x80);
    }

    u32 colors[16];
    for (int i = 0; i < 16; i++) {
        const u32 value = (u32)palette[NCLR_DATA + i * 2] | ((u32)palette[NCLR_DATA + i * 2 + 1] << 8);
        const u32 r = (value & 31) * 255 / 31, g = ((value >> 5) & 31) * 255 / 31, b = ((value >> 10) & 31) * 255 / 31;
        colors[i] = i == 0 ? 0 : (r | (g << 8) | (b << 16) | 0xFF000000u);
    }
    free(palette);
    for (int y = 0; y < FRONT_SIZE; y++) {
        for (int x = 0; x < FRONT_SIZE; x++) {
            const int pos = ((x % 8) + (y % 8) * 8 + (x / 8 + (y / 8) * 12) * 64) / 2;
            const u8 value = image[pos];
            rgba[y * FRONT_SIZE + x] = colors[(x & 1) ? (value >> 4) : (value & 15)];
        }
    }
    return TRUE;
}

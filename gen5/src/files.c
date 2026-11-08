// BW2's archives from the Black 2 or White 2 files the port extracted (src/port/gen5_rom.cpp), for the Gen 5 battles

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "constants/arc.h"
#include "gfl/arc.h"
#include "gen5.h"

// The archives BW2 names, a/0/0/0 to a/3/0/7: the length of gfl_use.c's NARC_PATH_LIST (its ARC_PATH_COUNT)
#define ARC_COUNT 308
#define PATH_MAX_LENGTH 512

static char sPaths[ARC_COUNT][PATH_MAX_LENGTH];
static const char *sPathTable[ARC_COUNT];

// From the current folder to dir: libntr opens a game's files by paths from the folder of the game being played
// (the data folder, or carts/<game>/ in it), and only relative ones
static BOOL RelativePath(const char *dir, char *out, size_t size)
{
    char cwd[PATH_MAX_LENGTH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return FALSE;
    }
    // The longest common folder, then up from the current folder to it and down to dir
    size_t common = 0;
    for (size_t i = 0; cwd[i] != '\0' && cwd[i] == dir[i]; i++) {
        if (cwd[i] == '/' || cwd[i + 1] == '\0') {
            common = cwd[i] == '/' ? i : i + 1;
        }
    }
    if (dir[common] != '/' && dir[common] != '\0') {
        return FALSE;
    }
    out[0] = '\0';
    for (const char *c = cwd + common; *c != '\0'; c++) {
        if (*c == '/') {
            strncat(out, "../", size - strlen(out) - 1);
        }
    }
    const char *down = dir[common] == '/' ? dir + common + 1 : dir + common;
    strncat(out, down, size - strlen(out) - 1);
    return TRUE;
}

int Gen5_FilesInit(const char *gen5Dir)
{
    char base[PATH_MAX_LENGTH];
    if (!RelativePath(gen5Dir, base, sizeof(base))) {
        printf("Gen 5: can't reach %s from the game's folder\n", gen5Dir);
        return FALSE;
    }
    for (u32 i = 0; i < ARC_COUNT; i++) {
        snprintf(sPaths[i], PATH_MAX_LENGTH, "%s/%s", base, GFL_ArcSysGetResourcePath(i));
        sPathTable[i] = sPaths[i];
    }
    GFL_ArcSysInit(sPathTable, ARC_COUNT);
    return TRUE;
}

int Gen5_SelfCheck(char *message, int size)
{
    // libntr asks for a ROM when a file it opens is missing: make sure it's there first
    FILE *file = fopen(sPathTable[ARCID_PERSONAL], "rb");
    if (file == NULL) {
        snprintf(message, size, "The Pokémon data (%s) is missing", sPathTable[ARCID_PERSONAL]);
        return FALSE;
    }
    fclose(file);

    // Bulbasaur's base stats: HP, Attack, Defense, Speed, Sp. Atk, Sp. Def
    static const u8 expected[6] = { 45, 49, 49, 45, 65, 65 };
    u8 personal[128]; // BW2's entries are 76 bytes
    const u32 length = GFL_ArcSysGetDataLength(ARCID_PERSONAL, 1);
    if (length < sizeof(expected) || length > sizeof(personal)) {
        snprintf(message, size, "Bulbasaur's data is %lu bytes long", (unsigned long)length);
        return FALSE;
    }
    GFL_ArcSysRead(personal, ARCID_PERSONAL, 1);
    if (memcmp(personal, expected, sizeof(expected)) != 0) {
        snprintf(message, size, "Bulbasaur's base stats read as %d/%d/%d/%d/%d/%d", personal[0], personal[1],
            personal[2], personal[3], personal[4], personal[5]);
        return FALSE;
    }
    snprintf(message, size, "Read Bulbasaur's base stats (45/49/49/45/65/65) through BW2's archive code");
    return TRUE;
}

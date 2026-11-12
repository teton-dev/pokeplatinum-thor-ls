// The game's own Pokémon and move data in the Gen 5 engine (ROM hacks: Renegade Platinum); see
// docs/thor/dev/gen5-battles.md, "Renegade Platinum".
//
// BW2's engine reads its species and moves from its own archives (gen5/a/0/1/6 and gen5/a/0/2/1, shared by every game).
// A game whose data isn't vanilla Platinum's gets copies of those archives, in its own folder (gen5_data/), with the
// fields a battle uses taken from the game's own files:
//   species: the base stats, the types and the abilities
//   moves: the type, the category, the power, the accuracy, the PP and the priority
// Everything else stays BW2's (the experience a Pokémon gives, the catch rate, the move effects: BW2's code decides
// those by the move, so the hack's changes to what a move does are not carried over, only its numbers).
#include "port/gen5_data.h"
#include "port/gen5_galactic.h"

#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gen5.h"
#include "port/gen5_rom.h"
#include "port/rom_patches.h"

#define PERSONAL_ARC 16 // ARCID_PERSONAL: a/0/1/6
#define MOVE_INFO_ARC 21 // ARCID_MOVE_INFO: a/0/2/1

// Changes with the way the files are made
#define DATA_VERSION 2
#define DIR "gen5_data"
#define PERSONAL_PATH DIR "/a/0/1/6"
#define MOVE_INFO_PATH DIR "/a/0/2/1"
#define ICONS_PATH DIR "/a/0/8/2"
#define STAMP_PATH DIR "/stamp.txt"
#define ICONS_ARC 82 // ARCID_APP_MENU_COMMON: a/0/8/2

// FNV-1a of vanilla Platinum's (USA) files: a game with others is a hack
#define VANILLA_PERSONAL_HASH 0xF8E50A08u
#define VANILLA_MOVES_HASH 0x1600E4D2u

#define SPECIES_MAX 493
#define MOVE_MAX 467
#define PLATINUM_PERSONAL_SIZE 44
#define PLATINUM_MOVE_SIZE 16
#define BW2_PERSONAL_SIZE 76
#define BW2_MOVE_SIZE 36

// BW2's types are Platinum's without "???" (9): Fire is 9. Fairy, which Renegade Platinum gives "???"'s number, is 17
#define GEN5_TYPE_NORMAL 0
#define GEN5_TYPE_FAIRY 17

typedef struct Archive {
    u8 *data;
    u32 size;
    u32 count;
    u32 *start; // of each member, from the start of the file
    u32 *end;
} Archive;

static u32 Read32(const u8 *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

static u32 Hash(const u8 *data, u32 size)
{
    u32 hash = 0x811C9DC5u;
    for (u32 i = 0; i < size; i++) {
        hash = (hash ^ data[i]) * 0x01000193u;
    }
    return hash;
}

static u8 *ReadWhole(const char *path, u32 *sizeOut)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    u8 *data = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);
        if (size > 0 && size < 16 * 1024 * 1024 && fseek(f, 0, SEEK_SET) == 0) {
            data = malloc(size);
            if (data != NULL && fread(data, 1, size, f) == (size_t)size) {
                *sizeOut = (u32)size;
            } else {
                free(data);
                data = NULL;
            }
        }
    }
    fclose(f);
    return data;
}

// A NARC's members (BTAF's table of start and end, from GMIF's data)
static BOOL ArchiveOpen(Archive *archive, const char *path)
{
    memset(archive, 0, sizeof(*archive));
    archive->data = ReadWhole(path, &archive->size);
    if (archive->data == NULL || archive->size < 0x30 || memcmp(archive->data, "NARC", 4) != 0
        || memcmp(archive->data + 0x10, "BTAF", 4) != 0) {
        return FALSE;
    }
    u32 btafSize = Read32(archive->data + 0x14);
    archive->count = archive->data[0x18] | (archive->data[0x19] << 8);
    u32 btnf = 0x10 + btafSize;
    if (btnf + 8 > archive->size || memcmp(archive->data + btnf, "BTNF", 4) != 0) {
        return FALSE;
    }
    u32 gmif = btnf + Read32(archive->data + btnf + 4);
    if (gmif + 8 > archive->size || memcmp(archive->data + gmif, "GMIF", 4) != 0) {
        return FALSE;
    }
    archive->start = malloc(archive->count * sizeof(u32));
    archive->end = malloc(archive->count * sizeof(u32));
    for (u32 i = 0; i < archive->count; i++) {
        archive->start[i] = gmif + 8 + Read32(archive->data + 0x1C + 8 * i);
        archive->end[i] = gmif + 8 + Read32(archive->data + 0x1C + 8 * i + 4);
        if (archive->end[i] > archive->size || archive->start[i] > archive->end[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

static u8 *Member(const Archive *archive, u32 index, u32 size)
{
    if (index >= archive->count || archive->end[index] - archive->start[index] < size) {
        return NULL;
    }
    return archive->data + archive->start[index];
}

static void ArchiveClose(Archive *archive)
{
    free(archive->data);
    free(archive->start);
    free(archive->end);
    memset(archive, 0, sizeof(*archive));
}

// The game gives "???" (9) a meaning: Fairy (Renegade Platinum)
static BOOL sFairy;

static int TypeToGen5(int platinumType, BOOL fairy)
{
    if (platinumType < 9) {
        return platinumType;
    }
    if (platinumType == 9) {
        return fairy ? GEN5_TYPE_FAIRY : GEN5_TYPE_NORMAL;
    }
    return platinumType - 1;
}

// The member of Platinum's personal data for a species and form: the species' own, or one of the 14 forms after them
static int PlatinumPersonalMember(int species, int form)
{
    if (form == 0) {
        return species;
    }
    switch (species) {
    case 386: // Deoxys: Attack, Defense, Speed
        return form <= 3 ? 493 + form : -1;
    case 413: // Wormadam: Sandy, Trash
        return form <= 2 ? 496 + form : -1;
    case 487: // Giratina: Origin
        return form == 1 ? 499 : -1;
    case 492: // Shaymin: Sky
        return form == 1 ? 500 : -1;
    case 479: // Rotom: Heat, Wash, Frost, Fan, Mow
        return form <= 5 ? 500 + form : -1;
    }
    return -1;
}

static void ConvertPersonal(u8 *bw2, const u8 *platinum)
{
    memcpy(bw2, platinum, 6); // hp, attack, defense, speed, special attack, special defense
    bw2[6] = TypeToGen5(platinum[6], sFairy);
    bw2[7] = TypeToGen5(platinum[7], sFairy);
    bw2[24] = platinum[22];
    bw2[25] = platinum[23];
    // No hidden ability
}

static BOOL PersonalFor(const Archive *bw2, const Archive *platinum)
{
    BOOL changed = FALSE;
    for (int species = 1; species <= SPECIES_MAX; species++) {
        u8 *record = Member(bw2, species, BW2_PERSONAL_SIZE);
        const u8 *source = Member(platinum, species, PLATINUM_PERSONAL_SIZE);
        if (record == NULL || source == NULL) {
            return FALSE;
        }
        ConvertPersonal(record, source);
        changed = TRUE;
        // The forms: BW2 keeps them from the member FormFile on
        u16 formFile = record[28] | (record[29] << 8);
        int formCount = record[32];
        for (int form = 1; form < formCount && formFile != 0; form++) {
            int platinumMember = PlatinumPersonalMember(species, form);
            u8 *formRecord = Member(bw2, formFile + form - 1, BW2_PERSONAL_SIZE);
            const u8 *formSource = platinumMember >= 0 ? Member(platinum, platinumMember, PLATINUM_PERSONAL_SIZE) : NULL;
            if (formRecord != NULL && formSource != NULL) {
                ConvertPersonal(formRecord, formSource);
            }
        }
    }
    return changed;
}

// --- Replaced moves --------------------------------------------------------------------------------------
//
// Renegade Platinum replaces eleven moves in their slots (Barrage is Draining Kiss...). BW2 knows a move by its number:
// its name, effect and animation. So a Pokémon's move goes to BW2 under the number of BW2's own move where it has one
// (Brine is Scald, 503) and, for the Fairy moves BW2 lacks, under that of a Gen 5 move that Renegade doesn't have and
// that does what the plain data says (BW2's battle gives a move special handling by its number, and these have none):
// the move's own numbers and effect are written into that move's data and its name into the names. The animation is
// the Gen 5 move's, until the real ones are copied from a ROM that has them.
typedef struct ReplacedMove {
    u16 platinum;      // the slot in the game
    u16 gen5;          // BW2's move that stands for it
    u8 platinumType;   // what the slot holds when it is this move: the type and the power
    u8 power;
    const char *name;  // for the ones BW2 lacks: the name to show
    // Fairy moves BW2 lacks: the effect to write into the proxy's data: the first stat it changes (BW2's number: 1 attack,
    // 3 special attack) and its stage, whether it is a draining move, whether it is a sound move
    u8 stat;
    s8 stage;
    u8 drain;
    BOOL sound;
    u16 moonScript;    // Moon Black 2's animation script (its slot) that is the move's animation, or 0
    const char *gen5Name; // the Gen 5 move's name, which the move's "used" messages (a message bank by move) say
} ReplacedMove;

static const ReplacedMove sReplacedMoves[] = {
    // BW2 has them
    { 362, 503, 11, 80, NULL, 0, 0, 0, FALSE, 0, NULL },  // Brine -> Scald
    { 132, 556, 15, 85, NULL, 0, 0, 0, FALSE, 0, NULL },  // Constrict -> Icicle Crash
    { 32, 529, 4, 80, NULL, 0, 0, 0, FALSE, 0, NULL },    // Horn Drill -> Drill Run
    { 328, 523, 4, 60, NULL, 0, 0, 0, FALSE, 0, NULL },   // Sand Tomb -> Bulldoze
    { 239, 542, 2, 110, NULL, 0, 0, 0, FALSE, 0, NULL },  // Twister -> Hurricane
    { 344, 528, 13, 100, NULL, 0, 0, 0, FALSE, 0, NULL }, // Volt Tackle -> Wild Charge
    // Fairy, BW2 lacks them: Night Daze, Snarl, Struggle Bug, Razor Shell and Horn Leech stand for them
    { 461, 539, 9, 95, "Moonblast", 3, -1, 0, FALSE, 461, "Night Daze" },
    { 295, 555, 9, 80, "Dazzling Gleam", 0, 0, 0, FALSE, 552, "Snarl" },
    { 296, 522, 9, 50, "Disarming Voice", 0, 0, 0, TRUE, 545, "Struggle Bug" },
    { 66, 534, 9, 90, "Play Rough", 1, -1, 0, FALSE, 285, "Razor Shell" },
    { 140, 532, 9, 75, "Draining Kiss", 0, 0, 75, FALSE, 0, "Horn Leech" },
};

static u16 sMoveToGen5[MOVE_MAX + 1];
static u8 sMoveReplaced[MOVE_MAX + 1]; // 1 + the index in sReplacedMoves, or 0
static u16 sMoveFromGen5[560];
static BOOL sMapBuilt;
static BOOL sMapActive;

// The map, from the game's moves (the platinum archive's, or NULL for none)
static void BuildMoveMap(const Archive *platinum)
{
    for (int move = 0; move <= MOVE_MAX; move++) {
        sMoveToGen5[move] = move;
        sMoveReplaced[move] = 0;
    }
    for (int move = 0; move < 560; move++) {
        sMoveFromGen5[move] = move;
    }
    sMapActive = FALSE;
    for (int i = 0; platinum != NULL && i < (int)NELEMS(sReplacedMoves); i++) {
        const ReplacedMove *replaced = &sReplacedMoves[i];
        const u8 *source = Member(platinum, replaced->platinum, PLATINUM_MOVE_SIZE);
        if (source == NULL || source[4] != replaced->platinumType || source[3] != replaced->power) {
            continue;
        }
        if (replaced->name != NULL && !sFairy) {
            continue;
        }
        sMoveToGen5[replaced->platinum] = replaced->gen5;
        sMoveReplaced[replaced->platinum] = 1 + i;
        sMoveFromGen5[replaced->gen5] = replaced->platinum;
        sMapActive = TRUE;
    }
}

// The game's moves are a hack's: the map is made at the first use
static void EnsureMoveMap(void)
{
    if (sMapBuilt) {
        return;
    }
    sMapBuilt = TRUE;
    sFairy = RomPatches_Info()->extraType;
    u32 size = 0;
    u8 *moves = ReadWhole("poketool/waza/pl_waza_tbl.narc", &size);
    if (moves != NULL && Hash(moves, size) != VANILLA_MOVES_HASH) {
        Archive platinum;
        if (ArchiveOpen(&platinum, "poketool/waza/pl_waza_tbl.narc")) {
            BuildMoveMap(&platinum);
        }
        ArchiveClose(&platinum);
    }
    free(moves);
}

u16 Gen5Data_MoveToGen5(u16 move)
{
    EnsureMoveMap();
    return move <= MOVE_MAX ? sMoveToGen5[move] : move;
}

u16 Gen5Data_MoveFromGen5(u16 move)
{
    EnsureMoveMap();
    return move < 560 ? sMoveFromGen5[move] : move;
}

static BOOL MovesFor(const Archive *bw2, const Archive *platinum)
{
    EnsureMoveMap();
    for (int move = 1; move <= MOVE_MAX; move++) {
        const u8 *source = Member(platinum, move, PLATINUM_MOVE_SIZE);
        u8 *record = Member(bw2, sMoveToGen5[move], BW2_MOVE_SIZE);
        if (record == NULL || source == NULL) {
            return FALSE;
        }
        // Platinum's category: 0 physical, 1 special, 2 status; BW2's: 0 status, 1 physical, 2 special
        static const u8 category[3] = { 1, 2, 0 };
        if (source[2] > 2) {
            continue;
        }
        record[0] = TypeToGen5(source[4], sFairy);
        record[2] = category[source[2]];
        record[3] = source[3];
        record[4] = source[5] == 0 ? 101 : source[5]; // never misses: BW2's 101
        record[5] = source[6];
        record[6] = source[10];
        if (sMoveReplaced[move] != 0 && sReplacedMoves[sMoveReplaced[move] - 1].name != NULL) {
            // A Fairy move on a Gen 5 move's data: the effect as the game has it
            const ReplacedMove *replaced = &sReplacedMoves[sMoveReplaced[move] - 1];
            record[1] = replaced->drain != 0 ? 8 : replaced->stat != 0 ? 6 : 0; // damage, drain or lowers a stat
            record[15] = 0;                                                    // no flinch
            record[18] = replaced->drain;
            memset(record + 21, 0, 9);                                          // the stat changes
            if (replaced->stat != 0) {
                record[21] = replaced->stat;
                record[24] = (u8)replaced->stage;
                record[27] = source[7];                                          // its chance
            }
            u32 flags = record[32] | (record[33] << 8) | (record[34] << 16) | ((u32)record[35] << 24);
            flags = replaced->sound ? (flags | 0x100) : (flags & ~0x100u);
            record[32] = flags & 0xFF;
            record[33] = (flags >> 8) & 0xFF;
        }
    }
    return TRUE;
}

// The game's own type chart (rows of attacker, defender and multiplier in Platinum's types) as BW2's 18 by 18 table,
// or NULL for BW2's own
static BOOL MakeTypeChart(u8 chart[18][18])
{
    const u8 (*rows)[3] = RomPatches_TypeChart(NULL);
    if (rows == NULL) {
        return FALSE;
    }
    memset(chart, 4, 18 * 18);
    for (int i = 0; rows[i][0] != 0xFF; i++) {
        if (rows[i][0] == 0xFE) {
            continue; // the rows after it are the Foresight-only ones: BW2 does that in code, the table says immune
        }
        const int attack = TypeToGen5(rows[i][0], sFairy);
        const int defense = TypeToGen5(rows[i][1], sFairy);
        static const u8 halves[] = { 0, 2, 4, 8 };
        const int multiplier = rows[i][2] == 0 ? 0 : rows[i][2] == 5 ? 1 : rows[i][2] == 10 ? 2 : 3;
        if ((rows[i][0] == 9 || rows[i][1] == 9) && !sFairy) {
            continue; // "???" has no row in a chart that doesn't use it
        }
        chart[attack][defense] = halves[multiplier];
    }
    return TRUE;
}

// --- Fairy's type icon ---------------------------------------------------------------------------------
//
// BW2's type icons (the menus' common archive, a/0/8/2: the pictures are members 0x22 + type, 32 by 16 pixels in 8 tiles
// of 4 bits, the colors member 0x21, three palettes of 16) don't have Fairy's. It's made from the letters of the others, with
// the colors 8 and 9 of the third palette, which no icon uses: F, I and R of FIRE, A of GRASS and Y of FLYING.
// The archive's member 0x33, which was the contest category Cool's and is never shown, takes it
// (gen5/patches/src/system/app_menu_common.c.patch).
#define ICON_MEMBER_PALETTE 0x21
#define ICON_MEMBER_FIRST_TYPE 0x22
#define ICON_MEMBER_FAIRY (ICON_MEMBER_FIRST_TYPE + GEN5_TYPE_FAIRY)
#define ICON_DATA_OFFSET 0x30
#define ICON_PALETTE_OFFSET 0x28
#define ICON_BACKGROUND 8
#define ICON_BORDER 9

static void DecodeIcon(const u8 *data, u8 pixels[16][32])
{
    for (int tile = 0; tile < 8; tile++) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                u8 byte = data[tile * 32 + y * 4 + x / 2];
                pixels[(tile / 4) * 8 + y][(tile % 4) * 8 + x] = (x & 1) ? byte >> 4 : byte & 15;
            }
        }
    }
}

static void EncodeIcon(u8 *data, u8 pixels[16][32])
{
    for (int tile = 0; tile < 8; tile++) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x += 2) {
                const u8 low = pixels[(tile / 4) * 8 + y][(tile % 4) * 8 + x];
                const u8 high = pixels[(tile / 4) * 8 + y][(tile % 4) * 8 + x + 1];
                data[tile * 32 + y * 4 + x / 2] = low | (high << 4);
            }
        }
    }
}

static BOOL MakeFairyIcon(Archive *icons)
{
    u8 *fire = Member(icons, ICON_MEMBER_FIRST_TYPE + 9, ICON_DATA_OFFSET + 256);
    u8 *grass = Member(icons, ICON_MEMBER_FIRST_TYPE + 11, ICON_DATA_OFFSET + 256);
    u8 *flying = Member(icons, ICON_MEMBER_FIRST_TYPE + 2, ICON_DATA_OFFSET + 256);
    u8 *target = Member(icons, ICON_MEMBER_FAIRY, ICON_DATA_OFFSET + 256);
    u8 *palette = Member(icons, ICON_MEMBER_PALETTE, ICON_PALETTE_OFFSET + 96 * 2);
    if (fire == NULL || grass == NULL || flying == NULL || target == NULL || palette == NULL) {
        return FALSE;
    }
    u8 fromFire[16][32], fromGrass[16][32], fromFlying[16][32], out[16][32];
    DecodeIcon(fire + ICON_DATA_OFFSET, fromFire);
    DecodeIcon(grass + ICON_DATA_OFFSET, fromGrass);
    DecodeIcon(flying + ICON_DATA_OFFSET, fromFlying);

    // The frame and the background, as the others have them
    memset(out, 0, sizeof(out));
    for (int y = 2; y <= 13; y++) {
        for (int x = 0; x < 32; x++) {
            out[y][x] = (y == 2 || y == 13 || x == 0 || x == 31) ? ICON_BORDER : ICON_BACKGROUND;
        }
    }
    // The letters (their pixels 14 and 15: the shadow and the fill), each from a column range of an icon, in the rows 4 to 11
    const struct {
        u8 (*pixels)[32];
        int from;
        int to;
        int at;
    } letters[] = {
        { fromFire, 5, 9, 2 },
        { fromGrass, 14, 18, 8 },
        { fromFire, 11, 14, 14 },
        { fromFire, 16, 20, 19 },
        { fromFlying, 11, 16, 25 },
    };
    for (int i = 0; i < (int)NELEMS(letters); i++) {
        for (int y = 4; y <= 11; y++) {
            for (int x = letters[i].from; x <= letters[i].to; x++) {
                const u8 value = letters[i].pixels[y][x];
                if (value == 14 || value == 15) {
                    out[y][letters[i].at + x - letters[i].from] = value;
                }
            }
        }
    }
    EncodeIcon(target + ICON_DATA_OFFSET, out);

    // Palette 2's colors 8 and 9: Fairy's pink and its frame
    static const u16 colors[2] = { 29 | (19 << 5) | (21 << 10), 22 | (11 << 5) | (15 << 10) };
    for (int i = 0; i < 2; i++) {
        u8 *color = palette + ICON_PALETTE_OFFSET + 2 * (2 * 16 + ICON_BACKGROUND + i);
        color[0] = colors[i] & 0xFF;
        color[1] = colors[i] >> 8;
    }
    return TRUE;
}

// --- The Fairy moves' names -------------------------------------------------------------------------------
//
// BW2's text: the system messages' archive (a/0/0/2, ARCID_SYSTEM_MESSAGE), member 403 the move names: a header (the
// languages, the number of lines, the size of the largest block, the language's offset), then a block with, for each
// line, its offset in the block and its length in 16-bit characters (the terminator 0xFFFF included), and the lines,
// each encrypted with a key that its number gives and that each character turns by 3 bits.
#define MOVE_NAMES_ARC 2
#define MOVE_NAMES_PATH DIR "/a/0/0/2"
#define MOVE_NAMES_MEMBER 403

static BOOL WriteWhole(const char *path, const u8 *data, u32 size);

static u16 NextKey(u16 key)
{
    return (u16)(((key & 0xE000) >> 13) | (u16)(key << 3));
}

// The move names with the Fairy moves' (the name of the Gen 5 move that stands for each), in a new member
static u8 *MoveNamesWithFairy(const u8 *member, u32 size, u32 *outSize)
{
    if (size < 16 || (member[0] | (member[1] << 8)) != 1) {
        return NULL;
    }
    const u32 lines = member[2] | (member[3] << 8);
    const u32 block = Read32(member + 12);
    if (block + 4 + lines * 8 > size) {
        return NULL;
    }
    u8 *out = calloc(1, size + 1024);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, member, block + 4 + lines * 8); // the header and the table, rewritten below
    u32 position = 4 + lines * 8;                // in the block
    for (u32 line = 0; line < lines; line++) {
        const u32 offset = Read32(member + block + 4 + line * 8);
        const u32 length = member[block + 4 + line * 8 + 4] | (member[block + 4 + line * 8 + 5] << 8);
        const char *name = NULL;
        for (int i = 0; i < (int)NELEMS(sReplacedMoves); i++) {
            if (sReplacedMoves[i].name != NULL && sMoveReplaced[sReplacedMoves[i].platinum] != 0 && sReplacedMoves[i].gen5 == line) {
                name = sReplacedMoves[i].name;
            }
        }
        u8 *dest = out + block + position;
        if (block + offset + length * 2 > size && name == NULL) {
            free(out);
            return NULL;
        }
        u32 newLength;
        if (name != NULL) {
            newLength = (u32)strlen(name) + 1;
            u16 key = (u16)((line + 3) * 0x2983);
            for (u32 k = 0; k < newLength; k++) {
                u16 c = k + 1 == newLength ? 0xFFFF : (u8)name[k];
                c ^= key;
                key = NextKey(key);
                dest[2 * k] = c & 0xFF;
                dest[2 * k + 1] = c >> 8;
            }
        } else {
            newLength = length;
            memcpy(dest, member + block + offset, length * 2);
        }
        out[block + 4 + line * 8] = position & 0xFF;
        out[block + 4 + line * 8 + 1] = (position >> 8) & 0xFF;
        out[block + 4 + line * 8 + 2] = (position >> 16) & 0xFF;
        out[block + 4 + line * 8 + 3] = (position >> 24) & 0xFF;
        out[block + 4 + line * 8 + 4] = newLength & 0xFF;
        out[block + 4 + line * 8 + 5] = (newLength >> 8) & 0xFF;
        out[block + 4 + line * 8 + 6] = member[block + 4 + line * 8 + 6];
        out[block + 4 + line * 8 + 7] = member[block + 4 + line * 8 + 7];
        position += newLength * 2;
    }
    out[block] = position & 0xFF;
    out[block + 1] = (position >> 8) & 0xFF;
    out[block + 2] = (position >> 16) & 0xFF;
    out[block + 3] = (position >> 24) & 0xFF;
    out[4] = position & 0xFF; // the largest block
    out[5] = (position >> 8) & 0xFF;
    out[6] = (position >> 16) & 0xFF;
    out[7] = (position >> 24) & 0xFF;
    *outSize = block + position;
    return out;
}

static void Write32(u8 *p, u32 value)
{
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
    p[2] = (value >> 16) & 0xFF;
    p[3] = (value >> 24) & 0xFF;
}

// A NARC as the archive is, with one member replaced (the members follow each other, each on a multiple of 4)
// The archive with some members replaced (count of them: index, data and size each)
static u8 *ArchiveWithMembers(const Archive *archive, u32 count, const u32 *indices, const u8 *const *data, const u32 *sizes, u32 *outSize)
{
    const u32 btaf = 0x10;
    const u32 btnf = btaf + Read32(archive->data + btaf + 4);
    const u32 gmif = btnf + Read32(archive->data + btnf + 4);
    const u32 gmifData = gmif + 8;
    u32 total = gmifData;
    for (u32 i = 0; i < archive->count; i++) {
        u32 length = archive->end[i] - archive->start[i];
        for (u32 k = 0; k < count; k++) {
            if (indices[k] == i) {
                length = sizes[k];
            }
        }
        total += (length + 3) & ~3u;
    }
    u8 *out = calloc(1, total);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, archive->data, gmifData);
    u32 position = 0;
    for (u32 i = 0; i < archive->count; i++) {
        const u8 *from = archive->data + archive->start[i];
        u32 length = archive->end[i] - archive->start[i];
        for (u32 k = 0; k < count; k++) {
            if (indices[k] == i) {
                from = data[k];
                length = sizes[k];
            }
        }
        memcpy(out + gmifData + position, from, length);
        Write32(out + 0x1C + 8 * i, position);
        Write32(out + 0x1C + 8 * i + 4, position + length);
        position += (length + 3) & ~3u;
    }
    Write32(out + 8, total);
    Write32(out + gmif + 4, 8 + position);
    *outSize = total;
    return out;
}

static u8 *ArchiveWithMember(const Archive *archive, u32 index, const u8 *data, u32 size, u32 *outSize)
{
    return ArchiveWithMembers(archive, 1, &index, &data, &size, outSize);
}

// The messages "X used move!" are a bank of their own (member 16: three per move, by who uses it), with the move's name in
// the text: a Fairy move's name goes where the Gen 5 move's was
#define MOVE_USED_MEMBER 16

static u8 *MoveUsedMessagesWithFairy(const u8 *member, u32 size, u32 *outSize)
{
    if (size < 16 || (member[0] | (member[1] << 8)) != 1) {
        return NULL;
    }
    const u32 lines = member[2] | (member[3] << 8);
    const u32 block = Read32(member + 12);
    if (block + 4 + lines * 8 > size) {
        return NULL;
    }
    u8 *out = calloc(1, size + 4096);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, member, block + 4 + lines * 8);
    u32 position = 4 + lines * 8;
    for (u32 line = 0; line < lines; line++) {
        const u32 offset = Read32(member + block + 4 + line * 8);
        const u32 length = member[block + 4 + line * 8 + 4] | (member[block + 4 + line * 8 + 5] << 8);
        if (block + offset + length * 2 > size) {
            free(out);
            return NULL;
        }
        const ReplacedMove *replaced = NULL;
        for (int i = 0; i < (int)NELEMS(sReplacedMoves); i++) {
            if (sReplacedMoves[i].name != NULL && sMoveReplaced[sReplacedMoves[i].platinum] != 0 && sReplacedMoves[i].gen5 == line / 3) {
                replaced = &sReplacedMoves[i];
            }
        }
        u16 text[512];
        u32 count = length < 512 ? length : 512;
        u16 key = (u16)((line + 3) * 0x2983);
        for (u32 k = 0; k < count; k++) {
            const u8 *w = member + block + offset + 2 * k;
            text[k] = (u16)((w[0] | (w[1] << 8)) ^ key);
            key = NextKey(key);
        }
        u16 result[512 + 64];
        u32 resultCount = 0;
        if (replaced != NULL && count == length) {
            const u32 oldLength = (u32)strlen(replaced->gen5Name);
            const u32 newLength = (u32)strlen(replaced->name);
            for (u32 k = 0; k < count;) {
                BOOL match = k + oldLength <= count;
                for (u32 j = 0; match && j < oldLength; j++) {
                    match = text[k + j] == (u8)replaced->gen5Name[j];
                }
                if (match && resultCount + newLength < NELEMS(result)) {
                    for (u32 j = 0; j < newLength; j++) {
                        result[resultCount++] = (u8)replaced->name[j];
                    }
                    k += oldLength;
                } else {
                    result[resultCount++] = text[k++];
                }
            }
        } else {
            memcpy(result, text, count * 2);
            resultCount = count;
        }
        u8 *dest = out + block + position;
        key = (u16)((line + 3) * 0x2983);
        for (u32 k = 0; k < resultCount; k++) {
            const u16 c = result[k] ^ key;
            key = NextKey(key);
            dest[2 * k] = c & 0xFF;
            dest[2 * k + 1] = c >> 8;
        }
        out[block + 4 + line * 8] = position & 0xFF;
        out[block + 4 + line * 8 + 1] = (position >> 8) & 0xFF;
        out[block + 4 + line * 8 + 2] = (position >> 16) & 0xFF;
        out[block + 4 + line * 8 + 3] = (position >> 24) & 0xFF;
        out[block + 4 + line * 8 + 4] = resultCount & 0xFF;
        out[block + 4 + line * 8 + 5] = (resultCount >> 8) & 0xFF;
        out[block + 4 + line * 8 + 6] = member[block + 4 + line * 8 + 6];
        out[block + 4 + line * 8 + 7] = member[block + 4 + line * 8 + 7];
        position += resultCount * 2;
    }
    Write32(out + block, position);
    Write32(out + 4, position);
    *outSize = block + position;
    return out;
}

// Animations: Moon Black 2 (a hack of BW2 with Fairy moves) has the Fairy moves' scripts, using BW2's own particles. A
// copy of its move animations (a/0/6/5), put in gen5_data/moon/, supplies them to the games with Fairy (never in the
// repository); each script goes to the slot of the Gen 5 move standing for the Fairy move
#define MOVE_ANIM_ARC 65 // ARCID_BATTLE_EFFECT: a/0/6/5
#define MOVE_ANIM_PATH DIR "/a/0/6/5"
#define MOON_ANIM_PATH DIR "/moon/a_0_6_5"

static BOOL MakeFairyAnimations(const char *bw2Dir)
{
    Archive moon;
    if (!ArchiveOpen(&moon, MOON_ANIM_PATH)) {
        ArchiveClose(&moon);
        return FALSE;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/a/0/6/5", bw2Dir);
    Archive bw2;
    BOOL ok = ArchiveOpen(&bw2, path);
    u32 indices[NELEMS(sReplacedMoves)];
    const u8 *data[NELEMS(sReplacedMoves)];
    u32 sizes[NELEMS(sReplacedMoves)];
    u32 count = 0;
    for (int i = 0; ok && i < (int)NELEMS(sReplacedMoves); i++) {
        const ReplacedMove *replaced = &sReplacedMoves[i];
        if (replaced->moonScript == 0 || sMoveReplaced[replaced->platinum] == 0 || replaced->moonScript >= moon.count
            || replaced->gen5 >= bw2.count) {
            continue;
        }
        indices[count] = replaced->gen5;
        data[count] = moon.data + moon.start[replaced->moonScript];
        sizes[count] = moon.end[replaced->moonScript] - moon.start[replaced->moonScript];
        count++;
    }
    u32 size = 0;
    u8 *file = ok ? ArchiveWithMembers(&bw2, count, indices, data, sizes, &size) : NULL;
    ok = file != NULL && WriteWhole(MOVE_ANIM_PATH, file, size);
    free(file);
    ArchiveClose(&bw2);
    ArchiveClose(&moon);
    return ok;
}

static BOOL MakeFairyNames(const char *bw2Dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/a/0/0/2", bw2Dir);
    Archive messages;
    BOOL ok = ArchiveOpen(&messages, path);
    if (ok) {
        u32 sizes[2] = { 0, 0 };
        u32 newSize = 0;
        const u8 *namesMember = Member(&messages, MOVE_NAMES_MEMBER, 16);
        const u8 *usedMember = Member(&messages, MOVE_USED_MEMBER, 16);
        u8 *names = namesMember != NULL ? MoveNamesWithFairy(namesMember, messages.end[MOVE_NAMES_MEMBER] - messages.start[MOVE_NAMES_MEMBER], &sizes[0]) : NULL;
        u8 *used = usedMember != NULL ? MoveUsedMessagesWithFairy(usedMember, messages.end[MOVE_USED_MEMBER] - messages.start[MOVE_USED_MEMBER], &sizes[1]) : NULL;
        const u32 indices[2] = { MOVE_NAMES_MEMBER, MOVE_USED_MEMBER };
        const u8 *data[2] = { names, used };
        u8 *file = names != NULL && used != NULL ? ArchiveWithMembers(&messages, 2, indices, data, sizes, &newSize) : NULL;
        ok = file != NULL && WriteWhole(MOVE_NAMES_PATH, file, newSize);
        free(names);
        free(used);
        free(file);
    }
    ArchiveClose(&messages);
    return ok;
}

static void MakeDirs(void)
{
    mkdir(DIR, 0755);
    mkdir(DIR "/a", 0755);
    mkdir(DIR "/a/0", 0755);
    mkdir(DIR "/a/0/1", 0755);
    mkdir(DIR "/a/0/2", 0755);
    mkdir(DIR "/a/0/8", 0755);
    mkdir(DIR "/a/0/0", 0755);
    mkdir(DIR "/a/0/6", 0755);
}

// Written over the file that is there, if any, so that BW2 keeps reading the one it opened
static BOOL WriteWhole(const char *path, const u8 *data, u32 size)
{
    FILE *f = fopen(path, "r+b");
    if (f == NULL) {
        f = fopen(path, "wb");
    }
    if (f == NULL) {
        return FALSE;
    }
    BOOL ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok;
}

static BOOL sRegistered;

BOOL Gen5Data_Prepare(void)
{
    sFairy = RomPatches_Info()->extraType;
    EnsureMoveMap();
    // The type chart (Fairy's and a hack's own changes), for any game that has one of its own
    {
        u8 chart[18][18];
        if (MakeTypeChart(chart)) {
            Gen5_PortTypeChart(&chart[0][0]);
            // 0 immune, 2 half, 4 normal, 8 double: Fairy on Dragon, Poison on Fairy, Fire on Fairy, Dragon on Fairy
            printf("Gen 5: the game's type chart (Fairy %s): Fairy on Dragon %d, Poison on Fairy %d, Steel on Fairy %d, Dragon on Fairy %d\n",
                sFairy ? "in" : "out", chart[GEN5_TYPE_FAIRY][15], chart[3][GEN5_TYPE_FAIRY], chart[8][GEN5_TYPE_FAIRY], chart[15][GEN5_TYPE_FAIRY]);
        }
    }
    u32 personalSize = 0;
    u32 movesSize = 0;
    u8 *personal = ReadWhole("poketool/personal/pl_personal.narc", &personalSize);
    u8 *moves = ReadWhole("poketool/waza/pl_waza_tbl.narc", &movesSize);
    BOOL done = FALSE;

    if (personal == NULL || moves == NULL) {
        goto out;
    }
    const u32 personalHash = Hash(personal, personalSize);
    const u32 movesHash = Hash(moves, movesSize);
    if (personalHash == VANILLA_PERSONAL_HASH && movesHash == VANILLA_MOVES_HASH) {
        goto out; // Platinum's own: BW2's data
    }

    u32 moonSize = 0;
    u8 *moon = sFairy ? ReadWhole(MOON_ANIM_PATH, &moonSize) : NULL;
    const u32 moonHash = moon != NULL ? Hash(moon, moonSize) : 0;
    free(moon);
    char stamp[64];
    snprintf(stamp, sizeof(stamp), "%d %08x %08x %08x\n", DATA_VERSION, personalHash, movesHash, moonHash);
    u32 stampSize = 0;
    u8 *old = ReadWhole(STAMP_PATH, &stampSize);
    const BOOL current = old != NULL && stampSize == strlen(stamp) && memcmp(old, stamp, stampSize) == 0;
    free(old);

    if (!current) {
        char path[512];
        Archive bw2Personal, bw2Moves, platinumPersonal, platinumMoves, bw2Icons;
        snprintf(path, sizeof(path), "%s/a/0/1/6", Gen5Rom_Dir());
        BOOL ok = ArchiveOpen(&bw2Personal, path);
        snprintf(path, sizeof(path), "%s/a/0/2/1", Gen5Rom_Dir());
        ok = ok && ArchiveOpen(&bw2Moves, path);
        ok = ok && ArchiveOpen(&platinumPersonal, "poketool/personal/pl_personal.narc");
        ok = ok && ArchiveOpen(&platinumMoves, "poketool/waza/pl_waza_tbl.narc");
        ok = ok && PersonalFor(&bw2Personal, &platinumPersonal) && MovesFor(&bw2Moves, &platinumMoves);
        snprintf(path, sizeof(path), "%s/a/0/8/2", Gen5Rom_Dir());
        const BOOL iconsOpen = sFairy && ArchiveOpen(&bw2Icons, path);
        ok = ok && (!sFairy || (iconsOpen && MakeFairyIcon(&bw2Icons)));
        if (ok) {
            MakeDirs();
            if (sFairy) {
                ok = WriteWhole(ICONS_PATH, bw2Icons.data, bw2Icons.size) && MakeFairyNames(Gen5Rom_Dir());
                if (ok && moonHash != 0 && !MakeFairyAnimations(Gen5Rom_Dir())) {
                    printf("Gen 5: Moon Black 2's animations couldn't be used: the Gen 5 moves' own are\n");
                    remove(MOVE_ANIM_PATH);
                }
            }
        }
        if (ok) {
            ok = WriteWhole(PERSONAL_PATH, bw2Personal.data, bw2Personal.size) && WriteWhole(MOVE_INFO_PATH, bw2Moves.data, bw2Moves.size)
                && WriteWhole(STAMP_PATH, (const u8 *)stamp, (u32)strlen(stamp));
        }
        if (iconsOpen) {
            ArchiveClose(&bw2Icons);
        }
        ArchiveClose(&bw2Personal);
        ArchiveClose(&bw2Moves);
        ArchiveClose(&platinumPersonal);
        ArchiveClose(&platinumMoves);
        if (!ok) {
            printf("Gen 5: the game's own Pokemon and move data couldn't be made: BW2's are used\n");
            goto out;
        }
        printf("Gen 5: the game's own Pokemon and move data made in %s\n", DIR);
    }
    if (!sRegistered) {
        Gen5_FilesOverride(PERSONAL_ARC, PERSONAL_PATH);
        Gen5_FilesOverride(MOVE_INFO_ARC, MOVE_INFO_PATH);
        if (sFairy) {
            Gen5_FilesOverride(ICONS_ARC, ICONS_PATH);
            Gen5_FilesOverride(MOVE_NAMES_ARC, MOVE_NAMES_PATH);
            FILE *anim = fopen(MOVE_ANIM_PATH, "rb");
            if (anim != NULL) {
                fclose(anim);
                Gen5_FilesOverride(MOVE_ANIM_ARC, MOVE_ANIM_PATH);
            }
        }
        sRegistered = TRUE;
    }
    done = TRUE;

out:
    {
        // A Fairy move stands in a Gen 5 move's slot: BW2's handlers for that move don't apply
        u16 plain[16];
        u32 n = 0;
        for (int i = 0; i < (int)NELEMS(sReplacedMoves) && n < 16; i++) {
            if (sReplacedMoves[i].name != NULL && sMoveReplaced[sReplacedMoves[i].platinum] != 0) {
                plain[n++] = sReplacedMoves[i].gen5;
            }
        }
        Gen5_PortNoHandlers(plain, n);
    }
    free(personal);
    free(moves);
    return done;
}

// The Team Galactic arena (src/port/gen5_galactic.c): BW2's Champion room with other streaks and stars. It is made from
// the player's ROM into copies of two archives, in gen5_data/, that BW2 then reads in place of its own:
//   archive 11 (ARCID_BTLV, a/0/1/1): the changed copy of the room's model resource (member 115) is added as the last
//     member (a new resource 572)
//   archive 151 (ARCID_BATTLE_BG_DATA, a/1/5/1): file 0 (the arenas) has, for the Champion's background (16), terrain 1
//     (Platinum's battles never use it there) in a new field, 47: file 1's rows of resources, with that field's the
//     Champion room's (18) with the new model. The room's animations (the scrolling of the streaks and the sky, by the
//     materials' names) and its stage (the gold rings) are the Champion room's.
// ArenaFromDTO picks background 16, terrain 1 for a Galactic battle.
#define BTLV_ARC 11 // ARCID_BTLV: a/0/1/1
#define BTLV_PATH DIR "/a/0/1/1"
#define BG_DATA_ARC 151 // ARCID_BATTLE_BG_DATA: a/1/5/1
#define BG_DATA_PATH DIR "/a/1/5/1"
#define GALACTIC_STAMP_PATH DIR "/galactic_arena.txt"
#define GALACTIC_VERSION 6
#define CHAMPION_MODEL 115 // member of archive 11
#define CHAMPION_STAGE 21
#define STAGE_MODEL 38 // member of archive 11: both grounds of the Champion room's stage
#define STAGE_ROW_SIZE 0x42 // a stage of file 2: the edge colour, 4 kinds of resources, 8 variants each
#define CHAMPION_FIELD 18
#define CHAMPION_BG 16
#define GALACTIC_TERRAIN 1
#define BG_ENTRY_SIZE 44 // an arena of file 0: light, seasonal, fieldIds[20], stageIds[20], 2 more
#define FIELD_ROW_SIZE 64 // a field of file 1: 4 kinds of resources, 8 variants each, 2 bytes each

// The archive with members added at the end (what ArchiveWithMembers does for the members it has, and BTAF's table
// grows with them)
static u8 *ArchiveWithAdded(const Archive *archive, u32 count, const u8 *const *data, const u32 *sizes, u32 *outSize)
{
    const u32 btaf = 0x10;
    const u32 btnf = btaf + Read32(archive->data + btaf + 4);
    const u32 gmif = btnf + Read32(archive->data + btnf + 4);
    const u32 btnfSize = gmif - btnf;
    const u32 total = archive->count + count;
    const u32 btafSize = 12 + 8 * total;
    const u32 gmifData = btaf + btafSize + btnfSize + 8;
    u32 dataSize = 0;
    for (u32 i = 0; i < archive->count; i++) {
        dataSize += (archive->end[i] - archive->start[i] + 3) & ~3u;
    }
    for (u32 k = 0; k < count; k++) {
        dataSize += (sizes[k] + 3) & ~3u;
    }
    u8 *out = calloc(1, gmifData + dataSize);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, archive->data, btaf); // the NARC's header
    memcpy(out + btaf, "BTAF", 4);
    Write32(out + btaf + 4, btafSize);
    out[btaf + 8] = total & 0xFF;
    out[btaf + 9] = total >> 8;
    memcpy(out + btaf + btafSize, archive->data + btnf, btnfSize);
    memcpy(out + btaf + btafSize + btnfSize, "GMIF", 4);
    Write32(out + btaf + btafSize + btnfSize + 4, 8 + dataSize);
    u32 position = 0;
    for (u32 i = 0; i < total; i++) {
        const u8 *from;
        u32 length;
        if (i < archive->count) {
            from = archive->data + archive->start[i];
            length = archive->end[i] - archive->start[i];
        } else {
            from = data[i - archive->count];
            length = sizes[i - archive->count];
        }
        memcpy(out + gmifData + position, from, length);
        Write32(out + btaf + 12 + 8 * i, position);
        Write32(out + btaf + 12 + 8 * i + 4, position + length);
        position += (length + 3) & ~3u;
    }
    Write32(out + 8, gmifData + dataSize);
    *outSize = gmifData + dataSize;
    return out;
}

// The model resource a field's row (of file 1) names first
static u32 Read16Table(const u8 *fields, u32 field)
{
    return fields[field * FIELD_ROW_SIZE] | (fields[field * FIELD_ROW_SIZE + 1] << 8);
}

static BOOL MakeGalacticArena(const char *bw2Dir)
{
    char path[512];
    Archive models, arenas;
    snprintf(path, sizeof(path), "%s/a/0/1/1", bw2Dir);
    BOOL ok = ArchiveOpen(&models, path);
    snprintf(path, sizeof(path), "%s/a/1/5/1", bw2Dir);
    ok = ok && ArchiveOpen(&arenas, path);
    u8 *model = NULL;
    u8 *bgs = NULL;
    u8 *fields = NULL;
    u8 *stageModel = NULL;
    u8 *stages = NULL;
    u8 *modelsFile = NULL;
    u8 *arenasFile = NULL;
    u32 modelSize = 0, bgsSize = 0, fieldsSize = 0, modelsFileSize = 0, arenasFileSize = 0;
    u32 newModel = 0, newField = 0, stagesSize = 0, newStage = 0, stageModelSize = 0;

    if (ok && arenas.count >= 3 && models.count > CHAMPION_MODEL && models.count > STAGE_MODEL) {
        modelSize = models.end[CHAMPION_MODEL] - models.start[CHAMPION_MODEL];
        bgsSize = arenas.end[0] - arenas.start[0];
        fieldsSize = arenas.end[1] - arenas.start[1];
        newModel = models.count;
        newField = fieldsSize / FIELD_ROW_SIZE;
        stageModelSize = models.end[STAGE_MODEL] - models.start[STAGE_MODEL];
        stagesSize = arenas.end[2] - arenas.start[2];
        newStage = stagesSize / STAGE_ROW_SIZE; // the file ends with 2 bytes more than its rows
    } else {
        ok = FALSE;
    }
    if (ok && (bgsSize < (CHAMPION_BG + 1) * BG_ENTRY_SIZE || fieldsSize % FIELD_ROW_SIZE != 0 || newField <= CHAMPION_FIELD
            || newField > 255 || newStage <= CHAMPION_STAGE || newStage > 255)) {
        ok = FALSE;
    }
    if (ok) {
        model = malloc(modelSize);
        bgs = malloc(bgsSize);
        fields = malloc(fieldsSize + FIELD_ROW_SIZE);
        stageModel = malloc(stageModelSize);
        stages = malloc(newStage * STAGE_ROW_SIZE + STAGE_ROW_SIZE);
        ok = model != NULL && bgs != NULL && fields != NULL && stageModel != NULL && stages != NULL;
    }
    if (ok) {
        memcpy(model, models.data + models.start[CHAMPION_MODEL], modelSize);
        memcpy(bgs, arenas.data + arenas.start[0], bgsSize);
        memcpy(fields, arenas.data + arenas.start[1], fieldsSize);
        // The new field's row: the Champion room's, its model the new one
        memcpy(fields + fieldsSize, fields + CHAMPION_FIELD * FIELD_ROW_SIZE, FIELD_ROW_SIZE);
        fields[fieldsSize] = newModel & 0xFF;
        fields[fieldsSize + 1] = newModel >> 8;
        // Only a Champion room made of that model, with its stage's models (stage ids stay)
        u8 *entry = bgs + CHAMPION_BG * BG_ENTRY_SIZE;
        ok = entry[2 + 0] == CHAMPION_FIELD && Read16Table(fields, CHAMPION_FIELD) == CHAMPION_MODEL && Gen5Galactic_Make(model, modelSize);
        entry[2 + GALACTIC_TERRAIN] = (u8)newField;
        // The new stage: the Champion room's (21), its grounds' model (38, one for both) a dark copy
        memcpy(stageModel, models.data + models.start[STAGE_MODEL], stageModelSize);
        memcpy(stages, arenas.data + arenas.start[2], newStage * STAGE_ROW_SIZE);
        memcpy(stages + newStage * STAGE_ROW_SIZE, stages + CHAMPION_STAGE * STAGE_ROW_SIZE, STAGE_ROW_SIZE);
        u8 *row = stages + newStage * STAGE_ROW_SIZE;
        ok = ok && row[2] == STAGE_MODEL && row[3] == 0 && row[4] == STAGE_MODEL && row[5] == 0
            && Gen5Galactic_MakeStage(stageModel, stageModelSize);
        row[2] = row[4] = (u8)((newModel + 1) & 0xFF);
        row[3] = row[5] = (u8)((newModel + 1) >> 8);
        entry[2 + 20 + GALACTIC_TERRAIN] = (u8)newStage;
    }
    if (ok) {
        const u8 *added[2] = { model, stageModel };
        const u32 addedSize[2] = { modelSize, stageModelSize };
        const u8 *replaced[3] = { bgs, fields, stages };
        const u32 replacedSize[3] = { bgsSize, fieldsSize + FIELD_ROW_SIZE, (newStage + 1) * STAGE_ROW_SIZE };
        const u32 indices[3] = { 0, 1, 2 };
        modelsFile = ArchiveWithAdded(&models, 2, added, addedSize, &modelsFileSize);
        u32 firstSize = 0;
        u8 *first = ArchiveWithMembers(&arenas, 3, indices, replaced, replacedSize, &firstSize);
        arenasFile = first;
        arenasFileSize = firstSize;
        ok = modelsFile != NULL && arenasFile != NULL;
    }
    if (ok) {
        MakeDirs();
        mkdir(DIR "/a/1", 0755);
        mkdir(DIR "/a/1/5", 0755);
        remove(BTLV_PATH);
        remove(BG_DATA_PATH);
        ok = WriteWhole(BTLV_PATH, modelsFile, modelsFileSize) && WriteWhole(BG_DATA_PATH, arenasFile, arenasFileSize);
    }
    free(model);
    free(bgs);
    free(fields);
    free(stageModel);
    free(stages);
    free(modelsFile);
    free(arenasFile);
    if (models.data != NULL) {
        ArchiveClose(&models);
    }
    if (arenas.data != NULL) {
        ArchiveClose(&arenas);
    }
    return ok;
}

BOOL Gen5Data_PrepareGalacticArena(void)
{
    static BOOL sGalacticRegistered;
    if (sGalacticRegistered) {
        return TRUE;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/a/0/1/1", Gen5Rom_Dir());
    u32 romSize = 0;
    {
        FILE *f = fopen(path, "rb");
        if (f == NULL) {
            return FALSE;
        }
        fseek(f, 0, SEEK_END);
        romSize = (u32)ftell(f);
        fclose(f);
    }
    char stamp[64];
    snprintf(stamp, sizeof(stamp), "%d %u\n", GALACTIC_VERSION, romSize);
    u32 stampSize = 0;
    u8 *old = ReadWhole(GALACTIC_STAMP_PATH, &stampSize);
    const BOOL current = old != NULL && stampSize == strlen(stamp) && memcmp(old, stamp, stampSize) == 0;
    free(old);
    if (!current) {
        if (!MakeGalacticArena(Gen5Rom_Dir())) {
            printf("Gen 5: the Team Galactic arena couldn't be made: the Champion room is used\n");
            return FALSE;
        }
        WriteWhole(GALACTIC_STAMP_PATH, (const u8 *)stamp, (u32)strlen(stamp));
        printf("Gen 5: the Team Galactic arena made in %s\n", DIR);
    }
    Gen5_FilesOverride(BTLV_ARC, BTLV_PATH);
    Gen5_FilesOverride(BG_DATA_ARC, BG_DATA_PATH);
    sGalacticRegistered = TRUE;
    return TRUE;
}

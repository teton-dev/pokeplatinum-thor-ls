// Battles in the Gen 5 engine; see include/port/gen5_battle.h
#include <stdio.h>
#include <stdlib.h>

#include "port/gen5_battle.h"

#include <nitro.h>
#include <simulator/sim.h>
#include <stdio.h>
#include <string.h>

#include "constants/battle.h"
#include "constants/battle/condition.h"
#include "constants/charcode.h"
#include "constants/species.h"
#include "constants/string.h"

#include "applications/naming_screen.h"
#include "constants/narc.h"
#include "data/trainer_class_prize_mul.h"
#include "generated/battle_backgrounds.h"
#include "generated/battle_terrains.h"
#include "generated/text_banks.h"
#include "generated/trainer_classes.h"
#include "generated/trainers.h"
#include "generated/trainer_message_types.h"
#include "message.h"
#include "trainer_data.h"
#include "bag.h"
#include "game_options.h"
#include "battle/battle_system.h"
#include "constants/heap.h"
#include "evolution.h"
#include "field_battle_data_transfer.h"
#include "game_records.h"
#include "generated/game_records.h"
#include "heap.h"
#include "pc_boxes.h"
#include "pokedex.h"
#include "poketch.h"
#include "string_gf.h"
#include "tv_segment.h"
#include "gen5.h"
#include "party.h"
#include "pokemon.h"
#include "charcode_convert.h"
#include "port/gen5_rom.h"
#include "port/sim_config_prj.h"
#include "port/test_script.h"
#include "system.h"
#include "trainer_info.h"

// gui_createmon_utils.c: a debug wild battle from the overworld
int GUI_CreateMon_StartBattle(int speciesNo, int level, int trainerID, int doubles);

static BOOL sRequested;
static BOOL sFailed;
// Gen5_BattleBegin ran and Gen5_BattleEnd hasn't yet
static BOOL sShown;

BOOL Gen5Battle_IsShown(void)
{
    return sShown;
}

BOOL Gen5Battle_MoveBgShown(void)
{
    return sShown && Gen5_MoveBgShown();
}

void Gen5Battle_RequestNext(void)
{
    sRequested = TRUE;
}

BOOL Gen5Battle_Wants(const FieldBattleDTO *dto)
{
    if (sRequested) {
        sRequested = FALSE;
        return TRUE;
    }
    if (!SIM_Config_prj_GetConfig()->gen5Battles || Gen5Rom_GameName() == NULL) {
        return FALSE;
    }
    // A single wild battle, with nothing special about it but a honey tree, or a single trainer battle
    if (dto->battleType == BATTLE_TYPE_WILD_MON) {
        return (dto->battleStatusMask & ~BATTLE_STATUS_HONEY_TREE) == 0;
    }
    // The story's trainers (Gym Leaders, the rival, Cynthia, Team Galactic) are started by their scripts as any
    // trainer is, with no flag; Barry's first battle on Route 201 has BATTLE_STATUS_FIRST_BATTLE (Platinum there
    // keeps critical hits out of it and shows only FIGHT; BW2's trainer battles never let the player run)
    return dto->battleType == BATTLE_TYPE_TRAINER && (dto->battleStatusMask & ~BATTLE_STATUS_FIRST_BATTLE) == 0;
}

BOOL Gen5Battle_StartDebug(int species, int level)
{
    if (Gen5Rom_State() != GEN5_ROM_READY && Gen5Rom_GameName() == NULL) {
        printf("Gen 5: no Black 2 or White 2 ROM imported\n");
        return FALSE;
    }
    if (!GUI_CreateMon_StartBattle(species, level, 0, FALSE)) {
        return FALSE;
    }
    Gen5Battle_RequestNext();
    return TRUE;
}

static void VBlank(void *data)
{
    (void)data;
    Gen5_BattleVBlank();
}

// The battle as handed to the Gen 5 code, and as it comes back
static Gen5Battle sBattle;
// The bag's counts before the battle
static Gen5Item sItemCountsBefore[GEN5_ITEMS_MAX];
static int sItemsBefore;
// After a catch: the naming screen
static NamingScreenArgs *sNamingArgs;
static ApplicationManager *sNamingApp;

// Platinum's text to Unicode, for BW2: the letters, digits and punctuation the ASCII table knows, and the accented e
// of Pokémon. Other characters (Japanese, symbols) are left out for now
void Gen5_TextToGen5(const charcode_t *src, int srcLength, unsigned short *dest, int destLength)
{
    int n = 0;
    // Room for the terminator (0xFFFF), and for a page break's three where one goes
    for (int i = 0; i < srcLength && src[i] != CHAR_EOS && n < destLength - 1; i++) {
        // A message's line and page breaks, as BW2 writes them (printsys.c: a new line, and the stream commands that
        // scroll or clear the window); Platinum's other commands (colors, placeholders) are left out
        if (src[i] == CHAR_CR) {
            dest[n++] = 0xFFFE;
            continue;
        }
        if (src[i] == CHAR_CONTROL_SCROLL || src[i] == CHAR_CONTROL_CLEAR) {
            if (n + 3 > destLength - 1) {
                break;
            }
            dest[n++] = 0xF000;
            dest[n++] = 0xBE00 | (src[i] == CHAR_CONTROL_CLEAR);
            dest[n++] = 0;
            continue;
        }
        if (src[i] == CHAR_FORMAT_ARG) {
            // The command, its parameter count, the parameters
            if (i + 2 < srcLength) {
                i += 2 + src[i + 2];
            }
            continue;
        }
        if (src[i] == CHAR_e_ACUTE) {
            dest[n++] = 0xE9;
            continue;
        }
        if (src[i] == CHAR_E_ACUTE) {
            dest[n++] = 0xC9;
            continue;
        }
        char c = '\0';
        CharCode_ToAsciiString((charcode_t *)&src[i], &c, 1);
        if (c != '\0') {
            dest[n++] = (unsigned char)c;
        }
    }
    dest[n] = 0xFFFF;
}

static unsigned StatusToGen5(u32 status)
{
    if (status & MON_CONDITION_SLEEP) {
        return GEN5_STATUS_SLEEP;
    }
    if (status & (MON_CONDITION_POISON | MON_CONDITION_TOXIC)) {
        return GEN5_STATUS_POISON;
    }
    if (status & MON_CONDITION_BURN) {
        return GEN5_STATUS_BURN;
    }
    if (status & MON_CONDITION_FREEZE) {
        return GEN5_STATUS_FREEZE;
    }
    if (status & MON_CONDITION_PARALYSIS) {
        return GEN5_STATUS_PARALYSIS;
    }
    return GEN5_STATUS_NONE;
}

// BW2 keeps no sleep counter or toxic flag outside a battle: a Pokémon asleep sleeps a few turns more, and one that
// was badly poisoned stays so if it is still poisoned
static u32 StatusFromGen5(unsigned status, u32 before)
{
    switch (status) {
    case GEN5_STATUS_SLEEP:
        return (before & MON_CONDITION_SLEEP) ? (before & MON_CONDITION_SLEEP) : 3;
    case GEN5_STATUS_POISON:
        return (before & MON_CONDITION_TOXIC) ? MON_CONDITION_TOXIC : MON_CONDITION_POISON;
    case GEN5_STATUS_BURN:
        return MON_CONDITION_BURN;
    case GEN5_STATUS_FREEZE:
        return MON_CONDITION_FREEZE;
    case GEN5_STATUS_PARALYSIS:
        return MON_CONDITION_PARALYSIS;
    default:
        return MON_CONDITION_NONE;
    }
}

void Gen5_PokemonToGen5(Pokemon *mon, Gen5Pokemon *dest)
{
    charcode_t name[MON_NAME_LEN + 1];
    charcode_t otName[TRAINER_NAME_LEN + 1];

    memset(dest, 0, sizeof(*dest));
    dest->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    dest->form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    dest->level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    dest->exp = Pokemon_GetValue(mon, MON_DATA_EXPERIENCE, NULL);
    dest->personality = Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL);
    dest->otId = Pokemon_GetValue(mon, MON_DATA_OT_ID, NULL);
    dest->heldItem = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);
    dest->ability = Pokemon_GetValue(mon, MON_DATA_ABILITY, NULL);
    dest->friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL);
    dest->gender = Pokemon_GetGender(mon);
    for (int i = 0; i < 4; i++) {
        dest->moves[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1 + i, NULL);
        dest->pp[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1_PP + i, NULL);
        dest->ppUps[i] = Pokemon_GetValue(mon, MON_DATA_MOVE1_PP_UPS + i, NULL);
    }
    dest->hp = Pokemon_GetValue(mon, MON_DATA_HP, NULL);
    dest->status = StatusToGen5(Pokemon_GetValue(mon, MON_DATA_STATUS, NULL));
    for (int i = 0; i < 6; i++) {
        dest->ivs[i] = Pokemon_GetValue(mon, MON_DATA_HP_IV + i, NULL);
        dest->evs[i] = Pokemon_GetValue(mon, MON_DATA_HP_EV + i, NULL);
    }
    dest->ball = Pokemon_GetValue(mon, MON_DATA_POKEBALL, NULL);
    dest->pokerus = Pokemon_GetValue(mon, MON_DATA_POKERUS, NULL);
    dest->metLevel = Pokemon_GetValue(mon, MON_DATA_MET_LEVEL, NULL);
    dest->otGender = Pokemon_GetValue(mon, MON_DATA_OT_GENDER, NULL);
    dest->isEgg = Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL);
    dest->hasNickname = Pokemon_GetValue(mon, MON_DATA_HAS_NICKNAME, NULL);
    Pokemon_GetValue(mon, MON_DATA_NICKNAME, name);
    Gen5_TextToGen5(name, MON_NAME_LEN + 1, dest->nickname, GEN5_NAME_LENGTH);
    Pokemon_GetValue(mon, MON_DATA_OT_NAME, otName);
    Gen5_TextToGen5(otName, TRAINER_NAME_LEN + 1, dest->otName, 8);
}

// What the battle changed. TRUE if the Pokémon gained a level (for evolving after the battle)
static BOOL PokemonFromGen5(Pokemon *mon, const Gen5Pokemon *src)
{
    const u32 level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    const u32 status = Pokemon_GetValue(mon, MON_DATA_STATUS, NULL);
    u32 value;

    Pokemon_SetValue(mon, MON_DATA_EXPERIENCE, &src->exp);
    for (int i = 0; i < 4; i++) {
        Pokemon_SetValue(mon, MON_DATA_MOVE1 + i, &src->moves[i]);
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP_UPS + i, &src->ppUps[i]);
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP + i, &src->pp[i]);
    }
    for (int i = 0; i < 6; i++) {
        value = src->evs[i];
        Pokemon_SetValue(mon, MON_DATA_HP_EV + i, &value);
    }
    Pokemon_SetValue(mon, MON_DATA_FRIENDSHIP, &src->friendship);
    Pokemon_SetValue(mon, MON_DATA_HELD_ITEM, &src->heldItem);
    Pokemon_SetValue(mon, MON_DATA_POKERUS, &src->pokerus);
    Pokemon_CalcLevelAndStats(mon);
    Pokemon_SetValue(mon, MON_DATA_HP, &src->hp);
    value = StatusFromGen5(src->status, status);
    Pokemon_SetValue(mon, MON_DATA_STATUS, &value);
    return Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL) > level;
}

// A Platinum trainer for BW2's battle (milestone 5): its party as Platinum made it (Trainer_Encounter), its class
// Platinum's own front sprite of a trainer class, which BW2 has no sprite for (Sinnoh's Elite Four), made into the art
// the Gen 5 battle draws in its place. Platinum keeps it as an 80x80 picture of 8x8 tiles (4 bits a pixel), laid out
// by the first cell of the class's cell bank (an NCER: OBJ rectangles over the tiles, the tile numbers in units of two
// tiles, 1D mapping); its first 16 colors are the palette. Drawn into a 256x128 texture, in 4 bits a pixel with the
// left pixel in the low nibble, as the DS's textures are. Nothing from the ROM is kept: this is done for each battle
static BOOL TrainerClassHasPlatinumArt(int trainerClass)
{
    return trainerClass >= TRAINER_CLASS_ELITE_FOUR_AARON && trainerClass <= TRAINER_CLASS_ELITE_FOUR_LUCIAN;
}

static void TrainerArtToGen5(int trainerClass, Gen5Trainer *dest)
{
    static const u8 sObjWidth[3][4] = { { 8, 16, 32, 64 }, { 16, 32, 32, 64 }, { 8, 8, 16, 32 } };
    static const u8 sObjHeight[3][4] = { { 8, 16, 32, 64 }, { 8, 8, 16, 32 }, { 16, 32, 32, 64 } };
    const int base = trainerClass * 5; // TRAINER_CLASS_NUM_FILETYPES: tiles, palette, cells, animations, scan
    const u8 *tiles;
    const u8 *palette;
    const u8 *cells;
    const u32 tileSize = NARC_GetMemberSizeByIndexPair(NARC_INDEX_POKETOOL__TRGRA__TRFGRA, base);
    int left = 0, top = 0, right = 0, bottom = 0;

    tiles = Heap_Alloc(HEAP_ID_APPLICATION, tileSize);
    palette = Heap_Alloc(HEAP_ID_APPLICATION, NARC_GetMemberSizeByIndexPair(NARC_INDEX_POKETOOL__TRGRA__TRFGRA, base + 1));
    cells = Heap_Alloc(HEAP_ID_APPLICATION, NARC_GetMemberSizeByIndexPair(NARC_INDEX_POKETOOL__TRGRA__TRFGRA, base + 2));
    NARC_ReadWholeMemberByIndexPair((void *)tiles, NARC_INDEX_POKETOOL__TRGRA__TRFGRA, base);
    NARC_ReadWholeMemberByIndexPair((void *)palette, NARC_INDEX_POKETOOL__TRGRA__TRFGRA, base + 1);
    NARC_ReadWholeMemberByIndexPair((void *)cells, NARC_INDEX_POKETOOL__TRGRA__TRFGRA, base + 2);

    // NCGR: "RGCN" header 16 bytes, "RAHC" section: tiles from 0x30 (0x18 after the section's start at 0x10 + 8).
    // NCLR: colors from 0x28. NCER: "KBEC" section at 0x10; cell entries (16 bytes each) from 0x30, OBJs (6 bytes) after
    const u8 *tileData = tiles + 0x30;
    const int cellCount = *(const u16 *)(cells + 0x18);
    const u8 *cellEntry = cells + 0x30;
    const u8 *objs = cells + 0x30 + cellCount * 16;
    const int objCount = *(const u16 *)cellEntry;
    objs += *(const u32 *)(cellEntry + 4);

    memset(dest->artTexture, 0, sizeof(dest->artTexture));
    for (int i = 0; i < 16; i++) {
        dest->artPalette[i] = *(const u16 *)(palette + 0x28 + i * 2);
    }
    dest->artPalette[0] = 0;
    // The OBJs are placed from the cell's centre (-40..40 for 80x80); the picture starts at its top left
    for (int k = 0; k < objCount; k++) {
        const u16 attr0 = *(const u16 *)(objs + k * 6);
        const u16 attr1 = *(const u16 *)(objs + k * 6 + 2);
        const int shape = (attr0 >> 14) & 3;
        const int size = (attr1 >> 14) & 3;
        int x = attr1 & 0x1FF;
        int y = attr0 & 0xFF;
        if (shape > 2) {
            continue;
        }
        if (x >= 256) {
            x -= 512;
        }
        if (y >= 128) {
            y -= 256;
        }
        if (k == 0) {
            left = x;
            top = y;
            right = x;
            bottom = y;
        }
        left = x < left ? x : left;
        top = y < top ? y : top;
        right = x + sObjWidth[shape][size] > right ? x + sObjWidth[shape][size] : right;
        bottom = y + sObjHeight[shape][size] > bottom ? y + sObjHeight[shape][size] : bottom;
    }
    const int width = right - left;
    const int height = bottom - top;
    for (int k = 0; k < objCount && width <= 256 && height <= 128; k++) {
        const u16 attr0 = *(const u16 *)(objs + k * 6);
        const u16 attr1 = *(const u16 *)(objs + k * 6 + 2);
        const u16 attr2 = *(const u16 *)(objs + k * 6 + 4);
        const int shape = (attr0 >> 14) & 3;
        const int size = (attr1 >> 14) & 3;
        int x = attr1 & 0x1FF;
        int y = attr0 & 0xFF;
        if (shape > 2) {
            continue;
        }
        if (x >= 256) {
            x -= 512;
        }
        if (y >= 128) {
            y -= 256;
        }
        const int w = sObjWidth[shape][size];
        const int h = sObjHeight[shape][size];
        const int firstTile = (attr2 & 0x3FF) * 2;
        for (int py = 0; py < h; py++) {
            for (int px = 0; px < w; px++) {
                const int tile = firstTile + (py / 8) * (w / 8) + px / 8;
                const u32 offset = tile * 32 + (py % 8) * 4 + (px % 8) / 2;
                if (offset >= tileSize - 0x30) {
                    continue;
                }
                const u8 pair = tileData[offset];
                const u8 pixel = (px & 1) ? pair >> 4 : pair & 15;
                const int tx = x - left + px;
                const int ty = y - top + py;
                u8 *out = &dest->artTexture[(ty * 256 + tx) / 2];
                *out |= (tx & 1) ? pixel << 4 : pixel;
            }
        }
    }
    dest->artWidth = width;
    dest->artHeight = height;
    dest->hasArt = width > 0 && height > 0 && width <= 256 && height <= 128;
    Heap_Free((void *)tiles);
    Heap_Free((void *)palette);
    Heap_Free((void *)cells);
}

// name and the line it says when beaten from Platinum's messages, and its class's prize (battle_script.c's table)
static void TrainerToGen5(FieldBattleDTO *dto, Gen5Trainer *dest)
{
    const Trainer *trainer = &dto->trainer[BATTLER_ENEMY_1];
    const int id = dto->trainerIDs[BATTLER_ENEMY_1];
    String *text = String_Init(GEN5_TEXT_LENGTH, HEAP_ID_APPLICATION);

    dest->id = id;
    dest->platinumClass = trainer->header.trainerType;
    dest->aiFlags = trainer->header.aiMask;
    if (TrainerClassHasPlatinumArt(dest->platinumClass)) {
        TrainerArtToGen5(dest->platinumClass, dest);
    }
    for (int i = 0; i < MAX_TRAINER_ITEMS && i < 4; i++) {
        dest->items[i] = trainer->header.items[i];
    }
    dest->prizeBase = dest->platinumClass < NELEMS(sTrainerClassPrizeMul) ? sTrainerClassPrizeMul[dest->platinumClass] : 0;
    // A trainer's name is kept compressed (CHAR_COMPRESSED_MARK first)
    String *packed = String_Init(TRAINER_NAME_LEN + 1, HEAP_ID_APPLICATION);
    String_CopyChars(packed, trainer->name);
    String_Clear(text);
    String_ConcatTrainerName(text, packed);
    String_Free(packed);
    Gen5_TextToGen5(String_GetData(text), String_Length(text) + 1, dest->name, 16);

    MessageLoader *loader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG,
        TEXT_BANK_TRAINER_CLASS_NAMES, HEAP_ID_APPLICATION);
    MessageLoader_GetString(loader, dest->platinumClass, text);
    MessageLoader_Free(loader);
    Gen5_TextToGen5(String_GetData(text), String_Length(text) + 1, dest->className, 32);

    dest->defeatText[0] = 0xFFFF;
    if (Trainer_HasMessageType(id, TRMSG_DEFEAT, HEAP_ID_APPLICATION)) {
        Trainer_LoadMessage(id, TRMSG_DEFEAT, text, HEAP_ID_APPLICATION);
        Gen5_TextToGen5(String_GetData(text), String_Length(text) + 1, dest->defeatText, GEN5_TEXT_LENGTH);
    }
    String_Free(text);

    Party *party = dto->parties[BATTLER_ENEMY_1];
    dest->partyCount = Party_GetCurrentCount(party);
    if (dest->partyCount > GEN5_PARTY_MAX) {
        dest->partyCount = GEN5_PARTY_MAX;
    }
    for (int i = 0; i < dest->partyCount; i++) {
        Gen5_PokemonToGen5(Party_GetPokemonBySlotIndex(party, i), &dest->party[i]);
    }
}

// BW2's arena (its battle background, the field environment's bgType, and the terrain that picks the floor) for
// where Platinum puts the battle. The numbers are the rows of BW2's arena table (a/1/5/1, file 0) and its terrain
// columns, seen by rendering each (tests/scripts/gen5_arena.txt): background 0 sunny grassland, 2 a city's grey sky,
// 4 an indoor or cave's dark walls, 5 the same, darker, 6 overcast, 7 mountains, 8 the sea, 9 and 10 sunset; 11 a
// purple void, 12 to 16 the Elite Four's and Champion's rooms, whatever the terrain. Terrains: 0 to 3 and 5 grass,
// 4 and 10 bare earth, 6 and 9 water, 8 sand, 13 and 14 ice and snow, 15 ash, 17 lava, 19 a stone floor. Terrain 7 has
// no floor in the dark backgrounds, and crashes.
enum {
    ARENA_BG_GRASS = 0,
    ARENA_BG_CITY = 2,
    ARENA_BG_INDOORS = 4,
    ARENA_BG_OVERCAST = 6,
    ARENA_BG_MOUNTAIN = 7,
    ARENA_BG_SEA = 8,
    ARENA_BG_VOID = 11,
    ARENA_BG_TOURNAMENT = 19,
};

static BOOL IsGymLeaderClass(int trainerClass)
{
    return trainerClass == TRAINER_CLASS_LEADER_ROARK || trainerClass == TRAINER_CLASS_LEADER_BYRON
        || (trainerClass >= TRAINER_CLASS_LEADER_GARDENIA && trainerClass <= TRAINER_CLASS_LEADER_VOLKNER);
}

static void ArenaFromDTO(const FieldBattleDTO *dto, int *bg, int *terrain)
{
    const int trainerClass = (dto->battleType & BATTLE_TYPE_TRAINER) ? dto->trainer[BATTLER_ENEMY_1].header.trainerType : -1;

    if (IsGymLeaderClass(trainerClass)) {
        // BW2's Gym Leaders battle on a stone floor in a dark hall (Clay's)
        *bg = ARENA_BG_INDOORS;
        *terrain = 19;
    } else if (trainerClass >= TRAINER_CLASS_ELITE_FOUR_AARON && trainerClass <= TRAINER_CLASS_CHAMPION_CYNTHIA) {
        const int id = dto->trainerIDs[BATTLER_ENEMY_1];
        if (id >= TRAINER_ELITE_FOUR_AARON_REMATCH && id <= TRAINER_ELITE_FOUR_LUCIAN_REMATCH) {
            // The Elite Four's rematches are in BW2's World Tournament's arena with the coloured lights (19); Cynthia
            // keeps her room in every fight
            *bg = ARENA_BG_TOURNAMENT;
        } else {
            // Each has a room of its own in Platinum: Aaron, Bertha, Flint, Lucian, Cynthia; BW2's are 12 to 16
            *bg = 12 + (trainerClass - TRAINER_CLASS_ELITE_FOUR_AARON);
        }
        *terrain = 0;
    } else {
        switch (dto->background) {
        case BACKGROUND_WATER:
            *bg = ARENA_BG_SEA;
            break;
        case BACKGROUND_CITY:
            *bg = ARENA_BG_CITY;
            break;
        case BACKGROUND_MOUNTAIN:
            *bg = ARENA_BG_MOUNTAIN;
            break;
        case BACKGROUND_SNOW:
            *bg = ARENA_BG_OVERCAST;
            break;
        case BACKGROUND_INDOORS_1:
        case BACKGROUND_INDOORS_2:
        case BACKGROUND_INDOORS_3:
        case BACKGROUND_CAVE_1:
        case BACKGROUND_CAVE_2:
        case BACKGROUND_CAVE_3:
            *bg = ARENA_BG_INDOORS;
            break;
        case BACKGROUND_DISTORTION_WORLD:
            *bg = ARENA_BG_VOID;
            break;
        default:
            *bg = ARENA_BG_GRASS;
            break;
        }
        switch (dto->terrain) {
        case TERRAIN_SAND:
            *terrain = 8;
            break;
        case TERRAIN_GRASS:
            *terrain = 5;
            break;
        case TERRAIN_PUDDLE:
            *terrain = 9;
            break;
        case TERRAIN_MOUNTAIN:
        case TERRAIN_CAVE:
        case TERRAIN_GREAT_MARSH:
            *terrain = 10;
            break;
        case TERRAIN_SNOW:
            *terrain = 14;
            break;
        case TERRAIN_ICE:
            *terrain = 13;
            break;
        case TERRAIN_WATER:
            *terrain = 6;
            break;
        case TERRAIN_BUILDING:
        case TERRAIN_BRIDGE:
            *terrain = *bg == ARENA_BG_INDOORS ? 19 : 0;
            break;
        default:
            *terrain = 0;
            break;
        }
    }
    const char *override = getenv("POKEPLATINUM_GEN5_ARENA");
    if (override != NULL) {
        sscanf(override, "%d,%d", bg, terrain);
    }
}

static BOOL Init(ApplicationManager *appMan, int *state)
{
    (void)state;
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);
    Party *party = dto->parties[BATTLER_PLAYER_1];
    TrainerInfo *trainerInfo = dto->trainerInfo[BATTLER_PLAYER_1];

    memset(&sBattle, 0, sizeof(sBattle));
    Gen5_TextToGen5(TrainerInfo_Name(trainerInfo), TRAINER_NAME_LEN + 1, sBattle.player.name, 8);
    ArenaFromDTO(dto, &sBattle.arenaBg, &sBattle.arenaTerrain);
    {
        // The time of day as Platinum has it (its battle backgrounds follow the clock): BW2's arenas that take the
        // zone's light (grass, city, mountain, sea...) change with it. POKEPLATINUM_GEN5_TIME=HOUR[:MINUTE] and
        // POKEPLATINUM_GEN5_ZONE=N override them for tests
        RTCTime now;
        RTC_GetCurrentTime(&now);
        sBattle.hour = now.hour;
        sBattle.minute = now.minute;
        sBattle.zone = 0;
        const char *envTime = getenv("POKEPLATINUM_GEN5_TIME");
        if (envTime != NULL) {
            int h = sBattle.hour, m = 0;
            sscanf(envTime, "%d:%d", &h, &m);
            sBattle.hour = h;
            sBattle.minute = m;
        }
        const char *envZone = getenv("POKEPLATINUM_GEN5_ZONE");
        if (envZone != NULL) {
            sBattle.zone = atoi(envZone);
        }
    }
    sBattle.textSpeed = Options_TextSpeed(dto->options);
    sBattle.setStyle = Options_BattleStyle(dto->options) == OPTIONS_BATTLE_STYLE_SET;
    sBattle.player.id = TrainerInfo_ID(trainerInfo);
    sBattle.player.gender = TrainerInfo_Gender(trainerInfo);
    sBattle.player.money = TrainerInfo_Money(trainerInfo);
    sBattle.player.badges = TrainerInfo_BadgeCount(trainerInfo);
    sBattle.partyCount = Party_GetCurrentCount(party);
    if (sBattle.partyCount > GEN5_PARTY_MAX) {
        sBattle.partyCount = GEN5_PARTY_MAX;
    }
    for (int i = 0; i < sBattle.partyCount; i++) {
        Gen5_PokemonToGen5(Party_GetPokemonBySlotIndex(party, i), &sBattle.party[i]);
    }
    Pokemon *wild = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0);
    sBattle.isTrainer = (dto->battleType & BATTLE_TYPE_TRAINER) != 0;
    if (sBattle.isTrainer) {
        TrainerToGen5(dto, &sBattle.trainer);
    } else {
        Gen5_PokemonToGen5(wild, &sBattle.wild);
    }
    // The pockets with what a battle can use. The DTO's bag goes back into the save after the battle
    const struct {
        BagItem *items;
        int size;
    } pockets[] = {
        { dto->bag->items, ITEM_POCKET_SIZE },
        { dto->bag->medicine, MEDICINE_POCKET_SIZE },
        { dto->bag->pokeballs, POKEBALL_POCKET_SIZE },
        { dto->bag->berries, BERRY_POCKET_SIZE },
        { dto->bag->battleItems, BATTLE_ITEM_POCKET_SIZE },
    };
    for (int p = 0; p < (int)NELEMS(pockets); p++) {
        for (int i = 0; i < pockets[p].size && sBattle.itemCount < GEN5_ITEMS_MAX; i++) {
            if (pockets[p].items[i].item != ITEM_NONE && pockets[p].items[i].quantity != 0) {
                sBattle.items[sBattle.itemCount].item = pockets[p].items[i].item;
                sBattle.items[sBattle.itemCount].count = pockets[p].items[i].quantity;
                sBattle.itemCount++;
            }
        }
    }
    sItemsBefore = sBattle.itemCount;
    memcpy(sItemCountsBefore, sBattle.items, sizeof(sBattle.items));
    // Seen, as Platinum's battle registers a wild Pokémon when it appears (BattleSystem_DexFlagSeen); a trainer's,
    // all of them for now (BW2 doesn't say which it sent out)
    for (int i = 0; i < Party_GetCurrentCount(dto->parties[BATTLER_ENEMY_1]); i++) {
        Pokedex_Encounter(dto->pokedex, Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], i));
    }
    (void)wild;

    sFailed = !Gen5_BattleBegin(&sBattle, Gen5Rom_Dir());
    if (sFailed) {
        printf("Gen 5: the battle couldn't start\n");
    } else {
        SetVBlankCallback(VBlank, NULL);
        sShown = TRUE;
        // BW2's shadows are translucent quads that overlap: the DS doesn't blend polygons of one ID over each other
        SIM_SetTranslucentPolygonIdRule(TRUE);
    }
    return TRUE;
}

// What the battle left, into the DTO, as Platinum's battle leaves it (BattleMain_CopyBattleSysToDTOAndFree)
static void Finish(FieldBattleDTO *dto)
{
    SetVBlankCallback(NULL, NULL);
    sShown = FALSE;
    SIM_SetTranslucentPolygonIdRule(FALSE);
    Gen5_SetWidescreen(1.0f);
    const enum Gen5BattleResult result = sFailed ? GEN5_BATTLE_FAILED : Gen5_BattleEnd(&sBattle);

    if (!sFailed) {
        // The party as the battle left it, which Platinum copies into the save (FieldBattleDTO_UpdateFieldSystem)
        Party *party = dto->parties[BATTLER_PLAYER_1];
        for (int i = 0; i < sBattle.partyCount; i++) {
            Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);
            if (PokemonFromGen5(mon, &sBattle.party[i])) {
                dto->leveledUpMonsMask |= 1 << i;
            }
            printf("Gen 5: party %d after the battle: species %u, level %u, exp %u, HP %u/%u, status %u\n", i,
                (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL), (unsigned)Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL),
                (unsigned)Pokemon_GetValue(mon, MON_DATA_EXPERIENCE, NULL), (unsigned)Pokemon_GetValue(mon, MON_DATA_HP, NULL),
                (unsigned)Pokemon_GetValue(mon, MON_DATA_MAX_HP, NULL), (unsigned)Pokemon_GetValue(mon, MON_DATA_STATUS, NULL));
        }
        // What the battle used up, out of the bag
        for (int i = 0; i < sItemsBefore; i++) {
            const int before = sItemCountsBefore[i].count;
            const int after = sBattle.items[i].count;
            if (after < before) {
                Bag_TryRemoveItem(dto->bag, sBattle.items[i].item, before - after, HEAP_ID_APPLICATION);
            } else if (after > before) {
                Bag_TryAddItem(dto->bag, sBattle.items[i].item, after - before, HEAP_ID_APPLICATION);
            }
        }
        TrainerInfo *trainerInfo = dto->trainerInfo[BATTLER_PLAYER_1];
        if (sBattle.moneyChange > 0) {
            TrainerInfo_GiveMoney(trainerInfo, sBattle.moneyChange);
        } else if (sBattle.moneyChange < 0) {
            TrainerInfo_TakeMoney(trainerInfo, -sBattle.moneyChange);
        }
    }
    switch (result) {
    case GEN5_BATTLE_WON:
        dto->resultMask = BATTLE_RESULT_WIN;
        break;
    case GEN5_BATTLE_LOST:
        dto->resultMask = BATTLE_RESULT_LOSE;
        break;
    case GEN5_BATTLE_CAUGHT:
        dto->resultMask = BATTLE_RESULT_CAPTURED_MON;
        break;
    default:
        // Ran, or failed: back to the overworld as if the player ran
        dto->resultMask = BATTLE_RESULT_PLAYER_FLED;
        break;
    }
}

enum {
    STATE_BATTLE,
    STATE_CATCH,
    STATE_NAMING,
    STATE_CHECK_EVOLUTION,
    STATE_WAIT_FOR_EVOLUTION,
};

// A caught Pokémon, as Platinum's battle takes it (battle_script.c's catch sequence, BattleSystem_DexFlagCaught,
// BattleSystem_SetPokemonCatchData): the wild Pokémon with what the battle changed, the player's catch data, the
// Pokédex, the Pokétch's history, the TV's capture record and the game records
static Pokemon *CaughtPokemon(FieldBattleDTO *dto)
{
    return Party_GetPokemonBySlotIndex(dto->parties[BATTLER_ENEMY_1], 0);
}

static void TakeCaught(FieldBattleDTO *dto)
{
    Pokemon *mon = CaughtPokemon(dto);

    PokemonFromGen5(mon, &sBattle.caughtMon);
    Pokedex_Capture(dto->pokedex, mon);
    Pokemon_SetCatchData(mon, dto->trainerInfo[BATTLER_PLAYER_1], sBattle.caughtMon.ball, dto->mapLabelTextID,
        dto->terrain, HEAP_ID_BATTLE);
    if (dto->poketch != NULL) {
        Poketch_PokemonHistoryEnqueue(dto->poketch, Pokemon_GetBoxPokemon(mon));
    }
    if (dto->captureAttempt != NULL) {
        CaptureAttempt_Init(dto->captureAttempt, mon, dto->resultMask, 1, HEAP_ID_BATTLE);
    }
    GameRecords_IncrementRecordValue(dto->records, RECORD_CAUGHT_POKEMON);
    dto->caughtBattlerIdx = BATTLER_ENEMY_1;
}

// Into the party, or the first box with room as Platinum's battle does (its PP restored, as a boxed Pokémon's is)
static void StoreCaught(FieldBattleDTO *dto)
{
    Pokemon *mon = CaughtPokemon(dto);

    if (Party_AddPokemon(dto->parties[BATTLER_PLAYER_1], mon)) {
        printf("Gen 5: caught species %u, into the party\n", (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL));
        return;
    }
    const u32 box = PCBoxes_FirstEmptyBox(dto->pcBoxes);
    if (box >= MAX_PC_BOXES) {
        // Platinum doesn't let a ball be thrown then; BW2's battle doesn't know Platinum's boxes
        printf("Gen 5: caught species %u, but every box is full\n", (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL));
        return;
    }
    PCBoxes_SetCurrentBox(dto->pcBoxes, box);
    for (int i = 0; i < LEARNED_MOVES_MAX; i++) {
        int pp = Pokemon_GetValue(mon, MON_DATA_MOVE1_MAX_PP + i, NULL);
        Pokemon_SetValue(mon, MON_DATA_MOVE1_PP + i, &pp);
    }
    Pokemon_SetGiratinaFormByHeldItem(mon);
    PCBoxes_TryStoreBoxMonInBox(dto->pcBoxes, box, Pokemon_GetBoxPokemon(mon));
    printf("Gen 5: caught species %u, into box %u\n", (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL),
        (unsigned)box + 1);
}

static BOOL Main(ApplicationManager *appMan, int *state)
{
    FieldBattleDTO *dto = ApplicationManager_Args(appMan);

    switch (*state) {
    case STATE_BATTLE:
        if (!sFailed) {
            // The keys and touch the game read this frame (the test scripts' among them), unchanged by the button
            // mode
            Gen5_SetInput(gSystem.heldKeysRaw, gSystem.touchHeld, gSystem.touchX, gSystem.touchY);
            // Widescreen: BW2's camera as wide as libntr draws the 3D this frame (port/widescreen.c asks for it)
            Gen5_SetWidescreen(SIM_IsWidescreenActive() ? SIM_WidescreenRatio() : 1.0f);
            const BOOL running = Gen5_BattleFrame();
            if (Gen5_TakeEvents() & GEN5_EVENT_COMMAND) {
                TestScript_OnEvent(TEST_EVENT_BATTLE_COMMAND);
            }
            if (running) {
                return FALSE;
            }
        }
        Finish(dto);
        *state = sBattle.caught ? STATE_CATCH : STATE_CHECK_EVOLUTION;
        return FALSE;
    // The caught Pokémon: Platinum's catch data, then its naming screen (left empty, it keeps its species' name)
    case STATE_CATCH: {
        Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_BATTLE, 0x30000);
        TakeCaught(dto);
        Pokemon *mon = CaughtPokemon(dto);
        sNamingArgs = NamingScreenArgs_Init(HEAP_ID_BATTLE, NAMING_SCREEN_TYPE_POKEMON,
            Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL), MON_NAME_LEN, dto->options);
        sNamingArgs->battleMsgID = 0;
        sNamingArgs->monForm = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
        sNamingArgs->pcBoxes = dto->pcBoxes;
        sNamingArgs->monGender = Pokemon_GetValue(mon, MON_DATA_GENDER, NULL);
        sNamingApp = ApplicationManager_New(&gNamingScreenAppTemplate, sNamingArgs, HEAP_ID_BATTLE);
        *state = STATE_NAMING;
        return FALSE;
    }
    case STATE_NAMING:
        if (!ApplicationManager_Exec(sNamingApp)) {
            return FALSE;
        }
        if (sNamingArgs->returnCode == NAMING_SCREEN_CODE_OK) {
            Pokemon_SetValue(CaughtPokemon(dto), MON_DATA_NICKNAME_STRING_AND_FLAG, sNamingArgs->textInputStr);
            GameRecords_IncrementRecordValue(dto->records, RECORD_POKEMON_NICKNAMED);
        }
        NamingScreenArgs_Free(sNamingArgs);
        ApplicationManager_Free(sNamingApp);
        sNamingArgs = NULL;
        sNamingApp = NULL;
        StoreCaught(dto);
        Heap_Destroy(HEAP_ID_BATTLE);
        *state = STATE_CHECK_EVOLUTION;
        return FALSE;
    // The Pokémon that gained a level and evolve now, as after Platinum's battles (battle_main.c)
    case STATE_CHECK_EVOLUTION: {
        int slot;
        int evoType;
        const int targetSpecies = Battle_FindEvolvingPartyMember(dto, &slot, &evoType);
        if (targetSpecies == SPECIES_NONE) {
            return TRUE;
        }
        Heap_Create(HEAP_ID_APPLICATION, HEAP_ID_EVOLUTION, HEAP_SIZE_EVOLUTION);
        Pokemon *mon = Party_GetPokemonBySlotIndex(dto->parties[BATTLER_PLAYER_1], slot);
        dto->postBattleData = Evolution_Begin(dto->parties[BATTLER_PLAYER_1], mon, targetSpecies, dto->options,
            dto->visitedContestHall, dto->pokedex, dto->bag, dto->records, dto->poketch, evoType, 0x1 | 0x2,
            HEAP_ID_EVOLUTION);
        *state = STATE_WAIT_FOR_EVOLUTION;
        return FALSE;
    }
    case STATE_WAIT_FOR_EVOLUTION:
        if (Evolution_IsDone(dto->postBattleData)) {
            Evolution_Free(dto->postBattleData);
            dto->postBattleData = NULL;
            Heap_Destroy(HEAP_ID_EVOLUTION);
            *state = STATE_CHECK_EVOLUTION;
        }
        return FALSE;
    }
    return TRUE;
}

static BOOL Exit(ApplicationManager *appMan, int *state)
{
    (void)appMan;
    (void)state;
    return TRUE;
}

const ApplicationManagerTemplate gGen5BattleApplicationTemplate = {
    .init = Init,
    .main = Main,
    .exit = Exit,
    .overlayID = FS_OVERLAY_ID_NONE,
};

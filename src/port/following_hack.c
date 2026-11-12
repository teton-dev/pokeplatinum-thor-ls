// Following Renegade Platinum; see include/port/following_hack.h and docs/thor/dev/following-renegade.md
#include "port/following_hack.h"

#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants/field/field_effect_renderer.h"
#include "constants/heap.h"
#include "constants/pokemon.h"
#include "generated/pokemon_data_params.h"

#include "field/field_system.h"
#include "overlay005/field_effect_manager.h"
#include "overlay005/field_effect_renderer.h"
#include "overlay005/map_object_anim_cmd.h"
#include "overlay005/object_event_gfx_data.h"
#include "overlay005/ov5_021ECA70.h"
#include "overlay005/ov5_021ECC20.h"
#include "overlay005/ov5_021ECE40.h"

#include "billboard.h"
#include "field_script_context.h"
#include "inlines.h"
#include "map_header.h"
#include "map_object.h"
#include "map_object_move.h"
#include "map_tile_behavior.h"
#include "terrain_collision_manager.h"
#include "generated/movement_actions.h"
#include "struct_defs/struct_020EDF0C.h"
#include "unk_020EDBAC.h"
#include "narc.h"
#include "heap.h"
#include "vram_transfer.h"
#include "overworld_anim_manager.h"
#include "party.h"
#include "player_avatar.h"
#include "pokemon.h"
#include "savedata.h"
#include "script_manager.h"
#include "simple3d.h"
#include "sound_playback.h"
#include "unk_02054884.h"
#include "unk_020655F4.h"

// --- The hack's code blob -----------------------------------------------------------------
//
// data/weather_sys.narc's member 65 (vanilla has 65 members, 0 to 64), loaded by the hack at 0x023C8000.
// It holds ARM code and the tables the hack moved there from arm9 and overlay 5, with new entries.

#define BLOB_MEMBER 65
#define BLOB_BASE 0x023C8000
#define BLOB_SIZE 0x18000

// The addresses in the blob of what the port reads (Following Platinum 2.0, in Renegade Platinum 1.3.0)
#define BLOB_RENDERERS 0x023D2F08     // ObjectEventGfxRendererEntry: id, renderer
#define BLOB_TEXTURES 0x023CFB40      // ObjectEventGfxFileEntry: id, mmodel.narc member
#define BLOB_FRAME_SEQS 0x023D1B48    // ObjectEventGfxFileEntry
#define BLOB_MODEL_ANIMS 0x023D62A0   // id, model, frame sequence, animations
#define BLOB_RENDER_DETAILS 0x023D48D0 // id, packed details
#define BLOB_FOLLOWER_ANIMS 0x023CF998 // the animations of the Pokemon (4 loops and the end)

static u8 *sBlob;
static BOOL sActive;

BOOL FollowingHack_IsActive(void)
{
    return sActive;
}

const u8 *FollowingHack_Blob(void)
{
    return sActive ? sBlob : NULL;
}

static u32 Read32(u32 address)
{
    const u8 *p = sBlob + (address - BLOB_BASE);
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

static BOOL InBlob(u32 address, u32 size)
{
    return address >= BLOB_BASE && address - BLOB_BASE + size <= BLOB_SIZE;
}

// Reads the member of a NARC (BTAF file table, BTNF, GMIF data)
static u8 *ReadNarcMember(const char *path, int member, u32 *sizeOut)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    u8 *data = NULL;
    u8 header[0x1c];
    if (fread(header, 1, sizeof(header), f) == sizeof(header) && memcmp(header, "NARC", 4) == 0
        && memcmp(header + 16, "BTAF", 4) == 0) {
        u32 btafSize = header[20] | (header[21] << 8) | (header[22] << 16) | ((u32)header[23] << 24);
        u32 count = header[24] | (header[25] << 8);
        if ((u32)member < count) {
            u8 entry[8];
            u32 entryOffset = 0x10 + 12 + 8 * member;
            u8 btnf[8];
            u32 btnfOffset = 0x10 + btafSize;
            if (fseek(f, entryOffset, SEEK_SET) == 0 && fread(entry, 1, 8, f) == 8
                && fseek(f, btnfOffset, SEEK_SET) == 0 && fread(btnf, 1, 8, f) == 8) {
                u32 start = entry[0] | (entry[1] << 8) | (entry[2] << 16) | ((u32)entry[3] << 24);
                u32 end = entry[4] | (entry[5] << 8) | (entry[6] << 16) | ((u32)entry[7] << 24);
                u32 btnfSize = btnf[4] | (btnf[5] << 8) | (btnf[6] << 16) | ((u32)btnf[7] << 24);
                u32 gmif = btnfOffset + btnfSize + 8; // GMIF's data
                if (end > start && end - start <= 0x100000 && fseek(f, gmif + start, SEEK_SET) == 0) {
                    data = malloc(end - start);
                    if (data != NULL && fread(data, 1, end - start, f) == end - start) {
                        *sizeOut = end - start;
                    } else {
                        free(data);
                        data = NULL;
                    }
                }
            }
        }
    }
    fclose(f);
    return data;
}

// --- Graphics -------------------------------------------------------------------------------
//
// The hack adds the graphics IDs 276 to 841: a billboard each, for the Pokemon (forms, females and shinies
// included), all drawn by one renderer. Its table entries come after the game's own in each table.

static BillboardAnim sFollowerAnims[5];

static void FollowerRenderer_Update(MapObject *mapObj);

// The renderer of the hack's Pokemon: Platinum's billboard renderer, with the hack's own update, which chooses
// the picture (hk_23CA2BC). Its function for when the map's objects come back (hk_23CA384, which also changes
// the follower's picture when the lead Pokemon has changed) is still Platinum's
static const UnkStruct_ov5_021FB0F0 sFollowerRenderer = {
    ov5_021ECA70,
    FollowerRenderer_Update,
    ov5_021ECABC,
    ov5_021ECAC4,
    ov5_021ECAF0
};

static void ExtendGraphicsTables(void)
{
    // Renderers
    {
        int vanilla = 0;
        while (gObjectEventGfxRenderersTable[vanilla].graphicsID != 0xFFFF) {
            vanilla++;
        }
        int total = 0;
        while (Read32(BLOB_RENDERERS + total * 8) != 0xFFFF) {
            total++;
        }
        ObjectEventGfxRendererEntry *table = malloc((total + 1) * sizeof(*table));
        memcpy(table, gObjectEventGfxRenderersTable, vanilla * sizeof(*table));
        for (int i = vanilla; i < total; i++) {
            table[i].graphicsID = Read32(BLOB_RENDERERS + i * 8);
            table[i].renderer = &sFollowerRenderer;
        }
        table[total].graphicsID = 0xFFFF;
        table[total].renderer = NULL;
        gObjectEventGfxRenderersTable = table;
        printf("Following hack: %d graphics, %d new\n", total, total - vanilla);
    }

    // Textures and frame sequences: ID and member of a NARC
    static const struct {
        u32 address;
        const ObjectEventGfxFileEntry **table;
    } fileTables[] = {
        { BLOB_TEXTURES, &gObjectEventGfxTexturesTable },
        { BLOB_FRAME_SEQS, &gObjectEventGfxFrameSeqsTable },
    };
    for (int t = 0; t < (int)NELEMS(fileTables); t++) {
        const ObjectEventGfxFileEntry *old = *fileTables[t].table;
        int vanilla = 0;
        while (old[vanilla].id != 0xFFFF) {
            vanilla++;
        }
        int total = 0;
        while (Read32(fileTables[t].address + total * 8) != 0xFFFF) {
            total++;
        }
        ObjectEventGfxFileEntry *table = malloc((total + 1) * sizeof(*table));
        memcpy(table, old, vanilla * sizeof(*table));
        for (int i = vanilla; i < total; i++) {
            table[i].id = Read32(fileTables[t].address + i * 8);
            table[i].narcIdx = Read32(fileTables[t].address + i * 8 + 4);
        }
        table[total].id = 0xFFFF;
        table[total].narcIdx = 0;
        *fileTables[t].table = table;
    }

    // Animations of the billboards
    for (int i = 0; i < 4; i++) {
        sFollowerAnims[i].startFrame = Read32(BLOB_FOLLOWER_ANIMS + i * 12);
        sFollowerAnims[i].endFrame = Read32(BLOB_FOLLOWER_ANIMS + i * 12 + 4);
        sFollowerAnims[i].animType = Read32(BLOB_FOLLOWER_ANIMS + i * 12 + 8);
    }
    sFollowerAnims[4].startFrame = 0;
    sFollowerAnims[4].endFrame = 0;
    sFollowerAnims[4].animType = BILLBOARD_ANIM_TYPE_TABLE_END;

    {
        int vanilla = 0;
        while (gObjectEventGfxModelAnimsTable[vanilla].graphicsID != 0xFFFF) {
            vanilla++;
        }
        int total = 0;
        while (Read32(BLOB_MODEL_ANIMS + total * 16) != 0xFFFF) {
            total++;
        }
        ObjectEventGfxModelAnimEntry *table = malloc((total + 1) * sizeof(*table));
        memcpy(table, gObjectEventGfxModelAnimsTable, vanilla * sizeof(*table));
        for (int i = vanilla; i < total; i++) {
            u32 entry = BLOB_MODEL_ANIMS + i * 16;
            table[i].graphicsID = Read32(entry);
            table[i].modelID = Read32(entry + 4);
            table[i].frameSeqID = Read32(entry + 8);
            // Every new entry's animations are the hack's one set
            table[i].animations = sFollowerAnims;
        }
        table[total].graphicsID = 0xFFFF;
        table[total].modelID = BILLBOARD_MODEL_SENTINEL_ID;
        table[total].frameSeqID = BILLBOARD_FRAME_SEQ_SENTINEL_ID;
        table[total].animations = NULL;
        gObjectEventGfxModelAnimsTable = table;
    }

    {
        int vanilla = 0;
        while (gObjectEventGfxRenderDetailsTable[vanilla].graphicsID != 0xFFFF) {
            vanilla++;
        }
        int total = 0;
        while (Read32(BLOB_RENDER_DETAILS + total * 8) != 0xFFFF) {
            total++;
        }
        ObjectEventGfxRenderDetailsEntry *table = calloc(total + 1, sizeof(*table));
        memcpy(table, gObjectEventGfxRenderDetailsTable, vanilla * sizeof(*table));
        for (int i = vanilla; i < total; i++) {
            // The details are packed in one word, as the table's bit fields are
            u32 packed = Read32(BLOB_RENDER_DETAILS + i * 8 + 4);
            table[i].graphicsID = Read32(BLOB_RENDER_DETAILS + i * 8);
            table[i].modelType = packed & 0xF;
            table[i].hasShadow = (packed >> 4) & 0x3;
            table[i].trackType = (packed >> 6) & 0xF;
            table[i].hasReflection = (packed >> 10) & 0x3;
        }
        table[total].graphicsID = 0xFFFF;
        gObjectEventGfxRenderDetailsTable = table;
    }
}

// --- The follower -------------------------------------------------------------------------------
//
// A map object with local ID 0xFD that the hack keeps in the field system (+0xCC the object, +0xE0 on,
// +0xE4 a request to check it, +0xE8 the animation of its ball): here a few static variables.

#define FOLLOWER_LOCAL_ID 0xFD
#define FOLLOWER_MOVEMENT_WALK 0x44 // follows the player
#define FOLLOWER_MOVEMENT_STAND 0x45 // stays where it is (events, bike, surf)
#define FOLLOWER_SCRIPT 0x2904 // what talking to it runs

#define BLOB_SPECIES_GRAPHICS 0x023CE560 // 493 entries: species, graphics ID (10 bits), forms and females
#define BLOB_SPECIES_GRAPHICS_COUNT 493
#define BLOB_SPRITE_OFFSETS 0x023CDC88 // 566 entries: graphics ID, offsets of the picture
#define BLOB_SPRITE_OFFSETS_COUNT 566
#define BLOB_BALL_ANIM_CMDS 0x023DA1A0 // the animation of script command 857

#define FIRST_FOLLOWER_GRAPHICS 276
#define LAST_FOLLOWER_GRAPHICS 841

#define BALL_RENDERER_ID (FIELD_EFFECT_RENDERER_COUNT + 1) // the hack's is 0x22 (its count is one more)

static struct {
    MapObject *object;
    BOOL enabled;
    BOOL request;
    OverworldAnimManager *ballAnim;
} sFollower;

static u8 sScriptByte; // script command 840's (the hack keeps it in the sound system's parameters)

static u16 Read16(u32 address)
{
    const u8 *p = sBlob + (address - BLOB_BASE);
    return p[0] | (p[1] << 8);
}

static u8 Read8(u32 address)
{
    return sBlob[address - BLOB_BASE];
}

static Party *PartyOf(FieldSystem *fieldSystem)
{
    return SaveData_GetParty(fieldSystem->saveData);
}

static BOOL FollowerValid(void)
{
    return sFollower.object != NULL && MapObject_CheckStatus(sFollower.object, MAP_OBJ_STATUS_0)
        && MapObject_GetLocalID(sFollower.object) == FOLLOWER_LOCAL_ID;
}

// The follower's map object (hk_23C81A4)
static MapObject *GetFollower(void)
{
    return FollowerValid() ? sFollower.object : NULL;
}

// It's in the game and a Pokemon can follow (hk_23C81AC)
static BOOL FollowerActive(FieldSystem *fieldSystem)
{
    return sFollower.enabled && FollowerValid() && Party_AliveMonsCount(PartyOf(fieldSystem)) > 0;
}

// The lead Pokemon (hk_23C81D0): the first that can battle
static Pokemon *LeadMon(FieldSystem *fieldSystem)
{
    return Party_FindFirstEligibleBattler(PartyOf(fieldSystem));
}

static u16 LeadSpecies(FieldSystem *fieldSystem)
{
    return Pokemon_GetValue(LeadMon(fieldSystem), MON_DATA_SPECIES, NULL);
}

static u8 LeadForm(FieldSystem *fieldSystem)
{
    return Pokemon_GetValue(LeadMon(fieldSystem), MON_DATA_FORM, NULL);
}

// The graphics ID of a Pokemon (hk_23C8218): 276 to 841
static BOOL IsPokemonGraphics(u32 graphicsID)
{
    return graphicsID >= FIRST_FOLLOWER_GRAPHICS && graphicsID <= LAST_FOLLOWER_GRAPHICS;
}

BOOL FollowingHack_IsPokemonGraphics(u32 graphicsID)
{
    return sActive && IsPokemonGraphics(graphicsID);
}

// The species table's entry of a species; the first entry when it isn't there
static u32 SpeciesEntry(u16 species)
{
    for (int i = 0; i < BLOB_SPECIES_GRAPHICS_COUNT; i++) {
        if (Read16(BLOB_SPECIES_GRAPHICS + i * 6) == species) {
            return BLOB_SPECIES_GRAPHICS + i * 6;
        }
    }
    return BLOB_SPECIES_GRAPHICS;
}

// hk_23C830C: the graphics ID of a species, form and gender
static u16 PokemonGraphicsID(u16 species, int form, int gender)
{
    u32 entry = SpeciesEntry(species);
    u16 graphicsID = Read16(entry + 2) & 0x3FF;
    BOOL hasFemale = Read8(entry + 4) & 1;
    int formCount = (Read8(entry + 4) >> 1) & 0x1F;

    if (hasFemale) {
        return (u16)(graphicsID + gender);
    }
    if (formCount >= form) {
        return (u16)(graphicsID + form);
    }
    return graphicsID;
}

// hk_23C84AC: where a Pokemon may follow
static BOOL AllowedOnMap(u16 species, enum MapHeaderID mapID)
{
    static const u16 noMaps[] = { 2, 0x1C, 0x23, 0x7A, 0x9A, 0x9B, 0x9C, 0xA7, 0xCD, 0x14C, 0x14D, 0x19A, 0x1D2, 0x237, 0x238 };
    for (int i = 0; i < (int)NELEMS(noMaps); i++) {
        if (mapID == noMaps[i]) {
            return FALSE;
        }
    }
    if (mapID >= 0x23D && mapID <= 0x247) {
        return FALSE;
    }
    if (MapHeader_GetMapMatrixID(mapID) == 0xCF) {
        return FALSE;
    }
    if (MapHeader_IsBuilding(mapID) == TRUE) {
        // The big ones stay out of buildings: Wailord, Steelix, Lugia, Ho-Oh, Kyogre, Groudon, Rayquaza,
        // Dialga, Palkia, Regigigas, Giratina, Arceus
        static const u16 bigOnes[] = { 0x141, 0xD0, 0xF9, 0xFA, 0x17E, 0x17F, 0x180, 0x1E3, 0x1E4, 0x1E6, 0x1E7, 0x1ED };
        for (int i = 0; i < (int)NELEMS(bigOnes); i++) {
            if (species == bigOnes[i]) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

// hk_23C8340: the offsets of a graphics ID's picture; the fourth is its kind (2 for the Pokemon)
static void SpriteOffsets(u32 graphicsID, fx32 *a, fx32 *b, fx32 *c, int *kind)
{
    if (graphicsID >= FIRST_FOLLOWER_GRAPHICS - 0) {
        for (int i = 0; i < BLOB_SPRITE_OFFSETS_COUNT; i++) {
            u32 entry = BLOB_SPRITE_OFFSETS + i * 4;
            if (Read16(entry) == graphicsID) {
                *a = (Read8(entry + 2) & 0xF) << 12;
                *b = ((Read8(entry + 2) >> 4) & 0xF) << 12;
                *c = (Read8(entry + 3) & 0xF) << 12;
                *kind = (Read8(entry + 3) >> 4) & 0xF;
                return;
            }
        }
    }
    *a = *b = *c = 0;
    *kind = 1;
}

// hk_23C83B4: the picture moved by its offsets toward where it faces (they keep a big Pokemon on its tile)
static void AddFacingOffset(const MapObject *mapObj, int dir, VecFx32 *offset)
{
    fx32 a, b, c;
    int kind;
    SpriteOffsets(MapObject_GetGraphicsID(mapObj), &a, &b, &c, &kind);
    switch (dir) {
    case DIR_NORTH:
        offset->z += c;
        break;
    case DIR_SOUTH:
        offset->z -= b;
        break;
    case DIR_WEST:
        offset->x += a;
        break;
    case DIR_EAST:
        offset->x -= a;
        break;
    }
}

// hk_23CA2BC, the hack's Pokemon's update each frame: the animation is the way it faces (the hack's four, of two
// pictures each, which step whether it walks or not), advanced once a frame (Platinum's walkers advance twice and
// choose among more animations, which the Pokemon don't have), the picture bobbing (2 units up for 5 frames of 10)
// and moved by its offsets. The follower's palette is its shiny one (the texture's second) when it is shiny
static void FollowerRenderer_Update(MapObject *mapObj)
{
    Billboard *billboard = ov5_021ECB80(mapObj);
    if (ov5_021EDD94(mapObj) == 1 || billboard == NULL) {
        return;
    }

    if (FollowingHack_IsFollower(mapObj) && billboard->gfxSequence.plttIdx != NULL && billboard->animTexture != NULL) {
        const u8 palette = (MapObject_GetDataAt(mapObj, 2) & 0x80) != 0 ? 1 : 0;
        const NNSG3dResDict *palettes = (const NNSG3dResDict *)((const u8 *)billboard->animTexture
            + billboard->animTexture->plttInfo.ofsDict);
        if (palette < palettes->numEntry) {
            for (u32 i = 0; i < billboard->gfxSequence.seqCount; i++) {
                ((u8 *)billboard->gfxSequence.plttIdx)[i] = palette;
            }
        }
    }

    const int dir = MapObject_GetFacingDir(mapObj);
    Billboard_SetAnimNum(billboard, ov5_021EDF18(dir));
    Billboard_AdvanceAnim(billboard, FX32_ONE);
    const int frame = Billboard_GetFrameNum(billboard) / FX32_ONE;

    VecFx32 offset;
    MapObject_GetSpriteJumpOffset(mapObj, &offset);
    offset.y = frame % 10 < 5 ? -FX32_CONST(2) : -0x100;
    AddFacingOffset(mapObj, dir, &offset);
    MapObject_SetSpritePosOffset(mapObj, &offset);

    ov5_021EDEB4(mapObj, billboard);
    ov5_021EDED8(mapObj, billboard);
}

static int PictureKind(const MapObject *mapObj)
{
    fx32 a, b, c;
    int kind;
    SpriteOffsets(MapObject_GetGraphicsID(mapObj), &a, &b, &c, &kind);
    return kind;
}

// hk_23C83B4: moves a position by the picture's offset towards a direction
static void OffsetByPicture(const MapObject *mapObj, int dir, VecFx32 *pos)
{
    fx32 a, b, c;
    int kind;
    SpriteOffsets(MapObject_GetGraphicsID(mapObj), &a, &b, &c, &kind);
    switch (dir) {
    case 0:
        pos->z += c;
        break;
    case 1:
        pos->z -= b;
        break;
    case 2:
        pos->x += a;
        break;
    case 3:
        pos->x -= a;
        break;
    }
}

// hk_23C864C: shown or hidden, with its flag in its data
static void SetFollowerData(MapObject *mapObj, int bit, BOOL on)
{
    int data = MapObject_GetDataAt(mapObj, 2);
    MapObject_SetDataAt(mapObj, on ? (data | bit) : (data & ~bit), 2);
}

static void FollowerSetShown(MapObject *mapObj, BOOL hidden)
{
    MapObject_SetHidden(mapObj, hidden);
    sub_02062DB4(mapObj, hidden);
    SetFollowerData(mapObj, 2, hidden);
}

// hk_23C8668: the follower is there and not hidden
static BOOL FollowerOut(MapObject *mapObj)
{
    return mapObj != NULL && FollowerActive(MapObject_FieldSystem(mapObj)) && !MapObject_IsHidden(mapObj);
}

// hk_23C88AC: puts the follower on the player's tile
static void FollowerToPlayer(FieldSystem *fieldSystem, int dir)
{
    MapObject *player = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);
    if (FollowerActive(fieldSystem)) {
        VecFx32 pos;
        MapObject_GetPosPtr(player, &pos);
        MapObject_SetPosDirFromVec(GetFollower(), &pos, dir);
    }
}

// hk_23C8440: turns it, unless something is in the way
static BOOL FollowerFace(MapObject *mapObj, int mask, int dir)
{
    if (PictureKind(mapObj) == 2) {
        int x = MapObject_GetX(mapObj) - MapObject_GetDxFromDir(dir);
        int y = MapObject_GetY(mapObj);
        int z = MapObject_GetZ(mapObj) - MapObject_GetDzFromDir(dir);
        VecFx32 pos;
        MapObject_GetPosPtr(mapObj, &pos);
        if (sub_02063E18(mapObj, &pos, x, y, z, dir) & mask) {
            return FALSE;
        }
    }
    ov5_021ECDFC(mapObj, dir);
    return TRUE;
}

// --- Coming out of the ball ---------------------------------------------------------------------
//
// A field effect with two models from the field effects' NARC (members 201 and 202 and the animation 203, the
// hack's own): the ball, then the ball opening. Type 0 shows the Pokemon when it opens and plays its cry
// (hk_23C9248); type 1 is the recall, simplified here (the hack fades the Pokemon's palette)

#define BALL_MODEL_CLOSED 201
#define BALL_MODEL_OPEN 202
#define BALL_ANIM 203

typedef struct BallRenderer {
    FieldEffectManager *fieldEffMan;
    Simple3DModel closedModel;
    Simple3DModel openModel;
    Simple3DAnimation anim;
    Simple3DRenderObj closedObj;
    Simple3DRenderObj openObj;
} BallRenderer;

typedef struct BallEffect {
    int state;
    int frames;
    u32 graphicsID;
    u32 localID;
    enum MapHeaderID mapID;
    FieldEffectManager *fieldEffMan;
    BallRenderer *renderer;
    MapObject *mapObj;
    u16 species;
    VecFx32 pos;
    BOOL closedShown;
    BOOL openShown;
} BallEffect;

typedef struct BallEffectArgs {
    FieldEffectManager *fieldEffMan;
    BallRenderer *renderer;
    MapObject *mapObj;
    u16 species;
    VecFx32 pos;
} BallEffectArgs;

static void *BallRenderer_New(FieldEffectManager *fieldEffMan)
{
    BallRenderer *renderer = FieldEffectManager_HeapAllocInit(fieldEffMan, sizeof(BallRenderer), FALSE, 0);
    renderer->fieldEffMan = fieldEffMan;
    FieldEffectManager_LoadModel(fieldEffMan, &renderer->closedModel, 0, BALL_MODEL_CLOSED, FALSE);
    FieldEffectManager_LoadModel(fieldEffMan, &renderer->openModel, 0, BALL_MODEL_OPEN, FALSE);
    FieldEffectManager_LoadAnimation(fieldEffMan, &renderer->anim, 0, BALL_ANIM, FALSE);
    return renderer;
}

static void BallRenderer_Free(void *data)
{
    BallRenderer *renderer = data;
    Simple3D_FreeModel(&renderer->closedModel);
    Simple3D_FreeModel(&renderer->openModel);
    Simple3D_FreeAnimation(&renderer->anim);
    FieldEffectManager_HeapFree(renderer);
}

static const FieldEffectRendererFuncs sBallRendererFuncs = { BALL_RENDERER_ID, BallRenderer_New, BallRenderer_Free };

static int BallEffect_Init(OverworldAnimManager *manager, void *context)
{
    BallEffect *effect = context;
    const BallEffectArgs *args = OverworldAnimManager_GetUserData(manager);

    effect->fieldEffMan = args->fieldEffMan;
    effect->renderer = args->renderer;
    effect->mapObj = args->mapObj;
    effect->species = args->species;
    effect->pos = args->pos;
    effect->graphicsID = MapObject_GetGraphicsID(effect->mapObj);
    effect->localID = MapObject_GetLocalID(effect->mapObj);
    effect->mapID = MapObject_GetMapHeaderID(effect->mapObj);
    effect->state = 0;
    effect->frames = 0;
    effect->closedShown = TRUE;
    effect->openShown = FALSE;

    OverworldAnimManager_SetPosition(manager, &effect->pos);
    Simple3D_CreateRenderObject(&effect->renderer->closedObj, &effect->renderer->closedModel);
    FieldEffectManager_ApplyAnimCopyToModel(effect->fieldEffMan, &effect->renderer->anim, &effect->renderer->openModel, &effect->renderer->anim, 0);
    Simple3D_CreateRenderObjectWithAnim(&effect->renderer->openObj, &effect->renderer->openModel, &effect->renderer->anim);
    return 1;
}

static void BallEffect_Exit(OverworldAnimManager *manager, void *context)
{
}

static BOOL BallEffect_ObjectGone(const BallEffect *effect)
{
    return sub_020627B4(effect->mapObj, effect->graphicsID, effect->localID, effect->mapID) == 0;
}

static void BallEffect_Tick(OverworldAnimManager *manager, void *context)
{
    BallEffect *effect = context;

    if (BallEffect_ObjectGone(effect)) {
        FieldEffectManager_FinishAnimManager(manager);
        return;
    }

    switch (effect->state) {
    case 0:
        if (++effect->frames >= 2) {
            effect->state = 1;
        }
        break;
    case 1:
            Sound_PlayEffect(0x5E8);
        effect->closedShown = FALSE;
        effect->openShown = TRUE;
        if (effect->localID == FOLLOWER_LOCAL_ID) {
            FollowerSetShown(effect->mapObj, FALSE);
        } else {
            MapObject_SetHidden(effect->mapObj, FALSE);
        }
        sub_02062B68(effect->mapObj);
        Simple3D_SetAnimFrame(&effect->renderer->anim, 0);
        effect->state = 2;
        if (Simple3D_UpdateAnim(&effect->renderer->anim, FX32_ONE, FALSE) == TRUE) {
            FieldEffectManager_FinishAnimManager(manager);
        }
        break;
    case 2:
        if (Simple3D_UpdateAnim(&effect->renderer->anim, FX32_ONE, FALSE) == TRUE) {
            Sound_PlayPokemonCry(effect->species, 0);
            FieldEffectManager_FinishAnimManager(manager);
        }
        break;
    }
}

static void BallEffect_Render(OverworldAnimManager *manager, void *context)
{
    BallEffect *effect = context;
    VecFx32 pos;

    if (BallEffect_ObjectGone(effect)) {
        FieldEffectManager_FinishAnimManager(manager);
        return;
    }
    OverworldAnimManager_GetPosition(manager, &pos);
    if (effect->closedShown) {
        Simple3D_DrawRenderObjWithPos(&effect->renderer->closedObj, &pos);
    }
    if (effect->openShown) {
        Simple3D_DrawRenderObjWithPos(&effect->renderer->openObj, &pos);
    }
}

static const OverworldAnimManagerFuncs sBallEffectFuncs = {
    sizeof(BallEffect),
    BallEffect_Init,
    BallEffect_Exit,
    BallEffect_Tick,
    BallEffect_Render,
};

// The recall (type 1, hk_23C9490 and the two after it): the Pokemon shrinks and its palette fades to white over 6
// frames, then it is hidden and the closed ball fades out over 6 more
#define RECALL_FRAMES 6

// hk_23C96CC: a color moved towards another by t sixteenths
static u16 RecallBlendColor(u16 color, u16 target, int t)
{
    if (t > 16) {
        t = 16;
    }
    const int r = color & 0x1F, g = (color >> 5) & 0x1F, b = (color >> 10) & 0x1F;
    const int r2 = target & 0x1F, g2 = (target >> 5) & 0x1F, b2 = (target >> 10) & 0x1F;
    return (u16)((r + (r2 - r) * t / 16) | ((g + (g2 - g) * t / 16) << 5) | ((b + (b2 - b) * t / 16) << 10));
}

// hk_23CB2A8: the member of mmodel.narc with a graphics ID's picture
static int TextureMember(u32 graphicsID)
{
    for (const ObjectEventGfxFileEntry *entry = gObjectEventGfxTexturesTable; entry->id != 0xFFFF; entry++) {
        if ((u32)entry->id == graphicsID) {
            return entry->narcIdx;
        }
    }
    return -1;
}

typedef struct BallRecall {
    BallEffect effect;
    u16 palette[32];   // the picture's own colors (the last 64 bytes of its member)
    u16 faded[32];
    BOOL havePalette;
} BallRecall;

static void RecallSendPalette(Billboard *billboard, u16 *colors)
{
    if (billboard != NULL) {
        u32 size = NNS_GfdGetPlttKeySize(billboard->plttKey);
        VramTransfer_Request(NNS_GFD_DST_3D_TEX_PLTT, NNS_GfdGetPlttKeyAddr(billboard->plttKey), colors, size < 64 ? size : 64);
    }
}

static int BallRecall_Init(OverworldAnimManager *manager, void *context)
{
    BallRecall *recall = context;
    BallEffect_Init(manager, context);
    recall->effect.closedShown = FALSE;
    recall->effect.openShown = FALSE;
    recall->havePalette = FALSE;
    return 1;
}

static void BallRecall_Tick(OverworldAnimManager *manager, void *context)
{
    BallRecall *recall = context;
    BallEffect *effect = &recall->effect;

    if (BallEffect_ObjectGone(effect)) {
        FieldEffectManager_FinishAnimManager(manager);
        return;
    }
    Billboard *billboard = ov5_021ECB80(effect->mapObj);
    VecFx32 scale = { FX32_ONE, FX32_ONE, FX32_ONE };

    switch (effect->state) {
    case 0: {
        const int member = TextureMember(MapObject_GetGraphicsID(effect->mapObj));
        if (member >= 0) {
            const u32 size = NARC_GetMemberSizeByIndexPair(NARC_INDEX_DATA__MMODEL__MMODEL, member);
            u8 *data = NARC_AllocAndReadWholeMemberByIndexPair(NARC_INDEX_DATA__MMODEL__MMODEL, member, HEAP_ID_FIELD1);
            if (data != NULL && size >= sizeof(recall->palette)) {
                memcpy(recall->palette, data + size - sizeof(recall->palette), sizeof(recall->palette));
                recall->havePalette = TRUE;
            }
            Heap_Free(data);
        }
        Sound_PlayEffect(0x5FE);
        effect->state = 1;
        effect->frames = 0;
        break;
    }
    case 1:
        if (recall->havePalette) {
            for (int i = 0; i < 32; i++) {
                recall->faded[i] = RecallBlendColor(recall->palette[i], 0x7FFF, effect->frames * 3);
            }
            RecallSendPalette(billboard, recall->faded);
        }
        scale.x = FX32_ONE - effect->frames * (FX32_ONE / 8);
        scale.y = scale.x;
        if (billboard != NULL) {
            Billboard_SetScale(billboard, &scale);
        }
        if (++effect->frames >= RECALL_FRAMES) {
            effect->state = 2;
            effect->frames = 0;
            effect->closedShown = TRUE;
            if (recall->havePalette) {
                RecallSendPalette(billboard, recall->palette);
            }
            MapObject_SetHidden(effect->mapObj, TRUE);
        }
        break;
    case 2:
        NNS_G3dMdlSetMdlAlphaAll(effect->renderer->closedModel.g3DModel, 31 - effect->frames * 5);
        if (++effect->frames >= RECALL_FRAMES) {
            NNS_G3dMdlSetMdlAlphaAll(effect->renderer->closedModel.g3DModel, 31);
            if (billboard != NULL) {
                Billboard_SetScale(billboard, &scale);
            }
            FieldEffectManager_FinishAnimManager(manager);
        }
        break;
    }
}

static void BallRecall_Render(OverworldAnimManager *manager, void *context)
{
    BallEffect *effect = context;
    VecFx32 pos;

    if (BallEffect_ObjectGone(effect) || !effect->closedShown) {
        return;
    }
    OverworldAnimManager_GetPosition(manager, &pos);
    Simple3D_DrawRenderObjWithPos(&effect->renderer->closedObj, &pos);
}

static const OverworldAnimManagerFuncs sBallRecallFuncs = {
    sizeof(BallRecall),
    BallRecall_Init,
    BallEffect_Exit,
    BallRecall_Tick,
    BallRecall_Render,
};

// hk_23C9248: the Pokemon comes out of its ball (type 0) or goes back into it (type 1)
static OverworldAnimManager *StartBallEffect(MapObject *mapObj, int type, u16 species)
{
    FieldEffectManager *fieldEffMan = MapObject_GetFieldEffectManager(mapObj);
    BallEffectArgs args;
    VecFx32 pos;

    MapObject_GetPosPtr(mapObj, &pos);
    pos.z += 6 * FX32_ONE;
    OffsetByPicture(mapObj, MapObject_GetFacingDir(mapObj), &pos);

    args.fieldEffMan = fieldEffMan;
    args.renderer = FieldEffectManager_GetRendererContext(fieldEffMan, BALL_RENDERER_ID);
    args.mapObj = mapObj;
    args.species = species;
    args.pos = pos;

    int priority = MapObject_CalculateTaskPriority(mapObj, 2);
    if (type == 0) {
        return FieldEffectManager_InitAnimManager(fieldEffMan, &sBallEffectFuncs, &pos, 1, &args, priority);
    }
    if (type == 1) {
        return FieldEffectManager_InitAnimManager(fieldEffMan, &sBallRecallFuncs, &pos, 1, &args, priority);
    }
    return NULL;
}

// hk_23CBA98 (sub_02063F00): an object at a tile. Objects that let others through (status 18) don't count, but the
// follower blocks everyone except the player; a big Pokemon (picture kind 2) also takes the tile behind it.
static BOOL NearHeight(const MapObject *obj, int y)
{
    int dy = MapObject_GetY(obj) - y;
    return (dy < 0 ? -dy : dy) < 2;
}

BOOL FollowingHack_ObjectAt(const MapObject *mapObj, int x, int y, int z)
{
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);
    const MapObject *obj = MapObjectMan_GetMapObjectConst(mapObjMan);
    int maxObjects = MapObjectMan_GetMaxObjects(mapObjMan);

    do {
        if (obj != mapObj && MapObject_CheckStatus(obj, MAP_OBJ_STATUS_0)
            && (!MapObject_CheckStatus(obj, MAP_OBJ_STATUS_18)
                || (MapObject_GetLocalID(obj) == FOLLOWER_LOCAL_ID && MapObject_GetLocalID(mapObj) != 0xFF))) {
            const int objX = MapObject_GetX(obj);
            const int objZ = MapObject_GetZ(obj);
            int dx = 0, dz = 0;
            if (PictureKind(obj) == 2) {
                dx = MapObject_GetDxFromDir(MapObject_GetFacingDir(obj));
                dz = MapObject_GetDzFromDir(MapObject_GetFacingDir(obj));
            }
            if ((objX == x || dx == objX - x) && (objZ == z || dz == objZ - z) && NearHeight(obj, y)) {
                return TRUE;
            }
            if (MapObject_GetXPrev(obj) == x && MapObject_GetZPrev(obj) == z && NearHeight(obj, y)) {
                return TRUE;
            }
        }
        sub_02062880(&obj);
        maxObjects--;
    } while (maxObjects);

    return FALSE;
}

// --- The follower's movement ------------------------------------------------------------------------
//
// Movement types 0x44 (on foot: hk_23CA46C, its steps hk_23CA4D0 and hk_23CA658) and 0x45 (on a bike or water:
// hk_23CA86C, it waits out of sight). The follower moves only when the player starts a step: the player's step
// functions record the action and the player's tile (hk_23C884C, the field system's +0xD0 to +0xDC in the hack), and
// one frame later the follower takes the player's last tile with the same speed. A ledge jump becomes a slow walk to
// the ledge, then a hop over it at the next step (actions 154 to 157). A hidden follower comes back at the player's
// next step, out of its ball when its data says so.

#define FOLLOWER_DATA_HIDDEN 0x2       // hk_23C86C0
#define FOLLOWER_DATA_HOP 0x8          // a hop over a ledge at the next step (hk_23C8704), its direction in bits 4-5
#define FOLLOWER_DATA_BALL 0x40        // comes back out of its ball (hk_23C87AC)
#define FOLLOWER_DATA_HOPPING 0x100    // the hop is under way (hk_23C883C)

static struct {
    u32 action;
    int x;
    int z;
    int state; // 0 nothing, 1 a step was recorded, 2 the follower moves this frame, 3 it has
} sStep;

typedef struct FollowerMovement {
    u8 step;
    u8 init;
    s16 playerX;
    s16 playerZ;
    u16 unused;
} FollowerMovement;

// Actions that move the player from one tile to another
static BOOL IsStepAction(u32 action)
{
    return (action >= MOVEMENT_ACTION_WALK_SLOWER_NORTH && action < MOVEMENT_ACTION_WALK_ON_SPOT_SLOWER_NORTH)
        || (action >= MOVEMENT_ACTION_JUMP_NEAR_FAST_NORTH && action <= MOVEMENT_ACTION_JUMP_FAR_NORTH + 3)
        || (action >= MOVEMENT_ACTION_WALK_SLIGHTLY_FAST_NORTH && action <= MOVEMENT_ACTION_RUN_NORTH + 3)
        || (action >= MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_NORTH && action <= MOVEMENT_ACTION_WALK_EVER_SO_SLIGHTLY_FAST_NORTH + 3);
}

// hk_23C884C, from the player's step functions
void FollowingHack_PlayerStep(MapObject *player, u32 action)
{
    if (!sActive || player == NULL || !IsStepAction(action)) {
        return;
    }
    sStep.action = action;
    sStep.x = MapObject_GetXPrev(player);
    sStep.z = MapObject_GetZPrev(player);
    if (sStep.state == 0) {
        sStep.state = 1;
    } else if (sStep.state == 3) {
        sStep.state = 2;
    }
}

static BOOL FollowerData(MapObject *mapObj, int bit)
{
    return (MapObject_GetDataAt(mapObj, 2) & bit) != 0;
}

// hk_23CA67C: the player is there; the first time, its tile is noted
static BOOL FollowerMovement_Check(MapObject *mapObj, FollowerMovement *data)
{
    MapObject *player = MapObjectMan_GetPlayerMapObject(MapObject_MapObjectManager(mapObj));
    if (player == NULL) {
        data->init = 0;
        return FALSE;
    }
    if (data->init == 0) {
        PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(MapObject_FieldSystem(mapObj));
        data->init = 1;
        data->playerX = PlayerAvatar_GetXPos(playerAvatar);
        data->playerZ = PlayerAvatar_GetZPos(playerAvatar);
        data->unused = 0xFF;
    }
    return TRUE;
}

// hk_23CA724: the follower's action for the player's (running is a fast walk; a ledge jump a slow walk, then a hop)
static u32 FollowerAction(MapObject *mapObj)
{
    u32 action = sStep.action;
    if (action >= MOVEMENT_ACTION_RUN_NORTH && action <= MOVEMENT_ACTION_RUN_NORTH + 3) {
        return MOVEMENT_ACTION_WALK_FAST_NORTH + (action - MOVEMENT_ACTION_RUN_NORTH);
    }
    if (action >= MOVEMENT_ACTION_JUMP_FAR_NORTH && action <= MOVEMENT_ACTION_JUMP_FAR_NORTH + 3) {
        const int dir = action - MOVEMENT_ACTION_JUMP_FAR_NORTH;
        int data = MapObject_GetDataAt(mapObj, 2) & ~0x30;
        MapObject_SetDataAt(mapObj, data | FOLLOWER_DATA_HOP | (dir << 4), 2);
        return MOVEMENT_ACTION_WALK_SLOW_NORTH + dir;
    }
    return action;
}

// hk_23CA7B0: the follower goes to the player's last tile
static BOOL FollowerMovement_Step(MapObject *mapObj)
{
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(MapObject_FieldSystem(mapObj));
    const int x = MapObject_GetX(mapObj);
    const int z = MapObject_GetZ(mapObj);
    const int playerX = PlayerAvatar_XPosPrev(playerAvatar);
    const int playerZ = PlayerAvatar_ZPosPrev(playerAvatar);
    u32 action;

    if (x == playerX && z == playerZ) {
        FollowerAction(mapObj);
        return FALSE;
    }
    if (FollowerData(mapObj, FOLLOWER_DATA_HOP)) {
        action = MovementAction_TurnActionTowardsDir((MapObject_GetDataAt(mapObj, 2) >> 4) & 3, 154);
        SetFollowerData(mapObj, FOLLOWER_DATA_HOP, FALSE);
        SetFollowerData(mapObj, FOLLOWER_DATA_HOPPING, TRUE);
        FollowerAction(mapObj);
    } else {
        const u32 walk = FollowerAction(mapObj);
        action = MovementAction_TurnActionTowardsDir(GetDirectionBetweenPoints(x, z, playerX, playerZ), walk);
    }
    if (x != sStep.x || z != sStep.z) {
        sub_02065668(mapObj, action);
    }
    return TRUE;
}

// hk_23C898C: the follower stands on a door or warp it faces
static BOOL FollowerOnWarp(MapObject *mapObj)
{
    const int dir = MapObject_GetFacingDir(mapObj);
    const u8 here = TerrainCollisionManager_GetTileBehavior(MapObject_FieldSystem(mapObj), MapObject_GetX(mapObj), MapObject_GetZ(mapObj));
    const u8 north = MapObject_GetTileBehaviorFromDir(mapObj, 0);

    switch (dir) {
    case 1:
        return TileBehavior_IsDoor(north) || TileBehavior_IsWarpNorth(north) || TileBehavior_IsWarpNorth(here);
    case 0:
        return TileBehavior_IsWarpEntranceSouth(north);
    case 2:
        return TileBehavior_IsWarpEast(here) || TileBehavior_IsWarpStairsEast(here);
    case 3:
        return TileBehavior_IsWarpWest(here) || TileBehavior_IsWarpStairsWest(here);
    }
    return FALSE;
}

// hk_23CA4D0
static int FollowerMovement_Wait(MapObject *mapObj, FollowerMovement *data)
{
    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    switch (sStep.state) {
    case 1:
        sStep.state = 2;
        return FALSE;
    case 3:
        sStep.state = 0;
        return FALSE;
    case 2:
        break;
    default:
        return FALSE;
    }

    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(MapObject_FieldSystem(mapObj));
    data->playerX = PlayerAvatar_GetXPos(playerAvatar);
    data->playerZ = PlayerAvatar_GetZPos(playerAvatar);

    if (FollowerData(mapObj, FOLLOWER_DATA_HIDDEN)) {
        if (FollowerData(mapObj, FOLLOWER_DATA_BALL)) {
            const int dir = MapObject_GetFacingDir(mapObj);
            MapObject *player = PlayerAvatar_GetMapObject(playerAvatar);
            MapObject_SetPosDirFromCoords(mapObj, PlayerAvatar_XPosPrev(playerAvatar), MapObject_GetYPrev(player), PlayerAvatar_ZPosPrev(playerAvatar), dir);
            MapObject_RecalculateObjectHeight(mapObj);
            if (!FollowerOnWarp(mapObj) && !FollowerFace(mapObj, 6, dir)) {
                if (dir == 0 || dir == 1) {
                    FollowerFace(mapObj, 6, 3);
                    FollowerFace(mapObj, 6, 2);
                } else {
                    FollowerFace(mapObj, 6, 0);
                    FollowerFace(mapObj, 6, 1);
                }
            }
            StartBallEffect(mapObj, 0, (u16)MapObject_GetDataAt(mapObj, 0));
            SetFollowerData(mapObj, FOLLOWER_DATA_BALL, FALSE);
        } else {
            FollowerSetShown(mapObj, FALSE);
        }
    }

    if (FollowerMovement_Step(mapObj)) {
        sub_02062D04(mapObj);
        if (FollowerData(mapObj, FOLLOWER_DATA_HOPPING)) {
            // The hack shakes the screen here for some heavy Pokemon (hk_23C8BD0): not ported
            SetFollowerData(mapObj, FOLLOWER_DATA_HOPPING, FALSE);
        }
        sStep.state++;
        data->step++;
        return TRUE;
    }
    sStep.state++;
    return FALSE;
}

// hk_23CA658
static int FollowerMovement_Moving(MapObject *mapObj, FollowerMovement *data)
{
    if (sub_020658DC(mapObj) == TRUE) {
        sub_02062D10(mapObj);
        sub_02062D80(mapObj, 0);
        data->step = 0;
    }
    return FALSE;
}

static int (*const sFollowerSteps[])(MapObject *, FollowerMovement *) = {
    FollowerMovement_Wait,
    FollowerMovement_Moving,
};

static void FollowerWalk_Init(MapObject *mapObj)
{
    FollowerMovement *data = sub_02062A54(mapObj, sizeof(FollowerMovement));
    FollowerMovement_Check(mapObj, data);
    sub_02062A0C(mapObj, 0);
    sub_02062D10(mapObj);
    sub_02062D80(mapObj, 0);
}

// On a bike or on the water the follower is in its ball: the hack switches it to movement 0x45 from its hooks in
// the bike's and Surf's code (not ported); the port does it here, from the player's state. It goes back with the
// recall effect, and comes back out of its ball at the player's first step on foot
static BOOL sRideHidden;

static BOOL PlayerRides(MapObject *mapObj)
{
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(MapObject_FieldSystem(mapObj));
    if (playerAvatar == NULL) {
        return FALSE;
    }
    const int state = PlayerAvatar_GetPlayerState(playerAvatar);
    return state == PLAYER_AVATAR_CYCLING || state == PLAYER_AVATAR_SURFING;
}

static void FollowerStartRide(MapObject *mapObj)
{
    if (!MapObject_IsHidden(mapObj) && !FollowerData(mapObj, FOLLOWER_DATA_HIDDEN)) {
        StartBallEffect(mapObj, 1, (u16)MapObject_GetDataAt(mapObj, 0));
    }
    SetFollowerData(mapObj, FOLLOWER_DATA_HIDDEN | FOLLOWER_DATA_BALL, TRUE);
    SetFollowerData(mapObj, FOLLOWER_DATA_HOP | FOLLOWER_DATA_HOPPING, FALSE);
    sStep.state = 0;
    sRideHidden = TRUE;
    MapObject_SwitchMovementType(mapObj, FOLLOWER_MOVEMENT_STAND);
}

static void FollowerWalk_Update(MapObject *mapObj)
{
    FollowerMovement *data = sub_02062A78(mapObj);
    if (!FollowerMovement_Check(mapObj, data)) {
        return;
    }
    if (PlayerRides(mapObj)) {
        FollowerStartRide(mapObj);
        return;
    }
    sub_02062D80(mapObj, 0);
    while (sFollowerSteps[data->step](mapObj, data) == TRUE) {
    }
}

// Movement 0x45 (hk_23CA8C8): out of sight while the player rides or surfs
static int FollowerStand_Wait(MapObject *mapObj, FollowerMovement *data)
{
    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(MapObject_FieldSystem(mapObj));
    // Not while the recall effect still shows it
    if (playerAvatar != NULL && MapObject_IsHidden(mapObj) && (PlayerAvatar_GetXPos(playerAvatar) != data->playerX || PlayerAvatar_GetZPos(playerAvatar) != data->playerZ)) {
        const VecFx32 away = { 0, 0, 0 };
        MapObject_SetPosDirFromVec(mapObj, &away, 1);
        data->playerX = PlayerAvatar_GetXPos(playerAvatar);
        data->playerZ = PlayerAvatar_GetZPos(playerAvatar);
    }
    return FALSE;
}

static void FollowerStand_Update(MapObject *mapObj)
{
    FollowerMovement *data = sub_02062A78(mapObj);
    if (!FollowerMovement_Check(mapObj, data)) {
        return;
    }
    // Off the bike or out of the water: it follows again, out of its ball at the next step
    if (sRideHidden && !PlayerRides(mapObj)) {
        sRideHidden = FALSE;
        sStep.state = 0;
        MapObject_SwitchMovementType(mapObj, FOLLOWER_MOVEMENT_WALK);
        return;
    }
    while (data->step == 0 && FollowerStand_Wait(mapObj, data) == TRUE) {
    }
}

static void FollowerMovement_Delete(MapObject *mapObj)
{
}

static const UnkStruct_020EDF0C sFollowerWalkMovement = { 0x3, FollowerWalk_Init, FollowerWalk_Update, FollowerMovement_Delete, sub_020633EC };
static const UnkStruct_020EDF0C sFollowerStandMovement = { 0x3, FollowerWalk_Init, FollowerStand_Update, FollowerMovement_Delete, sub_020633EC };

const UnkStruct_020EDF0C *FollowingHack_MovementType(u32 type)
{
    if (!sActive) {
        return NULL;
    }
    if (type == FOLLOWER_MOVEMENT_WALK) {
        return &sFollowerWalkMovement;
    }
    if (type == FOLLOWER_MOVEMENT_STAND) {
        return &sFollowerStandMovement;
    }
    return NULL;
}

// --- Spawning ------------------------------------------------------------------------------------

// hk_23C8894
static void ResetFollower(void)
{
    memset(&sFollower, 0, sizeof(sFollower));
}

// hk_23C80F8: the map object
static MapObject *AddFollowerObject(const MapObjectManager *mapObjMan, u16 species, int form, int gender, int dir, int x, int z, BOOL shiny)
{
    u16 graphicsID = PokemonGraphicsID(species, form, gender);

    MapObject *mapObj = MapObjectMan_AddMapObject(mapObjMan, x, z, dir, graphicsID, FOLLOWER_MOVEMENT_WALK, 1);
    if (mapObj == NULL) {
        return NULL;
    }
    MapObject_SetLocalID(mapObj, FOLLOWER_LOCAL_ID);
    MapObject_SetTrainerType(mapObj, 0);
    MapObject_SetFlag(mapObj, 0);
    MapObject_SetScript(mapObj, FOLLOWER_SCRIPT);
    MapObject_SetDataAt(mapObj, species, 0);
    MapObject_SetDataAt(mapObj, 0, 1);
    MapObject_SetDataAt(mapObj, 0, 2);
    MapObject_SetMovementRangeX(mapObj, -1);
    MapObject_SetMovementRangeZ(mapObj, -1);
    MapObject_SetStatusFlagOn(mapObj, 0x42400);
    MapObject_SetStatusFlagOff(mapObj, 0x600);
    MapObject_SetDynamicHeightCalculationEnabled(mapObj, TRUE);
    FollowerSetShown(mapObj, TRUE);
    (void)shiny;
    return mapObj;
}

// hk_23C8000: the lead Pokemon comes out at a tile (hidden: the scripts show it)
static MapObject *SpawnFollower(FieldSystem *fieldSystem, int x, int z, int dir, enum MapHeaderID mapID)
{
    ResetFollower();

    Party *party = PartyOf(fieldSystem);
    if (Party_GetCurrentCount(party) == 0) {
        return NULL;
    }
    Pokemon *mon = Party_AliveMonsCount(party) > 0 ? Party_FindFirstEligibleBattler(party) : Party_GetPokemonBySlotIndex(party, 0);

    u16 species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    int form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    int gender = Pokemon_GetValue(mon, MON_DATA_GENDER, NULL);
    BOOL shiny = Pokemon_IsShiny(mon);

    if (!AllowedOnMap(species, mapID)) {
        return NULL;
    }

    MapObject *mapObj = AddFollowerObject(fieldSystem->mapObjMan, species, form, gender, dir, x, z, shiny);
    sFollower.object = mapObj;
    sFollower.enabled = TRUE;
    if (mapObj == NULL) {
        return NULL;
    }
    SetFollowerData(mapObj, 0x80, shiny);

    switch (PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar)) {
    case PLAYER_AVATAR_CYCLING:
    case PLAYER_AVATAR_SURFING:
        SetFollowerData(mapObj, FOLLOWER_DATA_BALL, TRUE);
        sRideHidden = TRUE;
        MapObject_SwitchMovementType(mapObj, FOLLOWER_MOVEMENT_STAND);
        break;
    }
    return mapObj;
}

// hk_23C88E0: after the map's objects are there: the follower comes out, or goes where it may not follow
void FollowingHack_MapObjectsReady(FieldSystem *fieldSystem)
{
    if (!sActive || !sFollower.request) {
        return;
    }
    if (!sFollower.enabled) {
        SpawnFollower(fieldSystem, PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar),
            PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar), fieldSystem->location->mapHeaderID);
    } else if (!AllowedOnMap(LeadSpecies(fieldSystem), fieldSystem->location->mapHeaderID)) {
        if (GetFollower() != NULL) {
            MapObject_Delete(GetFollower());
        }
        ResetFollower();
    }
    sFollower.request = FALSE;
}

// hk_23C3BC: every frame the field's effects are drawn: the follower is to be checked at the next map
void FollowingHack_FrameDrawn(void)
{
    if (sActive) {
        sFollower.request = TRUE;
    }
}

// hk_23CB21C: a map is loaded (continue, warp): the player and the follower are there
void FollowingHack_MapLoaded(FieldSystem *fieldSystem)
{
    if (sActive) {
        SpawnFollower(fieldSystem, fieldSystem->location->x, fieldSystem->location->z, fieldSystem->location->faceDirection,
            fieldSystem->location->mapHeaderID);
    }
}

// The player walked into a connected map (no load): the follower goes on with the player (Platinum would delete it with
// the last map's objects, sub_0206184C), or goes where it may not follow and comes back, out of its ball at the
// player's next step, where it may. The hack does this from its hooks in the map change (not ported as such)
void FollowingHack_MapConnected(FieldSystem *fieldSystem)
{
    if (!sActive) {
        return;
    }
    const enum MapHeaderID mapID = fieldSystem->location->mapHeaderID;
    MapObject *follower = GetFollower();
    const BOOL allowed = AllowedOnMap(LeadSpecies(fieldSystem), mapID);

    if (follower != NULL) {
        if (allowed) {
            MapObject_SetMapHeaderID(follower, mapID);
        } else {
            MapObject_Delete(follower);
            ResetFollower();
        }
    } else if (allowed && sFollower.enabled == FALSE) {
        MapObject *spawned = SpawnFollower(fieldSystem, PlayerAvatar_GetXPos(fieldSystem->playerAvatar),
            PlayerAvatar_GetZPos(fieldSystem->playerAvatar), PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar), mapID);
        if (spawned != NULL) {
            SetFollowerData(spawned, FOLLOWER_DATA_BALL, TRUE);
        }
    }
}

// hk_23CBF0C: a script's object: the follower for 0xFD
MapObject *FollowingHack_ScriptObject(FieldSystem *fieldSystem, int localID)
{
    return sActive && localID == FOLLOWER_LOCAL_ID ? GetFollower() : NULL;
}

BOOL FollowingHack_IsFollower(const MapObject *mapObj)
{
    return sActive && mapObj != NULL && mapObj == sFollower.object && FollowerValid();
}

// --- Script commands --------------------------------------------------------------------------------
//
// Commands 840 and 844 to 860 (the blob's table, 0x023D2194; 841 to 843 are empty).

static BOOL ScrCmd_FollowerNoop(ScriptContext *ctx)
{
    return FALSE;
}

// 840: a byte
static BOOL ScrCmd_FollowerByte(ScriptContext *ctx)
{
    sScriptByte = ScriptContext_ReadHalfWord(ctx);
    return TRUE;
}

static BOOL FollowerBallDone(ScriptContext *ctx)
{
    if (sFollower.ballAnim != NULL && OverworldAnimManager_IsActive(sFollower.ballAnim) == TRUE) {
        return FALSE;
    }
    sFollower.ballAnim = NULL;
    return TRUE;
}

// 844: the follower goes back into its ball (the recall; simplified), and the script waits for it
static BOOL ScrCmd_FollowerOut(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    MapObject *follower = GetFollower();

    sFollower.ballAnim = NULL;
    if (FollowerActive(fieldSystem)) {
        sub_020656AC(follower);
        if (FollowerOut(follower)) {
            sFollower.ballAnim = StartBallEffect(follower, 1, LeadSpecies(fieldSystem));
        }
        MapObject_SwitchMovementType(follower, FOLLOWER_MOVEMENT_STAND);
        SetFollowerData(follower, 2, TRUE);
        sub_02062DB4(follower, 1);
    }
    ScriptContext_Pause(ctx, FollowerBallDone);
    return TRUE;
}

// 845: the follower turns to the player
static BOOL ScrCmd_FollowerFacePlayer(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    MapObject **target = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_TARGET_OBJECT);
    if (*target == NULL) {
        return FALSE;
    }
    FollowerFace(*target, 6, Direction_GetOpposite(PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar)));
    return FALSE;
}

// 846: movement paused (1) or not (0)
static BOOL ScrCmd_FollowerPause(ScriptContext *ctx)
{
    u16 pause = ScriptContext_ReadHalfWord(ctx);
    if (FollowerActive(ctx->fieldSystem)) {
        if (pause == 0) {
            MapObject_SetPauseMovementOff(GetFollower());
        } else {
            MapObject_SetPauseMovementOn(GetFollower());
        }
    }
    return FALSE;
}

static BOOL FollowerAnimationDone(ScriptContext *ctx)
{
    MapObject *follower = GetFollower();
    if (follower == NULL || MapObject_CheckStatusFlag(follower, 1 << 30)) {
        return FALSE;
    }
    return LocalMapObj_CheckAnimationFinished(follower);
}

// 847: waits for the follower's animation
static BOOL ScrCmd_FollowerWait(ScriptContext *ctx)
{
    if (FollowerActive(ctx->fieldSystem)) {
        ScriptContext_Pause(ctx, FollowerAnimationDone);
        return TRUE;
    }
    return FALSE;
}

// 848: its movement type
static BOOL ScrCmd_FollowerMovementType(ScriptContext *ctx)
{
    u16 movementType = ScriptContext_ReadHalfWord(ctx);
    if (FollowerActive(ctx->fieldSystem)) {
        MapObject_SwitchMovementType(GetFollower(), movementType);
    }
    return FALSE;
}

// 849: next to the player: a direction and the direction it faces
static BOOL ScrCmd_FollowerBesidePlayer(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    u8 side = ScriptContext_ReadByte(ctx);
    u8 facing = ScriptContext_ReadByte(ctx);

    if (FollowerActive(fieldSystem)) {
        VecFx32 pos;
        MapObject_GetPosPtr(PlayerAvatar_GetMapObject(fieldSystem->playerAvatar), &pos);
        switch (side) {
        case 0:
            pos.z -= 16 * FX32_ONE;
            break;
        case 1:
            pos.z += 16 * FX32_ONE;
            break;
        case 2:
            pos.x -= 16 * FX32_ONE;
            break;
        case 3:
            pos.x += 16 * FX32_ONE;
            break;
        }
        MapObject_SetPosDirFromVec(GetFollower(), &pos, facing);
    }
    return FALSE;
}

// 850: it follows again, from the player's tile, hidden until the player moves
static BOOL ScrCmd_FollowerFollow(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    MapObject *follower = GetFollower();
    if (FollowerActive(fieldSystem)) {
        SetFollowerData(follower, 0x40, TRUE);
        SetFollowerData(follower, 2, TRUE);
        FollowerToPlayer(fieldSystem, 1);
        MapObject_SwitchMovementType(follower, FOLLOWER_MOVEMENT_WALK);
    }
    return FALSE;
}

// 851: the same without the flags
static BOOL ScrCmd_FollowerFollowAt(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    if (FollowerActive(fieldSystem)) {
        FollowerToPlayer(fieldSystem, 1);
        MapObject_SwitchMovementType(GetFollower(), FOLLOWER_MOVEMENT_WALK);
    }
    return FALSE;
}

// 852: it comes out of its ball (the ball effect; its models are not drawn yet)
static BOOL ScrCmd_FollowerRecall(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    MapObject *follower = GetFollower();
    if (FollowerActive(fieldSystem)) {
        MapObject_SwitchMovementType(follower, FOLLOWER_MOVEMENT_WALK);
        StartBallEffect(follower, 0, LeadSpecies(fieldSystem));
        FollowerSetShown(follower, FALSE);
        MapObject_SetHidden(follower, TRUE);
    }
    return FALSE;
}

// 853, 854: hide, show
static BOOL ScrCmd_FollowerHide(ScriptContext *ctx)
{
    if (FollowerActive(ctx->fieldSystem)) {
        FollowerSetShown(GetFollower(), TRUE);
    }
    return FALSE;
}

static BOOL ScrCmd_FollowerShow(ScriptContext *ctx)
{
    if (FollowerActive(ctx->fieldSystem)) {
        FollowerSetShown(GetFollower(), FALSE);
    }
    return FALSE;
}

// 855: a map object comes out of a ball: its local ID, species and type (three variables)
static BOOL ScrCmd_ObjectBall(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    int localID = FieldSystem_TryGetVar(fieldSystem, ScriptContext_ReadHalfWord(ctx));
    int species = FieldSystem_TryGetVar(fieldSystem, ScriptContext_ReadHalfWord(ctx));
    int type = FieldSystem_TryGetVar(fieldSystem, ScriptContext_ReadHalfWord(ctx));

    MapObject *mapObj = MapObjMan_LocalMapObjByIndex(fieldSystem->mapObjMan, localID);
    sub_020656AC(mapObj);
    StartBallEffect(mapObj, type, species);
    return FALSE;
}

// 856: the index of the first Pokemon that can battle in a variable (255 when there is none)
static BOOL ScrCmd_FirstBattler(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    u16 *var = FieldSystem_GetVarPointer(fieldSystem, ScriptContext_ReadHalfWord(ctx));
    Party *party = PartyOf(fieldSystem);
    int count = Party_GetCurrentCount(party);

    for (int i = 0; i < count; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);
        if (!Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL) && Pokemon_GetValue(mon, MON_DATA_HP, NULL) != 0) {
            *var = i;
            return FALSE;
        }
    }
    *var = 0xFF;
    return FALSE;
}

// 857: the follower's animation (a hop)
static BOOL ScrCmd_FollowerAnimation(ScriptContext *ctx)
{
    if (FollowerActive(ctx->fieldSystem)) {
        static MapObjectAnimCmd cmds[12];
        for (int i = 0; i < (int)NELEMS(cmds); i++) {
            u32 entry = BLOB_BALL_ANIM_CMDS + i * 4;
            cmds[i].movementAction = Read16(entry);
            cmds[i].count = Read16(entry + 2);
            if (cmds[i].movementAction == 0xFE) {
                break;
            }
        }
        MapObject_StartAnimation(GetFollower(), cmds);
    }
    return FALSE;
}

// 858: the graphics ID of a party member (a variable) in a variable (0 for an egg or none)
static BOOL ScrCmd_PartyGraphics(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    int slot = FieldSystem_TryGetVar(fieldSystem, ScriptContext_ReadHalfWord(ctx));
    u16 *var = FieldSystem_GetVarPointer(fieldSystem, ScriptContext_ReadHalfWord(ctx));
    Party *party = PartyOf(fieldSystem);

    if (Party_GetCurrentCount(party) > slot) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, slot);
        if (!Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL)) {
            *var = PokemonGraphicsID(Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL), Pokemon_GetValue(mon, MON_DATA_FORM, NULL),
                Pokemon_GetValue(mon, MON_DATA_GENDER, NULL));
            return FALSE;
        }
    }
    *var = 0;
    return FALSE;
}

// 859: whether a Pokemon follows
static BOOL ScrCmd_FollowerActive(ScriptContext *ctx)
{
    u16 *var = FieldSystem_GetVarPointer(ctx->fieldSystem, ScriptContext_ReadHalfWord(ctx));
    *var = FollowerActive(ctx->fieldSystem);
    return FALSE;
}

// 860: where the follower is next to the player: 0 north, 1 south, 2 west, 3 east, 4 elsewhere
static BOOL ScrCmd_FollowerSide(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    u16 *var = FieldSystem_GetVarPointer(fieldSystem, ScriptContext_ReadHalfWord(ctx));
    MapObject *follower = GetFollower();
    MapObject *player = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);
    int fx = MapObject_GetX(follower);
    int fz = MapObject_GetZ(follower);
    int px = MapObject_GetX(player);
    int pz = MapObject_GetZ(player);

    if (fx == px && fz == pz - 1) {
        *var = 0;
    } else if (fx == px && fz == pz + 1) {
        *var = 1;
    } else if (fx == px - 1 && fz == pz) {
        *var = 2;
    } else if (fx == px + 1 && fz == pz) {
        *var = 3;
    } else {
        *var = 4;
    }
    return FALSE;
}

static void ExtendScriptCommands(void)
{
    static const struct {
        int id;
        ScrCmdFunc func;
    } commands[] = {
        { 840, ScrCmd_FollowerByte },
        { 844, ScrCmd_FollowerOut },
        { 845, ScrCmd_FollowerFacePlayer },
        { 846, ScrCmd_FollowerPause },
        { 847, ScrCmd_FollowerWait },
        { 848, ScrCmd_FollowerMovementType },
        { 849, ScrCmd_FollowerBesidePlayer },
        { 850, ScrCmd_FollowerFollow },
        { 851, ScrCmd_FollowerFollowAt },
        { 852, ScrCmd_FollowerRecall },
        { 853, ScrCmd_FollowerHide },
        { 854, ScrCmd_FollowerShow },
        { 855, ScrCmd_ObjectBall },
        { 856, ScrCmd_FirstBattler },
        { 857, ScrCmd_FollowerAnimation },
        { 858, ScrCmd_PartyGraphics },
        { 859, ScrCmd_FollowerActive },
        { 860, ScrCmd_FollowerSide },
    };
    const int total = 861;
    ScrCmdFunc *table = calloc(total, sizeof(*table));
    memcpy(table, gFieldScriptCommands, gNumFieldScriptCommands * sizeof(*table));
    for (int i = gNumFieldScriptCommands; i < total; i++) {
        table[i] = ScrCmd_FollowerNoop;
    }
    for (int i = 0; i < (int)NELEMS(commands); i++) {
        table[commands[i].id] = commands[i].func;
    }
    gFieldScriptCommands = table;
    gNumFieldScriptCommands = total;
}

void FollowingHack_TestBall(FieldSystem *fieldSystem, int type)
{
    MapObject *follower = GetFollower();
    if (sActive && fieldSystem != NULL && follower != NULL) {
        StartBallEffect(follower, type, LeadSpecies(fieldSystem));
    }
}

void FollowingHack_InitRenderer(FieldEffectManager *fieldEffMan)
{
    if (sActive) {
        FieldEffectManager_InitRenderer(fieldEffMan, BALL_RENDERER_ID);
    }
}

// The field effect renderer of the balls
const FieldEffectRendererFuncs *FollowingHack_RendererFuncs(u32 id)
{
    return sActive && id == BALL_RENDERER_ID ? &sBallRendererFuncs : NULL;
}

// --- Startup -----------------------------------------------------------------------------------

void FollowingHack_Init(void)
{
    u32 size = 0;
    sBlob = ReadNarcMember("data/weather_sys.narc", BLOB_MEMBER, &size);
    if (sBlob == NULL) {
        return;
    }
    // The blob is 96 KB of the hack's code: its first function, and the table of renderers, are checked
    if (size != BLOB_SIZE || !InBlob(BLOB_RENDERERS, 16) || Read32(BLOB_RENDERERS) != 0 || Read32(BLOB_RENDERERS + 4) != 0x021FB0F0) {
        printf("Following hack: weather_sys.narc's member %d is not the one known (%u bytes), not used\n", BLOB_MEMBER, size);
        free(sBlob);
        sBlob = NULL;
        return;
    }
    sActive = TRUE;
    ExtendGraphicsTables();
    ExtendScriptCommands();
    printf("Following hack: found\n");
}

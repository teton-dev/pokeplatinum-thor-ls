// Enhanced graphics, Platinum's adapter to port-common's game-neutral part (port/enhanced_scene.h); see
// include/port/enhanced_graphics.h
#include "port/enhanced_graphics.h"

#include <nitro.h>
#include <stdio.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#include <port/enhanced_scene.h>

#include "constants/field/map.h"
#include "generated/map_headers.h"

#include "field/field_system.h"
#include "map_tile_behavior.h"
#include "player_avatar.h"
#include "terrain_collision_manager.h"
#include "location.h"
#include "map_header.h"
#include "map_matrix.h"
#include "overlay005/area_data.h"
#include "overlay005/land_data.h"
#include "rtc.h"

// Fog: the maps that have it (by map header: Eterna Forest is a "cave" to the game, so not by map type)
typedef struct {
    u16 mapHeaderID;
    EnhancedFogLook fog;
} FogPreset;

static const FogPreset sFogPresets[] = {
    // Eterna Forest: mist lying among the trees, to about a person's knees, drifting slowly east
    { MAP_HEADER_ETERNA_FOREST, { 0.72f, 0.80f, 0.76f, 0.20f, 0.25f, 0.34f, 0.026f, 9.0f, 0.0008f, 170.0f, 40.0f, 0.85f, 5.0f, 1.5f } },
};

// Lamp light: the textures that are lamps' in the maps' texture sets (the land models' street lamps carry them, and so do
// the props'; found from the land models' texture names and checked on the screen, docs/thor/dev/enhanced-graphics.md),
// the light's colour and reach (the light at the middle of each lamp's glowing part)
static const EnhancedLampLook sLampTextures[] = {
    // (c2_light_b is the same Canalave lamps' frame: listing it too lit them twice)
    { "c1_o02b", 80.0f, 1.30f, 0.98f, 0.55f },    // Jubilife: the street lamps' heads
    { "c2_light_a", 72.0f, 1.10f, 0.85f, 0.50f }, // Canalave: the lamps on the posts along the canal
    { "c5_light", 80.0f, 1.30f, 0.98f, 0.55f },   // Hearthome: the street lamps
    // Veilstone's Game Corner front (its neon is found as about three groups, so each is dimmer)
    { "neon3", 72.0f, 0.55f, 0.25f, 0.45f },
};

static const EnhancedFogLook *Fog(int mapHeaderID)
{
    for (int i = 0; i < (int)(sizeof(sFogPresets) / sizeof(sFogPresets[0])); i++) {
        if (sFogPresets[i].mapHeaderID == mapHeaderID) {
            return &sFogPresets[i].fog;
        }
    }
    return NULL;
}

void EnhancedGraphics_FieldDrawing(FieldSystem *fieldSystem)
{
    if (fieldSystem == NULL || fieldSystem->playerAvatar == NULL || fieldSystem->areaDataManager == NULL) {
        EnhancedScene_FieldDrawing(0.0f, NULL, 0, NULL, 0);
        return;
    }
    const NNSG3dResTex *sets[2] = { AreaDataManager_GetMapTexture(fieldSystem->areaDataManager),
        AreaDataManager_GetMapPropTexture(fieldSystem->areaDataManager) };
    EnhancedScene_FieldDrawing(PlayerAvatar_GetPos(fieldSystem->playerAvatar)->y / 4096.0f, sets, 2, sLampTextures,
        (int)(sizeof(sLampTextures) / sizeof(sLampTextures[0])));
}

// A tile of the loaded map matrix: the game's own lookup of one outside it sets a link error
// (LandDataManager_GetRelativeLoadedMapsQuadrantOfTile), which, while the field's link session runs, ended in the error
// reset: a white screen on loading into a small map with water on (and every headless test with water)
static BOOL InLoadedMap(const FieldSystem *fieldSystem, int x, int z)
{
    if (fieldSystem->landDataMan == NULL || fieldSystem->mapMatrix == NULL) {
        return FALSE;
    }
    const int fx = x - LandDataManager_GetOffsetTileX(fieldSystem->landDataMan);
    const int fz = z - LandDataManager_GetOffsetTileZ(fieldSystem->landDataMan);
    return fx >= 0 && fz >= 0 && fx < MapMatrix_GetWidth(fieldSystem->mapMatrix) * MAP_TILES_COUNT_X
        && fz < MapMatrix_GetHeight(fieldSystem->mapMatrix) * MAP_TILES_COUNT_Z;
}

typedef struct {
    FieldSystem *fieldSystem;
    fx32 playerHeight;
} WaterContext;

static BOOL IsWaterTile(void *context, int x, int z)
{
    FieldSystem *fieldSystem = ((WaterContext *)context)->fieldSystem;
    if (!InLoadedMap(fieldSystem, x, z)) {
        return FALSE;
    }
    const u8 behavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);
    return behavior != 0xFF && (TileBehavior_HasReflectiveSurface(behavior) || TileBehavior_IsSurfable(behavior));
}

static fx32 WaterHeight(void *context, int x, int z)
{
    WaterContext *water = context;
    u8 source;
    return TerrainCollisionManager_GetHeight(water->fieldSystem, water->playerHeight,
        x * MAP_OBJECT_TILE_SIZE + MAP_OBJECT_TILE_SIZE / 2, z * MAP_OBJECT_TILE_SIZE + MAP_OBJECT_TILE_SIZE / 2, &source);
}

void EnhancedGraphics_DrawWaterMask(FieldSystem *fieldSystem)
{
    if (fieldSystem == NULL || fieldSystem->playerAvatar == NULL || fieldSystem->location == NULL) {
        EnhancedScene_DrawWaterMask(NULL);
        return;
    }
    WaterContext context = { fieldSystem, PlayerAvatar_GetPos(fieldSystem->playerAvatar)->y };
    const EnhancedWaterQuery query = {
        .context = &context,
        .isWater = IsWaterTile,
        .height = WaterHeight,
        .playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar),
        .playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar),
        .mapId = fieldSystem->location->mapHeaderID,
    };
    EnhancedScene_DrawWaterMask(&query);
}

void EnhancedGraphics_Update(FieldSystem *fieldSystem)
{
    EnhancedSceneState state = { 0 };
    if (fieldSystem != NULL && fieldSystem->location != NULL) {
        state.field = TRUE;
        state.outdoors = MapHeader_IsOutdoors(fieldSystem->location->mapHeaderID);
        // Tests: POKEPLATINUM_ENHANCED_HOUR=H[.5] fixes the clock the light follows (the game's own is the real one)
        state.hour = EnhancedScene_Hour(GetSecondsSinceMidnight() / 3600.0f, "POKEPLATINUM_ENHANCED_HOUR");
        state.fog = Fog(fieldSystem->location->mapHeaderID);
#ifdef __ANDROID__
        // `adb shell setprop debug.eterna.poslog 1`: the map and tile at each step, to app-log.txt (to find a spot seen on
        // a device in a desktop test)
        static char sPosLog[PROP_VALUE_MAX];
        static int sPosLogAge = 0, sLastX = -1, sLastZ = -1;
        if (sPosLogAge-- <= 0) {
            sPosLogAge = 60;
            if (__system_property_get("debug.eterna.poslog", sPosLog) <= 0) {
                sPosLog[0] = '\0';
            }
        }
        if (sPosLog[0] == '1' && fieldSystem->playerAvatar != NULL) {
            const int x = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
            const int z = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
            if (x != sLastX || z != sLastZ) {
                sLastX = x;
                sLastZ = z;
                printf("position: map %d x %d z %d\n", fieldSystem->location->mapHeaderID, x, z);
                fflush(stdout);
            }
        }
#endif
    }
    EnhancedScene_Update(&state);
}

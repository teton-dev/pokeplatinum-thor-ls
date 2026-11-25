// Enhanced graphics: the time of day's light over the outdoor field map; see include/port/enhanced_graphics.h
#include "port/enhanced_graphics.h"

#include <nitro.h>
#include <math.h>
#include <stdlib.h>
#include <simulator/config/sim_config.h>
#include <simulator/sim.h>

#include "constants/field/map.h"

#include "field/field_system.h"
#include "map_tile_behavior.h"
#include "player_avatar.h"
#include "terrain_collision_manager.h"
#include "location.h"
#include "map_header.h"
#include "rtc.h"

typedef struct {
    int hour;
    u8 r, g, b;
    u8 vignette;
} LightKey;

// The light at each hour, between which it is blended by the minute. Gentle: the game's own area light already
// darkens the night and warms the evening, so this is a grade on top of it, not a replacement.
static const LightKey sKeys[] = {
    { 0, 175, 190, 235, 70 },
    { 5, 185, 195, 235, 60 },
    { 7, 255, 228, 205, 30 },
    { 9, 255, 255, 255, 0 },
    { 16, 255, 255, 255, 0 },
    { 18, 255, 214, 176, 35 },
    { 20, 190, 180, 215, 55 },
    { 22, 175, 190, 235, 70 },
    { 24, 175, 190, 235, 70 },
};

// The air by the hour: the haze's colour and its share at the top of the screen, and how brightly the glowing parts
// (lamps, lit windows) shine: the luminance they must pass and how strongly they glow
typedef struct {
    int hour;
    float hazeR, hazeG, hazeB, haze;
    float bloomThreshold, bloomStrength;
} AirKey;

static const AirKey sAir[] = {
    { 0, 0.30f, 0.38f, 0.60f, 0.28f, 0.70f, 0.80f },
    { 5, 0.30f, 0.38f, 0.60f, 0.28f, 0.70f, 0.80f },
    { 7, 0.95f, 0.70f, 0.55f, 0.30f, 0.90f, 0.35f },
    { 9, 0.75f, 0.85f, 0.97f, 0.20f, 0.97f, 0.12f },
    { 16, 0.78f, 0.86f, 0.97f, 0.20f, 0.97f, 0.12f },
    { 18, 1.00f, 0.66f, 0.45f, 0.32f, 0.92f, 0.35f },
    { 20, 0.35f, 0.38f, 0.62f, 0.28f, 0.72f, 0.75f },
    { 24, 0.30f, 0.38f, 0.60f, 0.28f, 0.70f, 0.80f },
};

static void SetAir(float hour, float shaft)
{
    AirKey now = sAir[0];
    for (int i = 0; i < (int)(sizeof(sAir) / sizeof(sAir[0])) - 1; i++) {
        if (hour >= sAir[i].hour && hour < sAir[i + 1].hour) {
            const float t = (hour - sAir[i].hour) / (float)(sAir[i + 1].hour - sAir[i].hour);
            now.hazeR = sAir[i].hazeR + (sAir[i + 1].hazeR - sAir[i].hazeR) * t;
            now.hazeG = sAir[i].hazeG + (sAir[i + 1].hazeG - sAir[i].hazeG) * t;
            now.hazeB = sAir[i].hazeB + (sAir[i + 1].hazeB - sAir[i].hazeB) * t;
            now.haze = sAir[i].haze + (sAir[i + 1].haze - sAir[i].haze) * t;
            now.bloomThreshold = sAir[i].bloomThreshold + (sAir[i + 1].bloomThreshold - sAir[i].bloomThreshold) * t;
            now.bloomStrength = sAir[i].bloomStrength + (sAir[i + 1].bloomStrength - sAir[i].bloomStrength) * t;
            break;
        }
    }
    SIM_SetEnhancedAtmosphere(shaft, now.hazeR, now.hazeG, now.hazeB, now.haze);
    SIM_SetEnhancedBloom(now.bloomThreshold, now.bloomStrength);
}

static u8 Lerp(u8 a, u8 b, int t, int span)
{
    return (u8)(a + ((int)b - (int)a) * t / span);
}

// The sun's shadows by the hour: they fall to the left in the morning (the sun in the east) and the right in the
// evening, short at noon and long when it is low, with none at night
static BOOL sSunShadows = FALSE;
static BOOL sSunLit = FALSE; // the sun is up (and a setting that reads the game's camera is on)
// Toward the sun, in the world (x east, y up, z south), unit length
static float sSunWorld[3] = { 0.0f, 1.0f, 0.0f };
// The sprites' shadows' lean (SIM_SetEnhancedSun): sideways in NDC x per NDC y of height, and up the screen per height
static float sSpriteShear[2] = { 0.0f, 0.24f };

void EnhancedGraphics_SetCamera(const MtxFx44 *projection, const MtxFx43 *camera)
{
    if (!sSunLit || projection == NULL || camera == NULL) {
        return;
    }
    const float k = 1.0f / 4096.0f;
    float x = sSunWorld[0], y = sSunWorld[1], z = sSunWorld[2];
    // The 3D's sun casts as the sprites' shadows lean: a thing as tall as a sprite throws a shadow as long, the same way.
    // A sprite's shadow moves (4/3) shx (NDC x is 4/3 of NDC y on a 4:3 screen) across and shy up the screen per unit of
    // its height, in view units; on the ground that is shx' east per unit of height (the camera has no yaw: view x is
    // world x) and, northward, whatever reaches shy up the screen through the camera's pitch (view y of world z, _21)
    if (camera->_00 > 0 && camera->_21 < -0.05f * 4096.0f) {
        const float east = (4.0f / 3.0f) * sSpriteShear[0] / (camera->_00 * k);
        const float north = sSpriteShear[1] / (-camera->_21 * k);
        const float n = sqrtf(east * east + 1.0f + north * north);
        x = -east / n;
        y = 1.0f / n;
        z = north / n;
    }
    // The matrices are the DS's, for row vectors: a world direction is multiplied on the left
    float vx = (x * camera->_00 + y * camera->_10 + z * camera->_20) * k;
    float vy = (x * camera->_01 + y * camera->_11 + z * camera->_21) * k;
    float vz = (x * camera->_02 + y * camera->_12 + z * camera->_22) * k;
    const float length = sqrtf(vx * vx + vy * vy + vz * vz);
    if (length < 0.0001f) {
        return;
    }
    // The world's up in view space, for libntr to tell the ground from walls and roofs
    float ux = camera->_10 * k, uy = camera->_11 * k, uz = camera->_12 * k;
    const float upLength = sqrtf(ux * ux + uy * uy + uz * uz);
    if (upLength < 0.0001f) {
        return;
    }
    SIM_SetEnhancedShadowCamera(projection->_00 * k, projection->_11 * k, projection->_22 * k, projection->_32 * k,
        vx / length, vy / length, vz / length, ux / upLength, uy / upLength, uz / upLength);
}

void EnhancedGraphics_LandDrawing(BOOL drawing)
{
    // The commands already sent are drawn as they were
    NNS_G3dGeFlushBuffer();
    SIM_SetHideBuiltInShadows(drawing && sSunShadows);
}

void EnhancedGraphics_CasterDrawing(BOOL drawing)
{
    NNS_G3dGeFlushBuffer();
    SIM_SetSunCasters(drawing && sSunShadows);
}

BOOL EnhancedGraphics_SunShadowsShown(void)
{
    return sSunShadows;
}

static void SetSun(float hour)
{
    static const struct {
        float hour;
        u8 strength;
    } kStrength[] = { { 6.0f, 0 }, { 7.5f, 70 }, { 10.0f, 105 }, { 14.0f, 105 }, { 17.0f, 90 }, { 19.0f, 40 }, { 20.0f, 0 } };
    float strength = 0.0f;
    for (int i = 0; i < (int)(sizeof(kStrength) / sizeof(kStrength[0])) - 1; i++) {
        if (hour >= kStrength[i].hour && hour < kStrength[i + 1].hour) {
            const float t = (hour - kStrength[i].hour) / (kStrength[i + 1].hour - kStrength[i].hour);
            strength = kStrength[i].strength + (kStrength[i + 1].strength - kStrength[i].strength) * t;
        }
    }
    const float fromNoon = hour - 12.0f;
    const float away = fromNoon < 0.0f ? -fromNoon : fromNoon;
    sSpriteShear[0] = fromNoon * 0.12f;
    sSpriteShear[1] = 0.24f + 0.03f * away;
    SIM_SetEnhancedSun(sSpriteShear[0], sSpriteShear[1], (u8)strength);
    sSunShadows = strength >= 30.0f && SIM_GetConfigPtr()->enhancedShadows;
    const BOOL wantsLight = SIM_GetConfigPtr()->enhancedHighlights || SIM_GetConfigPtr()->enhancedAtmosphere;
    sSunLit = strength >= 30.0f && (SIM_GetConfigPtr()->enhancedShadows || wantsLight);
    // The sun's light for the highlights: white-warm at noon, orange as it gets low, as strong as the shadows' strength
    const float low = away / 6.0f > 1.0f ? 1.0f : away / 6.0f;
    // The air scatters more of a low sun's light: stronger shafts in the morning and evening
    SetAir(hour, strength >= 30.0f ? 0.05f + 0.10f * low : 0.0f);
    if (strength >= 30.0f && wantsLight) {
        const float level = strength / 105.0f > 1.0f ? 1.0f : strength / 105.0f;
        SIM_SetEnhancedSunLight(level * (1.0f - 0.0f * low), level * (0.95f - 0.23f * low), level * (0.85f - 0.4f * low));
    } else {
        SIM_SetEnhancedSunLight(0.0f, 0.0f, 0.0f);
    }
    // Toward the sun: up, and opposite the shadows' way (the sprites' shadows lean to the east in the evening and away
    // from the camera, north, at noon: the sun is in the west, south of overhead)
    {
        const float ex = -fromNoon * 0.35f, ez = 0.45f + 0.05f * away;
        const float n = sqrtf(ex * ex + 1.0f + ez * ez);
        sSunWorld[0] = ex / n;
        sSunWorld[1] = 1.0f / n;
        sSunWorld[2] = ez / n;
    }
}

// The water mask: the water tiles around the player as runs along x, found again when the player's tile changes (or
// every half second), and drawn every frame as a few quads that libntr keeps as the mask (SIM_SetPolygonTag)
#define WATER_RANGE_X 30
#define WATER_RANGE_Z 22
#define WATER_MAX_RUNS 400

typedef struct {
    s16 x;
    s16 z;
    s16 length;
    fx32 height;
} WaterRun;

static WaterRun sWaterRuns[WATER_MAX_RUNS];
static int sWaterRunCount = 0;
static int sWaterCacheX = -1, sWaterCacheZ = -1, sWaterCacheMap = -1, sWaterCacheAge = 0;

static BOOL IsWaterTile(u8 behavior)
{
    return behavior != 0xFF && (TileBehavior_HasReflectiveSurface(behavior) || TileBehavior_IsSurfable(behavior));
}

static void FindWater(FieldSystem *fieldSystem, int px, int pz, fx32 playerHeight)
{
    sWaterRunCount = 0;
    for (int z = pz - WATER_RANGE_Z; z <= pz + WATER_RANGE_Z; z++) {
        int start = -1;
        for (int x = px - WATER_RANGE_X; x <= px + WATER_RANGE_X + 1; x++) {
            const BOOL water = x <= px + WATER_RANGE_X && x >= 0 && z >= 0 && IsWaterTile(TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z));
            if (water && start < 0) {
                start = x;
            } else if (!water && start >= 0) {
                if (sWaterRunCount < WATER_MAX_RUNS) {
                    u8 source;
                    WaterRun *run = &sWaterRuns[sWaterRunCount++];
                    run->x = start;
                    run->z = z;
                    run->length = x - start;
                    run->height = TerrainCollisionManager_GetHeight(fieldSystem, playerHeight,
                        start * MAP_OBJECT_TILE_SIZE + MAP_OBJECT_TILE_SIZE / 2, z * MAP_OBJECT_TILE_SIZE + MAP_OBJECT_TILE_SIZE / 2, &source);
                }
                start = -1;
            }
        }
    }
}

void EnhancedGraphics_DrawWaterMask(FieldSystem *fieldSystem)
{
    if (!SIM_GetConfigPtr()->enhancedWater || fieldSystem == NULL || fieldSystem->playerAvatar == NULL
        || fieldSystem->location == NULL) {
        SIM_SetEnhancedWater(0);
        return;
    }

    const int px = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    const int pz = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
    const int map = fieldSystem->location->mapHeaderID;
    if (px != sWaterCacheX || pz != sWaterCacheZ || map != sWaterCacheMap || ++sWaterCacheAge >= 30) {
        sWaterCacheX = px;
        sWaterCacheZ = pz;
        sWaterCacheMap = map;
        sWaterCacheAge = 0;
        FindWater(fieldSystem, px, pz, PlayerAvatar_GetPos(fieldSystem->playerAvatar)->y);
    }
    SIM_SetEnhancedWater(sWaterRunCount > 0 ? 190 : 0);
    if (sWaterRunCount == 0) {
        return;
    }

    // Each run is a unit quad scaled to its length: the polygons are not drawn, libntr keeps them as the mask
    MtxFx33 identity;
    MTX_Identity33(&identity);
    NNS_G3dGeFlushBuffer();
    SIM_SetPolygonTag(1);
    G3_PolygonAttr(GX_LIGHTMASK_NONE, GX_POLYGONMODE_MODULATE, GX_CULL_NONE, 0, 31, 0);
    for (int i = 0; i < sWaterRunCount; i++) {
        const WaterRun *run = &sWaterRuns[i];
        VecFx32 pos = { run->x * MAP_OBJECT_TILE_SIZE, run->height, run->z * MAP_OBJECT_TILE_SIZE };
        VecFx32 scale = { run->length * MAP_OBJECT_TILE_SIZE, FX32_ONE, MAP_OBJECT_TILE_SIZE };
        NNS_G3dGlbSetBaseTrans(&pos);
        NNS_G3dGlbSetBaseRot(&identity);
        NNS_G3dGlbSetBaseScale(&scale);
        NNS_G3dGlbFlush();
        G3_Begin(GX_BEGIN_QUADS);
        G3_Vtx(0, 0, 0);
        G3_Vtx(FX16_ONE, 0, 0);
        G3_Vtx(FX16_ONE, 0, FX16_ONE);
        G3_Vtx(0, 0, FX16_ONE);
        G3_End();
    }
    NNS_G3dGeFlushBuffer();
    SIM_SetPolygonTag(0);
    // The base transform back to nothing: later draws set their own, but not every one sets all three
    VecFx32 origin = { 0, 0, 0 };
    VecFx32 unit = { FX32_ONE, FX32_ONE, FX32_ONE };
    NNS_G3dGlbSetBaseTrans(&origin);
    NNS_G3dGlbSetBaseScale(&unit);
}

void EnhancedGraphics_Update(FieldSystem *fieldSystem)
{
    if (fieldSystem == NULL || fieldSystem->location == NULL
        || !MapHeader_IsOutdoors(fieldSystem->location->mapHeaderID)) {
        SIM_SetEnhancedScene(255, 255, 255, 0);
        SIM_SetEnhancedSun(0.0f, 0.0f, 0);
        SIM_SetEnhancedSunLight(0.0f, 0.0f, 0.0f);
        SIM_SetEnhancedAtmosphere(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        SIM_SetEnhancedBloom(1.0f, 0.0f);
        sSunShadows = FALSE;
        sSunLit = FALSE;
        return;
    }

    int minutes = GetSecondsSinceMidnight() / 60;
    // Tests: POKEPLATINUM_ENHANCED_HOUR=H[.5] fixes the clock the light follows (the game's own is the real one)
    const char *testHour = getenv("POKEPLATINUM_ENHANCED_HOUR");
    if (testHour != NULL && testHour[0] != '\0') {
        minutes = (int)(atof(testHour) * 60.0);
    }
    SetSun(minutes / 60.0f);
    for (int i = 0; i < (int)(sizeof(sKeys) / sizeof(sKeys[0])) - 1; i++) {
        const int from = sKeys[i].hour * 60;
        const int to = sKeys[i + 1].hour * 60;
        if (minutes >= from && minutes < to) {
            const int t = minutes - from;
            const int span = to - from;
            SIM_SetEnhancedScene(Lerp(sKeys[i].r, sKeys[i + 1].r, t, span),
                Lerp(sKeys[i].g, sKeys[i + 1].g, t, span),
                Lerp(sKeys[i].b, sKeys[i + 1].b, t, span),
                Lerp(sKeys[i].vignette, sKeys[i + 1].vignette, t, span));
            return;
        }
    }
    SIM_SetEnhancedScene(255, 255, 255, 0);
}

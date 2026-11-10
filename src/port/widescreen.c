// Widescreen overworld (experimental); see include/port/widescreen.h
#include "port/widescreen.h"

#include <nitro.h>
#include <simulator/config/sim_config.h>
#include <simulator/sim.h>

#include "constants/overworld_weather.h"

#include "field/field_system.h"
#include "field/field_system_sub2_t.h"
#include "overlay005/ov5_021D5EB8.h"
#include "overlay006/hm_cut_in.h"
#include "applications/bag/application.h"
#include "applications/journal_display/journal_controller.h"
#include "applications/party_menu/main.h"
#include "applications/town_map/main.h"
#include "port/debug_field.h"
#include "port/gear_data.h"
#ifdef GEN5_BATTLES
#include "port/gen5_battle.h"
#endif

#include "camera.h"
#include "start_menu.h"

// The field camera's projection as last computed, and its widening
static const Camera *sFieldCamera = NULL;
static float sFieldRatio = 1.0f;

// The field map is running with no sub-screen open (its camera exists then)
static FieldSystem *ShownFieldSystem(void)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || fieldSystem->processManager == NULL || fieldSystem->processManager->child != NULL
        || !fieldSystem->runningFieldMap || fieldSystem->camera == NULL) {
        return NULL;
    }
    return fieldSystem;
}

// How the weather's 2D layers go wide: its sprites (rain, snow, sand, ash, hail) repeated across
// the sides, the 4:3 picture wrapping around, and its BG2 picture (fog, sand, clouds, the
// forests' light) drawn across the whole width from its tilemap, which scrolls and repeats.
// FALSE: the scene stays 4:3 (Flash's darkness, a circle of windows drawn line by line)
static BOOL WeatherLayers(FieldSystem *fieldSystem, u8 *objWrap, u8 *wideBgs, u8 *mirrorBgs)
{
    *objWrap = 0;
    *wideBgs = 0;
    *mirrorBgs = 0;
    // An HM's cut-in: its band is BG3 behind a window (its rings mirrored across the sides), and the
    // wind particles and Pokemon that cross it (sprites) continue to the edges
    if (HMCutIn_PortIsPlaying()) {
        *wideBgs |= 1 << 3;
        *mirrorBgs |= 1 << 3;
        *objWrap |= 0x0F;
    }
    if (fieldSystem->unk_04 == NULL || fieldSystem->unk_04->unk_0C == NULL) {
        return TRUE;
    }
    const u32 weather = ov5_021D5FE0(fieldSystem->unk_04->unk_0C);
    if (weather == OVERWORLD_WEATHER_DARK_FLASH) {
        return FALSE;
    }
    BOOL sprites, bg2;
    ov5_PortWeatherLayers(weather, &sprites, &bg2);
    // Not while the start menu is open: its icons are sprites too, at the right of the 4:3 screen
    if (sprites && !StartMenu_PortIsOpen(fieldSystem)) {
        *objWrap |= 0x0F;
    }
    if (bg2) {
        *wideBgs |= 1 << 2;
    }
    return TRUE;
}

// A battle in Black 2 and White 2's engine is shown: its arena is 3D, and its camera is widened
// as the field camera is (Gen5_SetWidescreen, from port/gen5_battle.c)
static BOOL Gen5BattleShown(void)
{
#ifdef GEN5_BATTLES
    return Gen5Battle_IsShown();
#else
    return FALSE;
#endif
}

// A full-screen 2D application that fills the wide screen is running over the overworld: the Bag, the party screen, the Journal (also
// the page that opens on loading a save) and the Town Map (and the Fly map). They are drawn for 256 pixels; their
// layers' edge columns continue across the sides, so the backgrounds reach the edges of the window and the
// contents (text, lists, the map) stay in the 4:3 middle
static BOOL WideApplication(void)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || fieldSystem->processManager == NULL || fieldSystem->processManager->child == NULL) {
        return FALSE;
    }
    const OverlayFunc init = fieldSystem->processManager->child->template.init;
    return init == gBagApplicationTemplate.init || init == (OverlayFunc)JournalController_Init
        || init == TownMap_Init || init == gPokemonPartyAppTemplate.init;
}

void Widescreen_Update(void)
{
    // Platinum's battles stay 4:3 with black bars: the backdrop is a 256-pixel picture and the
    // bases, HP boxes and text box are placed for 256 pixels, so the sides had nothing real to
    // show (they repeated the backdrop's tiles and cut the bases off at the 4:3 edges)
    FieldSystem *fieldSystem = ShownFieldSystem();
    u8 objWrap = 0;
    u8 wideBgs = 0;
    u8 mirrorBgs = 0;
    BOOL wide = Gen5BattleShown();
    if (!wide && Gear_ActiveBattleSystem() == NULL && fieldSystem != NULL) {
        wide = WeatherLayers(fieldSystem, &objWrap, &wideBgs, &mirrorBgs);
    }
    u8 edgeBgs = Gen5BattleShown() ? (1 << 1) : 0;
    if (!wide && WideApplication()) {
        wide = TRUE;
        edgeBgs = 0x0F;
    }
    SIM_SetWidescreenSceneOptions(FALSE, wideBgs);
    SIM_SetWidescreenObjWrap(objWrap);
    SIM_SetWidescreenBgMirror(mirrorBgs);
    // BW2's text box (BG1) reaches the edges of the wide screen; its text stays in the middle
    SIM_SetWidescreenBgEdgeExtend(edgeBgs);
    SIM_SetWidescreenScene(wide);
}

static float WantedRatio(const Camera *camera)
{
    FieldSystem *fieldSystem = ShownFieldSystem();
    if (fieldSystem == NULL || camera != fieldSystem->camera || !SIM_IsWidescreenActive()) {
        return 1.0f;
    }
    return SIM_WidescreenRatio();
}

fx32 Widescreen_AspectRatio(const Camera *camera, fx32 aspectRatio)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || camera != fieldSystem->camera) {
        return aspectRatio;
    }
    const float ratio = WantedRatio(camera);
    sFieldCamera = camera;
    sFieldRatio = ratio;
    return ratio == 1.0f ? aspectRatio : (fx32)((float)aspectRatio * ratio);
}

void Widescreen_BeforeFieldRender(FieldSystem *fieldSystem)
{
    Camera *camera = fieldSystem->camera;
    // Only while the field camera is the one in use: the projection is global
    if (camera == NULL || Camera_PortGetActive() != camera) {
        return;
    }
    if (sFieldCamera != camera || sFieldRatio != WantedRatio(camera)) {
        Camera_ComputeProjectionMatrix(camera->projection, camera);
    }
}

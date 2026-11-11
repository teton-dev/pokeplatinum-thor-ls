// Widescreen overworld (experimental); see include/port/widescreen.h
#include "port/widescreen.h"

#include <nitro.h>
#include <string.h>
#include <simulator/config/sim_config.h>
#include <simulator/sim.h>

#include "constants/overworld_weather.h"

#include "field/field_system.h"
#include "field/field_system_sub2_t.h"
#include "overlay005/ov5_021D5EB8.h"
#include "overlay005/encounter_effect_core.h"
#include "overlay006/hm_cut_in.h"
#include "applications/bag/application.h"
#include "applications/journal_display/journal_controller.h"
#include "applications/party_menu/main.h"
#include "applications/town_map/main.h"
#include "port/debug_field.h"
#include "port/gear_data.h"
#include "battle/battle_display.h"
#include "battle/battle_system.h"
#include "battle/healthbox.h"
#include "sprite.h"
#include "sprite_system.h"
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
    // A Gym Leader's encounter effect: its banner is BG3 (mirrored across the sides); the bars that slide across
    // (sprites of priority 1 to 3) continue to the edges, the leader's sprite (priority 0) stays where it is
    if (EncounterEffect_PortGymLeaderPlaying()) {
        *wideBgs |= 1 << 3;
        *mirrorBgs |= 1 << 3;
        *objWrap |= 0x0E;
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
static BOOL WideApplication(u8 *edgeBgs, u8 *edgeMirrorBgs)
{
    *edgeBgs = 0x0F;
    *edgeMirrorBgs = 0;
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem == NULL || fieldSystem->processManager == NULL || fieldSystem->processManager->child == NULL) {
        return FALSE;
    }
    const OverlayFunc init = fieldSystem->processManager->child->template.init;
    if (init == gPokemonPartyAppTemplate.init) {
        // Its background (BG3, lines) is mirrored across the sides, so the lines continue
        *edgeMirrorBgs = (1 << 2) | (1 << 3);
        return TRUE;
    }
    return init == gBagApplicationTemplate.init || init == (OverlayFunc)JournalController_Init
        || init == TownMap_Init;
}

// The HP box end cap (5x33, RGB, pure white is transparent): the enemy's box gets it at its left end, the player's
// mirrored at its right end, when the sides of a wide Platinum battle show the backdrop
static const u8 sCapImage[5 * 33 * 3] = {
    0xFF, 0xFF, 0xFF, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8,
    0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0x68, 0x70, 0x58, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28,
    0xD8, 0xD8, 0xD8, 0x68, 0x70, 0x58, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38,
    0xD8, 0xD8, 0xD8, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28,
    0xD8, 0xD8, 0xD8, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38,
    0xD8, 0xD8, 0xD8, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38, 0x60, 0x48, 0x38,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0xA8, 0xA8, 0x90, 0xA8, 0xA8, 0x90,
    0xD8, 0xD8, 0xD8, 0x68, 0x70, 0x58, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28,
    0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0x68, 0x70, 0x58, 0x28, 0x30, 0x28, 0x28, 0x30, 0x28,
    0xFF, 0xFF, 0xFF, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8
};

#define CAP_WIDTH  5
#define CAP_HEIGHT 33

// The healthboxes' end caps in a wide Platinum battle. Their sprite positions are the boxes' centres: the enemy's
// box (116 wide, 33 high) reaches 58 to the left of its position, the player's (128 wide, 42 high with the EXP bar)
// 64 to the right. The boxes slide in and out, so the caps follow them
static void UpdateHealthboxCaps(BOOL wideBattle)
{
    SIM_WideStamp stamps[8];
    memset(stamps, 0, sizeof(stamps));
    int count = 0;
    BattleSystem *battleSys = Gear_ActiveBattleSystem();
    if (wideBattle && battleSys != NULL) {
        const int battlers = BattleSystem_GetMaxBattlers(battleSys);
        for (int i = 0; i < battlers && count < 8; i++) {
            BattlerData *battlerData = BattleSystem_GetBattlerData(battleSys, i);
            HealthBox *healthbox = battlerData != NULL ? BattlerData_GetHealthBox(battlerData) : NULL;
            if (healthbox == NULL || healthbox->mainSprite == NULL || healthbox->mainSprite->sprite == NULL
                || !Sprite_GetDrawFlag(healthbox->mainSprite->sprite)) {
                continue;
            }
            const VecFx32 *position = Sprite_GetPosition(healthbox->mainSprite->sprite);
            const int x = position->x >> FX32_SHIFT;
            const int y = position->y >> FX32_SHIFT;
            switch (healthbox->type) {
            case HEALTHBOX_TYPE_ENEMY_SOLO:
            case HEALTHBOX_TYPE_ENEMY_SLOT_1:
            case HEALTHBOX_TYPE_ENEMY_SLOT_2:
                // a double battle's first enemy box is 6 pixels wider (its position is 64, the second's 58)
                stamps[count].x = x - (healthbox->type == HEALTHBOX_TYPE_ENEMY_SLOT_1 ? 64 : 58) - CAP_WIDTH;
                stamps[count].y = y - 16;
                stamps[count].height = CAP_HEIGHT;
                stamps[count].flipX = 0;
                count++;
                break;
            case HEALTHBOX_TYPE_PLAYER_SOLO:
                stamps[count].x = x + 64;
                stamps[count].y = y - 21;
                stamps[count].height = 42;
                stamps[count].extraTop = 1; // the player's HP band is a row lower
                stamps[count].flipX = 1;
                count++;
                break;
            case HEALTHBOX_TYPE_PLAYER_SLOT_1:
            case HEALTHBOX_TYPE_PLAYER_SLOT_2:
                stamps[count].x = x + 64;
                stamps[count].y = y - 16;
                stamps[count].height = CAP_HEIGHT;
                stamps[count].flipX = 1;
                count++;
                break;
            default:
                break;
            }
        }
    }
    SIM_SetWidescreenStampImage(sCapImage, CAP_WIDTH, CAP_HEIGHT);
    SIM_SetWidescreenStamps(3, stamps, count);
}

void Widescreen_Update(void)
{
    FieldSystem *fieldSystem = ShownFieldSystem();
    u8 objWrap = 0;
    u8 wideBgs = 0;
    u8 mirrorBgs = 0;
    BOOL wideBattle = FALSE;
    BOOL wide = Gen5BattleShown();
    if (!wide && Gear_ActiveBattleSystem() == NULL && fieldSystem != NULL) {
        wide = WeatherLayers(fieldSystem, &objWrap, &wideBgs, &mirrorBgs);
    }
    u8 edgeBgs = Gen5BattleShown() ? (1 << 1) : 0;
    if (!wide && Gear_ActiveBattleSystem() != NULL) {
        // Platinum's battle: its backdrop (BG3) continued across the sides by its edge columns (not mirrored: the
        // bases the Pokémon stand on are part of it and would repeat); the HP boxes and text box stay as they are, in
        // the 4:3 middle
        wide = TRUE;
        wideBgs = 1 << 3;
        wideBattle = TRUE;
    }
    UpdateHealthboxCaps(wideBattle);
    u8 edgeMirrorBgs = 0;
    if (!wide && WideApplication(&edgeBgs, &edgeMirrorBgs)) {
        wide = TRUE;
    }
    // Platinum's battle draws its Pokemon as 3D quads for a 4:3 screen: narrowed so they keep their shape
    SIM_SetWidescreenSceneOptions(wideBattle, wideBgs);
    SIM_SetWidescreenObjWrap(objWrap);
    SIM_SetWidescreenBgMirror(mirrorBgs);
    // BW2's text box (BG1) reaches the edges of the wide screen; its text stays in the middle
    SIM_SetWidescreenBgEdgeExtend(edgeBgs);
    SIM_SetWidescreenBgEdgeMirror(edgeMirrorBgs);
    // and its black bars (the party balls', the ability's), sprites that touch the picture's edges
    SIM_SetWidescreenObjEdgeExtend(Gen5BattleShown() ? 0x0F : 0);
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

int Widescreen_BattleSpreadX(int battlerType)
{
    if (!SIM_IsWidescreenActive()) {
        return 0;
    }

    // Slot 1 of each side is the one nearer the screen's edge... in the player's side the left one, in the enemy's the
    // right one: they move outward, the other slot of the pair the other way
    switch (battlerType) {
    case BATTLER_TYPE_PLAYER_SIDE_SLOT_1:
    case BATTLER_TYPE_ENEMY_SIDE_SLOT_2:
        return -14;
    case BATTLER_TYPE_PLAYER_SIDE_SLOT_2:
    case BATTLER_TYPE_ENEMY_SIDE_SLOT_1:
        return 14;
    }
    return 0;
}

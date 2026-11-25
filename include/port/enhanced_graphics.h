#ifndef POKEPLATINUM_PORT_ENHANCED_GRAPHICS_H
#define POKEPLATINUM_PORT_ENHANCED_GRAPHICS_H

// Enhanced graphics (Settings > Display, libntr's settings; plan: docs/thor/dev/enhanced-graphics.md).
// The game tells libntr what light the scene wants (SIM_SetEnhancedScene); libntr's pass draws it.

#include <nitro/types.h>
#include <nitro/fx/fx.h>

#include "field/field_system_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Once per main loop iteration. fieldSystem: the field map being shown, or NULL anywhere else (menus, battles), which
// draws nothing extra
void EnhancedGraphics_Update(FieldSystem *fieldSystem);

// In the field map's 3D render, after the scene: the water tiles around the player as the water mask (libntr draws the
// reflections from it); nothing with Water reflections off
void EnhancedGraphics_DrawWaterMask(FieldSystem *fieldSystem);

// The sun casts the objects' shadows now (the game's own blob shadows stay hidden then)
BOOL EnhancedGraphics_SunShadowsShown(void);

// In the field map's 3D render, once the camera is set and before the map is drawn: libntr's sun shadows of the 3D need the
// game's projection and camera to read the depth buffer; and around the land and buildings being drawn, their own faint
// shadow polygons are not drawn while the sun's are shown
void EnhancedGraphics_SetCamera(const MtxFx44 *projection, const MtxFx43 *camera);
void EnhancedGraphics_LandDrawing(BOOL drawing);
// Around other models that should cast the sun's shadows (signposts, mailboxes: map objects drawn as models)
void EnhancedGraphics_CasterDrawing(BOOL drawing);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_ENHANCED_GRAPHICS_H

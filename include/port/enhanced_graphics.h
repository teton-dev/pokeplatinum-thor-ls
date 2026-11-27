#ifndef POKEPLATINUM_PORT_ENHANCED_GRAPHICS_H
#define POKEPLATINUM_PORT_ENHANCED_GRAPHICS_H

// Enhanced graphics (Settings > Display, libntr's settings; plan: docs/thor/dev/enhanced-graphics.md). The game-neutral
// part is port-common's port/enhanced_scene.h (EnhancedScene_*), shared with the HGSS port; this is Platinum's adapter:
// the hour, the outdoor test, the per-map looks (fog, lamps' textures) and the water tiles.

#include <nitro/types.h>

#include "field/field_system_decl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Once per main loop iteration. fieldSystem: the field map being shown, or NULL anywhere else (menus, battles), which
// draws nothing extra
void EnhancedGraphics_Update(FieldSystem *fieldSystem);

// In the field map's 3D render, before the camera is set (EnhancedScene_SetCamera): what this frame of the field needs
// from it (the fog's ground under the player, the lamps' textures in the map's texture sets), read while the field is
// surely there
void EnhancedGraphics_FieldDrawing(FieldSystem *fieldSystem);

// In the field map's 3D render, after the scene: the water tiles around the player as the water mask (libntr draws the
// reflections from it); nothing with Water reflections off
void EnhancedGraphics_DrawWaterMask(FieldSystem *fieldSystem);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_ENHANCED_GRAPHICS_H

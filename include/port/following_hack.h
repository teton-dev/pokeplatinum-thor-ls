#ifndef POKEPLATINUM_PORT_FOLLOWING_HACK_H
#define POKEPLATINUM_PORT_FOLLOWING_HACK_H

// Following Renegade Platinum (Mikelan98 and AdAstra's Following Platinum, integrated into Renegade
// Platinum). Its followers are in the game's scripts and cut scenes, and run as ARM code the hack adds
// to the ROM: weather_sys.narc's member 65, loaded at 0x023C8000 at startup, with hooks in arm9 and the
// overlays. The port can't run that code, so this file and the others behind SDK_PORT do what it does
// (docs/thor/dev/following-renegade.md has the map of the hack's code and tables).
//
// FollowingHack_Init() finds the hack in the data folder and extends the port's tables with the hack's
// (graphics of the Pokemon, movement types, script commands). The port's own follower (port/follower.h)
// is off while the hack is in the game.

#include <nitro/types.h>

#include "struct_decls/map_object.h"

#include "field/field_system_decl.h"
#include "overlay005/field_effect_manager.h"
#include "overlay005/field_effect_renderer.h"
#include "struct_defs/struct_020EDF0C.h"

#ifdef __cplusplus
extern "C" {
#endif

// Once at startup, with the ROM's files in the working directory
void FollowingHack_Init(void);

// The game is Following Renegade Platinum
BOOL FollowingHack_IsActive(void);

// The hack's code and tables as the DS has them (loaded at FOLLOWING_HACK_BASE), or NULL without the hack
#define FOLLOWING_HACK_BASE 0x023C8000
#define FOLLOWING_HACK_SIZE 0x18000
const u8 *FollowingHack_Blob(void);

// Hooks where the hack's code is called from the game's (see following_hack.c)
void FollowingHack_MapObjectsReady(FieldSystem *fieldSystem);    // after the map's objects are added
void FollowingHack_FrameDrawn(void);                             // after the field's effects are drawn
void FollowingHack_MapLoaded(FieldSystem *fieldSystem);          // a map is loaded: the follower comes out
void FollowingHack_MapConnected(FieldSystem *fieldSystem);       // the player walked into the next map
MapObject *FollowingHack_ScriptObject(FieldSystem *fieldSystem, int localID); // a script's object by local ID
BOOL FollowingHack_IsFollower(const MapObject *mapObj);
BOOL FollowingHack_IsPokemonGraphics(u32 graphicsID);
BOOL FollowingHack_ObjectAt(const MapObject *mapObj, int x, int y, int z); // sub_02063F00 with the follower // 276 to 841, the hack's Pokemon
const FieldEffectRendererFuncs *FollowingHack_RendererFuncs(u32 id); // the hack's field effect renderer
void FollowingHack_InitRenderer(FieldEffectManager *fieldEffMan);

// The follower's movement types (0x44, 0x45), or NULL for the others or without the hack
const UnkStruct_020EDF0C *FollowingHack_MovementType(u32 type);
// The player starts a step (player_move.c): the follower takes the player's last tile
void FollowingHack_PlayerStep(MapObject *player, u32 action);

// For the tests (follower-ball): the follower comes out of its ball (type 0) or goes back (1)
void FollowingHack_TestBall(FieldSystem *fieldSystem, int type);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_FOLLOWING_HACK_H

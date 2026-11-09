#ifndef GEN5_PLAYER_BACK_H
#define GEN5_PLAYER_BACK_H

// The port (teton-dev fork): the player's back sprite in Gen 5 battles is the owner's own art of Lucas or Dawn
// (gen5/art, converted by gen5/tools/player_back.py), drawn by MCSS in place of BW2's. BW2's own back sprite is still
// read and animated: its animation picks one of the art's poses for each of its multi-cells (standing, ball up, ball
// higher, winding up, thrown) and moves it as it moves its own, with its timing. Only the image and the palette are
// the art's (mcss.c's patch).

#include "types.h"

// A quad of a pose, in pixels: its top-left from the sprite's origin (under the feet, y up), its size, and where it is
// in the texture
typedef struct {
    s16 x;
    s16 y;
    u16 width;
    u16 height;
    u16 s;
    u16 t;
} Gen5BackQuad;

// A pose: its quads in the sprite's list, front to back
typedef struct {
    u8 first;
    u8 count;
} Gen5BackPose;

typedef struct {
    // A 256x128 4-bit texture (the size of each MCSS sprite's) and its 16 colors, the first transparent
    const u8 *texture;
    const u16 *palette;
    const Gen5BackQuad *quads;
    // One for each of BW2's multi-cells; a later one is drawn as the first
    const Gen5BackPose *poses;
    u8 poseCount;
} Gen5BackSprite;

#define GEN5_BACK_TEXTURE_SIZE 0x4000

struct MCSSSystem;

// The art for a BW2 trainer class shown as the player's back (the player's classes: Lucas for the male ones, Dawn for
// the female), or NULL to leave BW2's sprite
const Gen5BackSprite *PlayerBack_ForTrainerClass(u32 trainerClass);

// mcss.c's patch: the next sprite MCSSSys_Add adds is drawn with this art (NULL: its own)
void MCSSSys_SetNextBackSprite(struct MCSSSystem *system, const Gen5BackSprite *sprite);

#endif // GEN5_PLAYER_BACK_H

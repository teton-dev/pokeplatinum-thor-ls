// The port (teton-dev fork): the player's back sprite in Gen 5 battles, the owner's own art of Lucas and Dawn
// (gen5/include/gen5/player_back.h). The art's data is generated at build time from gen5/art by
// gen5/tools/player_back.py.

#include <string.h>

#include "types.h"
#include "constants/trainer_classes.h"
#include "gen5/player_back.h"
#include "gen5.h"

#include "gen5_player_back_data.h"

// The opposing trainers' art in this battle (Gen5_SetTrainerArt, runtime.c): their poses and the animation Platinum
// plays once; the later sprites of a trainer in the battle hold the last pose
typedef struct {
    u8 texture[GEN5_BACK_TEXTURE_SIZE];
    u16 palette[16];
    Gen5BackQuad quads[GEN5_ART_MAX_QUADS];
    Gen5BackPose poses[GEN5_ART_MAX_POSES];
    u8 seqPose[GEN5_ART_MAX_SEQ];
    u16 seqTicks[GEN5_ART_MAX_SEQ];
    Gen5BackSprite animated;
    Gen5BackSprite still;
    u32 trainerClass;
    int set;
    int adds;
} FrontArt;

static FrontArt sFront[GEN5_ART_SLOTS];

void Gen5_SetTrainerArt(int slot, u32 trainerClass, const Gen5Trainer *art) {
    FrontArt *front;
    int i;

    if (slot < 0 || slot >= GEN5_ART_SLOTS) {
        return;
    }
    front = &sFront[slot];
    front->set = 0;
    front->adds = 0;
    if (art == NULL || !art->hasArt || art->artPoseCount == 0) {
        return;
    }
    memcpy(front->texture, art->artTexture, GEN5_BACK_TEXTURE_SIZE);
    memcpy(front->palette, art->artPalette, sizeof(front->palette));
    for (i = 0; i < art->artQuadCount; i++) {
        front->quads[i].x = art->artQuad[i][0];
        front->quads[i].y = art->artQuad[i][1];
        front->quads[i].width = art->artQuad[i][2];
        front->quads[i].height = art->artQuad[i][3];
        front->quads[i].s = art->artQuad[i][4];
        front->quads[i].t = art->artQuad[i][5];
    }
    for (i = 0; i < art->artPoseCount; i++) {
        front->poses[i].first = art->artPoseFirst[i];
        front->poses[i].count = art->artPoseQuads[i];
    }
    for (i = 0; i < art->artSeqCount; i++) {
        front->seqPose[i] = art->artSeqPose[i];
        front->seqTicks[i] = art->artSeqTicks[i];
    }
    memset(&front->animated, 0, sizeof(front->animated));
    front->animated.texture = front->texture;
    front->animated.palette = front->palette;
    front->animated.quads = front->quads;
    front->animated.poses = front->poses;
    front->animated.poseCount = art->artPoseCount;
    front->animated.seqPose = front->seqPose;
    front->animated.seqTicks = front->seqTicks;
    front->animated.seqCount = art->artSeqCount;
    front->animated.lastPose = art->artLastPose;
    front->still = front->animated;
    front->still.seqCount = 0;
    front->still.useFixed = 1;
    front->trainerClass = trainerClass;
    front->set = 1;
}

const Gen5BackSprite *Gen5_TrainerFrontArt(u32 trainerClass) {
    int i;

    for (i = 0; i < GEN5_ART_SLOTS; i++) {
        FrontArt *front = &sFront[i];
        if (front->set && front->trainerClass == trainerClass) {
            // The first time the trainer appears (the introduction) its animation plays; when it slides in again,
            // for its words, it stands in the last pose
            return front->adds++ == 0 && front->animated.seqCount > 0 ? &front->animated : &front->still;
        }
    }
    return NULL;
}

const Gen5BackSprite *PlayerBack_ForTrainerClass(u32 trainerClass) {
    switch (trainerClass) {
    // BW2's player classes: its own players (Nate, Rosa), their Pokéwood and Black/White versions. A battle the port
    // runs gives the player one of the last (a game other than BW2: btl_main.c's TrainerData_SetFromPlayer)
    case TRAINER_CLASS_PKMN_TRAINER_NATE:
    case TRAINER_CLASS_PKMN_TRAINER_M_180:
    case TRAINER_CLASS_PKMN_TRAINER_M_182:
    case TRAINER_CLASS_PKMN_TRAINER_M_184:
        return &sLucasBack;
    case TRAINER_CLASS_PKMN_TRAINER_ROSA:
    case TRAINER_CLASS_PKMN_TRAINER_F_181:
    case TRAINER_CLASS_PKMN_TRAINER_F_183:
    case TRAINER_CLASS_PKMN_TRAINER_F_185:
        return &sDawnBack;
    default:
        return NULL;
    }
}

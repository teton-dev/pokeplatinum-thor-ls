// The port (teton-dev fork): the player's back sprite in Gen 5 battles, the owner's own art of Lucas and Dawn
// (gen5/include/gen5/player_back.h). The art's data is generated at build time from gen5/art by
// gen5/tools/player_back.py.

#include <string.h>

#include "types.h"
#include "constants/trainer_classes.h"
#include "gen5/player_back.h"
#include "gen5.h"

#include "gen5_player_back_data.h"

// The opposing trainer's art in this battle (Gen5_SetTrainerArt, runtime.c): its poses and the animation Platinum plays
// once; the later sprites of the trainer in the battle hold the last pose
static u8 sFrontTexture[GEN5_BACK_TEXTURE_SIZE];
static u16 sFrontPalette[16];
static Gen5BackQuad sFrontQuads[GEN5_ART_MAX_QUADS];
static Gen5BackPose sFrontPoses[GEN5_ART_MAX_POSES];
static u8 sFrontSeqPose[GEN5_ART_MAX_SEQ];
static u16 sFrontSeqTicks[GEN5_ART_MAX_SEQ];
static Gen5BackSprite sFrontAnimated;
static Gen5BackSprite sFrontStill;
static u32 sFrontClass;
static int sFrontSet;
static int sFrontAdds;

void Gen5_SetTrainerArt(u32 trainerClass, const Gen5Trainer *art) {
    int i;

    sFrontSet = 0;
    sFrontAdds = 0;
    if (art == NULL || !art->hasArt || art->artPoseCount == 0) {
        return;
    }
    memcpy(sFrontTexture, art->artTexture, GEN5_BACK_TEXTURE_SIZE);
    memcpy(sFrontPalette, art->artPalette, sizeof(sFrontPalette));
    for (i = 0; i < art->artQuadCount; i++) {
        sFrontQuads[i].x = art->artQuad[i][0];
        sFrontQuads[i].y = art->artQuad[i][1];
        sFrontQuads[i].width = art->artQuad[i][2];
        sFrontQuads[i].height = art->artQuad[i][3];
        sFrontQuads[i].s = art->artQuad[i][4];
        sFrontQuads[i].t = art->artQuad[i][5];
    }
    for (i = 0; i < art->artPoseCount; i++) {
        sFrontPoses[i].first = art->artPoseFirst[i];
        sFrontPoses[i].count = art->artPoseQuads[i];
    }
    for (i = 0; i < art->artSeqCount; i++) {
        sFrontSeqPose[i] = art->artSeqPose[i];
        sFrontSeqTicks[i] = art->artSeqTicks[i];
    }
    memset(&sFrontAnimated, 0, sizeof(sFrontAnimated));
    sFrontAnimated.texture = sFrontTexture;
    sFrontAnimated.palette = sFrontPalette;
    sFrontAnimated.quads = sFrontQuads;
    sFrontAnimated.poses = sFrontPoses;
    sFrontAnimated.poseCount = art->artPoseCount;
    sFrontAnimated.seqPose = sFrontSeqPose;
    sFrontAnimated.seqTicks = sFrontSeqTicks;
    sFrontAnimated.seqCount = art->artSeqCount;
    sFrontAnimated.lastPose = art->artLastPose;
    sFrontStill = sFrontAnimated;
    sFrontStill.seqCount = 0;
    sFrontStill.useFixed = 1;
    sFrontClass = trainerClass;
    sFrontSet = 1;
}

const Gen5BackSprite *Gen5_TrainerFrontArt(u32 trainerClass) {
    if (!sFrontSet || trainerClass != sFrontClass) {
        return NULL;
    }
    // The first time the trainer appears (the introduction) its animation plays; when it slides in again, for its
    // words, it stands in the last pose
    return sFrontAdds++ == 0 && sFrontAnimated.seqCount > 0 ? &sFrontAnimated : &sFrontStill;
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

// BW2's sound for the Gen 5 battles, silent for now: in place of pokebw2's src/system/snd_sys.c and snd_bgm_stack.c
//
// BW2's sound runs on NitroSystem's sound library with its own sound data (swan_sound_data.sdat), and the library has
// one sound archive at a time, which Platinum's sound system holds. So the battle's music, sound effects and (in
// runtime.c) cries make no sound yet: what the battle calls is here, keeping only what it waits on, the music's fades,
// which take their frames as they would. Playing BW2's sound is for later (docs/thor/dev/gen5-battles.md).

#include "types.h"

#include "gfl/sound.h"

// The sound players BW2 has (snd_sys.c's SND_PLAYER_COUNT); their handles are never bound, so NitroSystem's calls on
// them do nothing
#define PLAYER_COUNT 5

static NNSSndHandle sHandles[PLAYER_COUNT];
static BOOL sHandlesReady;
static u32 sBGM;
static s32 sFadeFrames;

// Each frame of a battle (runtime.c), as GFL_SndUpdate
void Gen5_SoundUpdate(void)
{
    if (sFadeFrames != 0) {
        sFadeFrames--;
    }
}

NNSSndHandle *GFL_SndPlayerGetHandle(s32 player)
{
    if (!sHandlesReady) {
        for (int i = 0; i < PLAYER_COUNT; i++) {
            NNS_SndHandleInit(&sHandles[i]);
        }
        sHandlesReady = TRUE;
    }
    if (player < 0 || player >= PLAYER_COUNT) {
        player = 0;
    }
    return &sHandles[player];
}

void GFL_SndBGMPlay(u32 bgm, u32 channelMask)
{
    (void)channelMask;
    sBGM = bgm;
}

u32 GFL_SndBGMGetID(void)
{
    return sBGM;
}

BOOL GFL_SndBGMIsPlaying(void)
{
    return FALSE;
}

void GFL_SndBGMFadeIn(u16 frames)
{
    sFadeFrames = frames;
}

void GFL_SndBGMFadeOut(u16 frames)
{
    sFadeFrames = frames;
}

BOOL GFL_SndBGMIsFading(void)
{
    return sFadeFrames != 0;
}

void GFL_SndBGMPush(void)
{
}

void GFL_SndBGMPop(void)
{
}

void GFL_SndBGMSetPaused(BOOL paused)
{
    (void)paused;
}

void GFL_SndBGMStopImmediate(void)
{
    sBGM = 0;
    sFadeFrames = 0;
}

BOOL GFL_SndIsLoadThreadRunning(void)
{
    return FALSE;
}

BOOL GFL_SndIsPlaying(u32 seq)
{
    (void)seq;
    return FALSE;
}

BOOL GFL_SndPlayerIsActive(s32 player)
{
    (void)player;
    return FALSE;
}

BOOL GFL_SndPlayerIsActiveAny(void)
{
    return FALSE;
}

void GFL_SndPlayerSetMuteStateEx(BOOL on, u32 playerMask)
{
    (void)on;
    (void)playerMask;
}

void GFL_SndPlayerSetParams(s32 player, s32 tempoRatio, s32 pitch, s32 pan)
{
    (void)player;
    (void)tempoRatio;
    (void)pitch;
    (void)pan;
}

void GFL_SndPlayerStop(s32 player)
{
    (void)player;
}

void GFL_SndStop(void)
{
}

void GFL_SndSEPlay(u32 se)
{
    (void)se;
}

void GFL_SEPlayKeepVol(u32 se, s32 player)
{
    (void)se;
    (void)player;
}

// The player a sequence plays on: from the sound archive, which isn't loaded; the battle uses it with the handles
s32 GFL_SndSeqGetPlayerIndex(u32 seq)
{
    (void)seq;
    return 1;
}

// BW2's sound for the Gen 5 battles: in place of pokebw2's src/system/snd_sys.c and snd_bgm_stack.c, and main.c's
// cry callbacks
//
// The battle's sound effects and the Pokémon's cries are BW2's, from the sound data of the BW2 ROM the player imported
// (gen5/swan_sound_data.sdat); the music stays Platinum's. NitroSystem's sound library (libntrsystem) has one current
// sound archive, Platinum's, which its sound system opened at boot. BW2's is opened next to it, with a heap of its own,
// and is made the current one while BW2's code runs (Gen5_SoundEnter and Gen5_SoundLeave, around each battle frame),
// so that what BW2's code asks of the library (a sequence's player, a cry's wave archive) is answered from BW2's data.
//
// Sequences: NitroSystem's sound players are set up once from Platinum's archive (NNS_SndArcPlayerSetup), and
// Platinum's music plays on them during the battle. BW2's sound effects play on Platinum's four sound effect players
// and its cry player (BW2's players 1 to 5), given BW2's channels for the battle and Platinum's back after it. A
// sequence's data (with its bank and wave archives) is loaded into the heap the first time it plays and stays there.
// The music calls (GFL_SndBGM...) keep only the fades' timing, which the battle waits for; BW2's music and fanfares
// aren't played.
//
// Cries: pm_voice.c reads a cry's wave archive from the current archive and plays it on wave output channels 10 to 15,
// as on BW2.

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "constants/sound.h"
#include "constants/species.h"
#include "gfl/arc.h"
#include "gfl/sound.h"

// The sound effect players BW2 has (snd_sys.c's SND_PLAYER_COUNT): BW2's players 1 to 5
#define PLAYER_COUNT 5
// The NitroSystem player each of BW2's plays on: Platinum's PLAYER_SE_1 to PLAYER_SE_4 (3 to 6), and PLAYER_PV (0)
// for BW2's player 5 (two sequences use it). Platinum's music players (1, 2 and 7) are left alone
static const int sPortPlayer[PLAYER_COUNT] = { 3, 4, 5, 6, 0 };

// BW2's heap was 0x9d000 bytes with its music. A battle's sound effects take under 100 KB (the archive's tables
// about 0x8000); when less than HEAP_LOW is left, a battle's start empties it (its sequences load again as they play)
#define HEAP_SIZE (2 * 1024 * 1024)
#define HEAP_LOW (512 * 1024)
#define SDAT_NAME "swan_sound_data.sdat"
#define PATH_MAX_LENGTH 512

// The cries (pm_voice.c and main.c's getResIDFunc and getWaveInfoFunc)
#define POKE_VOICE_SPECIES_MAX 684
#define POKE_VOICE_DIRECT_MIN 652
#define ARCID_POKE_VOICE_DATA 191

typedef struct {
    u16 unk0;
    u16 volume;
} PokeVoiceData;

typedef struct {
    u16 species;
    u16 form;
    u16 waveArc;
} PokeVoiceFormEntry;

static const PokeVoiceFormEntry sVoiceForms[] = {
    { SPECIES_SHAYMIN, 1, 685 },
    { SPECIES_KYUREM, 1, 686 },
    { SPECIES_KYUREM, 2, 687 },
    { SPECIES_TORNADUS, 1, 688 },
    { SPECIES_THUNDURUS, 1, 689 },
    { SPECIES_LANDORUS, 1, 690 },
};

typedef struct {
    u32 seq;
    NNSSndHandle handle;
} SndPlayer;

static NNSSndArc sArc;
static BOOL sArcOpen;
static void *sHeapMemory;
static NNSSndHeapHandle sHeap;
// The heap's level with the archive's tables in it, and nothing else
static int sHeapBase;
// Platinum's archive while BW2's is the current one (Gen5_SoundEnter)
static NNSSndArc *sPlatinumArc;
static int sEnterDepth;
static BOOL sBattle;
static BOOL sVolumeChanged;

static SndPlayer sPlayers[PLAYER_COUNT];
static BOOL sHandlesReady;
static u32 sBGM;
static s32 sFadeFrames;
static u32 sLoadFailures;

// What a battle's sound did, for the log at its end (the tests' check, with no audio in the container): the sound
// effects started, the frames on which one of them played, a cry played, and Platinum's music played
static struct {
    u32 started;
    u32 frames;
    u32 seFrames;
    u32 cryFrames;
    u32 musicFrames;
} sStats;
// Platinum's music player (PLAYER_BGM)
#define PLATINUM_PLAYER_BGM 7

static void InitHandles(void)
{
    if (!sHandlesReady) {
        for (int i = 0; i < PLAYER_COUNT; i++) {
            NNS_SndHandleInit(&sPlayers[i].handle);
        }
        sHandlesReady = TRUE;
    }
}

// The cries' wave archive of a species and form (BW2's getResIDFunc)
static void VoiceResID(u32 species, u32 form, u32 *waveArc)
{
    if (species < 1 || species > POKE_VOICE_SPECIES_MAX) {
        *waveArc = 1;
        return;
    }
    if (species >= POKE_VOICE_DIRECT_MIN) {
        *waveArc = species;
        return;
    }
    for (u32 i = 0; i < NELEMS(sVoiceForms); i++) {
        if (sVoiceForms[i].species == species && sVoiceForms[i].form == form) {
            *waveArc = sVoiceForms[i].waveArc;
            return;
        }
    }
    *waveArc = species;
}

// A cry's volume (BW2's getWaveInfoFunc). FALSE: pm_voice.c reads the wave archive. With no sound archive, a cry of
// no samples, which pm_voice.c doesn't play. Chatot's recording isn't played (Platinum's save keeps it its own way)
static BOOL VoiceWaveInfo(u32 species, u32 form, const PokeVoiceChatterInfo *chatterInfo, void **samples, u32 *size,
    u32 *sampleRate, u32 *speed, s8 *volume)
{
    (void)form;
    (void)chatterInfo;
    (void)samples;
    (void)sampleRate;
    (void)speed;
    if (!sArcOpen) {
        *size = 0;
        *volume = 0;
        return TRUE;
    }
    if (species < 1 || species > POKE_VOICE_SPECIES_MAX) {
        species = 1;
    }
    PokeVoiceData data;
    GFL_ArcSysRead(&data, ARCID_POKE_VOICE_DATA, species);
    *volume = (s8)data.volume;
    return FALSE;
}

BOOL Gen5_SoundInit(void)
{
    InitHandles();
    PokeVoice_Init(1, VoiceResID, VoiceWaveInfo);
    if (sArcOpen) {
        return TRUE;
    }
    char path[PATH_MAX_LENGTH];
    snprintf(path, sizeof(path), "%s/%s", Gen5_FilesBase(), SDAT_NAME);
    // libntr asks for a ROM when a file it opens is missing: make sure it's there first
    if (access(path, R_OK) != 0) {
        printf("Gen 5: no sound data (%s)\n", path);
        return FALSE;
    }
    sHeapMemory = malloc(HEAP_SIZE);
    if (sHeapMemory == NULL) {
        return FALSE;
    }
    sHeap = NNS_SndHeapCreate(sHeapMemory, HEAP_SIZE);
    NNSSndArc *platinum = NNS_SndArcGetCurrent();
    NNS_SndArcInit(&sArc, path, sHeap, FALSE);
    // NNS_SndArcInit makes the new archive the current one
    NNS_SndArcSetCurrent(platinum);
    if (sArc.info == NULL || sArc.fat == NULL) {
        printf("Gen 5: BW2's sound data (%s) didn't open\n", path);
        return FALSE;
    }
    sHeapBase = NNS_SndHeapSaveState(sHeap);
    sArcOpen = TRUE;
    printf("Gen 5: BW2's sound data is open (%s), %lu bytes of sound heap free\n", path,
        (unsigned long)NNS_SndHeapGetFreeSize(sHeap));
    return TRUE;
}

void Gen5_SoundEnter(void)
{
    if (sArcOpen && sEnterDepth++ == 0) {
        sPlatinumArc = NNS_SndArcSetCurrent(&sArc);
    }
}

void Gen5_SoundLeave(void)
{
    if (sArcOpen && sEnterDepth > 0 && --sEnterDepth == 0) {
        NNS_SndArcSetCurrent(sPlatinumArc);
    }
}

// The channels a player may take, from an archive's player table (the current one's)
static void SetPlayerChannels(int portPlayer, int archivePlayer)
{
    const NNSSndArcPlayerInfo *info = NNS_SndArcGetPlayerInfo(archivePlayer);
    if (info != NULL) {
        NNS_SndPlayerSetAllocatableChannel(portPlayer, info->allocChBitFlag);
    }
}

void Gen5_SoundBattleBegin(void)
{
    sBGM = 0;
    sFadeFrames = 0;
    sVolumeChanged = FALSE;
    memset(&sStats, 0, sizeof(sStats));
    for (int i = 0; i < PLAYER_COUNT; i++) {
        sPlayers[i].seq = 0;
    }
    if (!sArcOpen) {
        return;
    }
    // The last battle's sounds stopped at its end
    if (NNS_SndHeapGetFreeSize(sHeap) < HEAP_LOW) {
        NNS_SndHeapLoadState(sHeap, sHeapBase);
    }
    // Platinum's sound effect players get BW2's channels: channels 14 and 15 (and 10 to 13 with more than one cry)
    // are the cries' while they play
    Gen5_SoundEnter();
    for (int i = 0; i < PLAYER_COUNT; i++) {
        SetPlayerChannels(sPortPlayer[i], i + 1);
    }
    Gen5_SoundLeave();
    sBattle = TRUE;
}

void Gen5_SoundBattleEnd(void)
{
    if (!sBattle) {
        return;
    }
    sBattle = FALSE;
    PokeVoice_ReleaseAll();
    for (int i = 0; i < PLAYER_COUNT; i++) {
        NNS_SndPlayerStopSeq(&sPlayers[i].handle, 0);
        sPlayers[i].seq = 0;
        // Platinum's channels and volume back
        SetPlayerChannels(sPortPlayer[i], sPortPlayer[i]);
        if (sVolumeChanged) {
            NNS_SndPlayerSetPlayerVolume(sPortPlayer[i], SND_VOLUME_MAX);
        }
    }
    printf("Gen 5: the battle's sound: %lu sound effects started, one playing on %lu of %lu frames, a cry on %lu, "
           "Platinum's music on %lu; %lu bytes of sound heap free\n",
        (unsigned long)sStats.started, (unsigned long)sStats.seFrames, (unsigned long)sStats.frames,
        (unsigned long)sStats.cryFrames, (unsigned long)sStats.musicFrames, (unsigned long)NNS_SndHeapGetFreeSize(sHeap));
}

void Gen5_SoundUpdate(void)
{
    if (sFadeFrames != 0) {
        sFadeFrames--;
    }
    sStats.frames++;
    if (GFL_SndPlayerIsActiveAny()) {
        sStats.seFrames++;
    }
    if (PokeVoice_IsPlayingAny()) {
        sStats.cryFrames++;
    }
    if (NNS_SndPlayerCountPlayingSeqByPlayerNo(PLATINUM_PLAYER_BGM) != 0) {
        sStats.musicFrames++;
    }
}

NNSSndHandle *GFL_SndPlayerGetHandle(s32 player)
{
    InitHandles();
    if (player < 0 || player >= PLAYER_COUNT) {
        player = 0;
    }
    return &sPlayers[player].handle;
}

// Plays a sound effect: seq is BW2's, player one of BW2's sound effect players (0 to 4), or -1 for the sequence's own
static BOOL PlaySeq(u32 seq, s32 player)
{
    if (!sArcOpen || seq == 0) {
        return FALSE;
    }
    BOOL played = FALSE;
    Gen5_SoundEnter();
    const NNSSndSeqParam *param = seq < NNS_SndArcGetSeqCount() ? NNS_SndArcGetSeqParam(seq) : NULL;
    if (param != NULL) {
        if (player < 0) {
            player = param->playerNo - 1;
        }
        if (player < 0 || player >= PLAYER_COUNT) {
            // A sequence of BW2's music players: the music stays Platinum's
            printf("Gen 5: sound %lu is music (player %d), not played\n", (unsigned long)seq, param->playerNo);
        } else if (!NNS_SndArcLoadSeq(seq, sHeap)) {
            if (sLoadFailures++ < 8) {
                printf("Gen 5: sound %lu didn't load (%lu bytes of sound heap free)\n", (unsigned long)seq,
                    (unsigned long)NNS_SndHeapGetFreeSize(sHeap));
            }
        } else {
            SndPlayer *state = &sPlayers[player];
            state->seq = 0;
            if (NNS_SndArcPlayerStartSeqEx(&state->handle, sPortPlayer[player], -1, -1, seq)) {
                state->seq = seq;
                played = TRUE;
                sStats.started++;
            }
        }
    }
    Gen5_SoundLeave();
    return played;
}

void GFL_SndSEPlay(u32 se)
{
    PlaySeq(se, -1);
}

void GFL_SEPlayKeepVol(u32 se, s32 player)
{
    PlaySeq(se, player);
}

s32 GFL_SndSeqGetPlayerIndex(u32 seq)
{
    if (!sArcOpen || seq < SEQ_SE_DUMMY || seq >= SEQ_SE_END) {
        return 1;
    }
    s32 player = 1;
    Gen5_SoundEnter();
    if (seq < NNS_SndArcGetSeqCount()) {
        player = NNS_SndArcGetSeqParam(seq)->playerNo - 1;
    }
    Gen5_SoundLeave();
    return player;
}

void GFL_SndPlayerStop(s32 player)
{
    if (player >= 0 && player < PLAYER_COUNT) {
        NNS_SndPlayerStopSeq(&sPlayers[player].handle, 0);
    }
}

void GFL_SndStop(void)
{
    for (int i = 0; i < PLAYER_COUNT; i++) {
        GFL_SndPlayerStop(i);
    }
}

BOOL GFL_SndPlayerIsActive(s32 player)
{
    return player >= 0 && player < PLAYER_COUNT && NNS_SndHandleIsValid(&sPlayers[player].handle);
}

BOOL GFL_SndPlayerIsActiveAny(void)
{
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (GFL_SndPlayerIsActive(i)) {
            return TRUE;
        }
    }
    return FALSE;
}

// Whether a sound effect of BW2's plays. By the handles: NitroSystem counts by number, and Platinum's music has
// numbers of its own
BOOL GFL_SndIsPlaying(u32 seq)
{
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (sPlayers[i].seq == seq && NNS_SndHandleIsValid(&sPlayers[i].handle)) {
            return TRUE;
        }
    }
    return FALSE;
}

// On: the players' full volume; off: silent. BW2's mask is by player number, 0 its music's (left alone) and 1 to 5
// its sound effects'
void GFL_SndPlayerSetMuteStateEx(BOOL on, u32 playerMask)
{
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (playerMask & (1 << (i + 1))) {
            NNS_SndPlayerSetPlayerVolume(sPortPlayer[i], on == TRUE ? SND_VOLUME_MAX : 0);
            sVolumeChanged = TRUE;
        }
    }
}

void GFL_SndPlayerSetParams(s32 player, s32 tempoRatio, s32 pitch, s32 pan)
{
    if (player < 0 || player >= PLAYER_COUNT) {
        return;
    }
    NNSSndHandle *handle = &sPlayers[player].handle;
    if (tempoRatio != -1) {
        NNS_SndPlayerSetTempoRatio(handle, tempoRatio);
    }
    if (pitch != -1) {
        NNS_SndPlayerSetTrackPitch(handle, SND_CHANNEL_MASK_ALL, pitch);
    }
    if (pan != -1) {
        NNS_SndPlayerSetTrackPan(handle, SND_CHANNEL_MASK_ALL, pan);
    }
}

// The music: Platinum's plays on. What the battle waits for, the fades, take their frames

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

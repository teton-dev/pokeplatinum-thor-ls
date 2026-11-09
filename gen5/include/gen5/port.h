#ifndef GEN5_PORT_H
#define GEN5_PORT_H

// Included before every pokebw2 file of the Gen 5 battles build: what pokebw2's own copies of the SDKs
// (lib/nitro, lib/nnsys) declare and libntr's NitroSDK 4 and NitroSystem name otherwise or leave out.

#include <nitro.h>
#include <nnsys/g3d.h>
#include <string.h>

// The port's interface (Gen5_... functions and their types)
#include "gen5.h"

// NitroSDK's older name, which pokebw2's fx.h keeps
#ifndef DEG_TO_IDX
#define DEG_TO_IDX(deg) ((u16)((deg) * 0x10000 / 360))
#endif

// pokebw2's gx.h: a BG's screen and character bases by their offsets, and all planes
#ifndef GX_BG_SCRBASE
#define GX_BG_SCRBASE(offset) ((offset) / 0x800)
#define GX_BG_CHARBASE(offset) ((offset) / 0x4000)
#endif
#ifndef GX_PLANEMASK_ALL
#define GX_PLANEMASK_ALL 0x1f
#endif

// OS_AllocFromHeap and friends: no heap
#ifndef OS_HEAP_INVALID
#define OS_HEAP_INVALID (-1)
#endif

// TwlSDK: Black 2 and White 2 run on a DS as well as a DSi, and the port is a DS (SDK 4 has no TWL calls)
#if SDK_VERSION_MAJOR < 5
#define OS_IsRunOnTwl() (FALSE)

static inline u32 PM_GetBatteryLevel(u16 *level) {
    (void)level;
    return PM_RESULT_ERROR;
}
#endif

// TwlSDK's power-management callbacks, which pokebw2's twl/pm.h declares functions with
typedef void (*PMGenCallback)(void *arg);

typedef struct PMGenCallbackInfo {
    PMGenCallback callback;
    void *arg;
    int priority;
    struct PMGenCallbackInfo *next;
} PMGenCallbackInfo;

// The DS's 4 MB of debug RAM, which gfl_use.c sets up as a heap on a DS (the port's is in gen5/src/sdk.c)
OSHeapHandle OS_CreateExtraHeap(OSArenaId arena);

// pokebw2's name for a copy whose ranges may overlap
static inline void MI_CpuMove(const void *src, void *dest, u32 size) {
    memmove(dest, src, size);
}

// GFL's clock cache (src/gfl/rtc_cache.c), which pokebw2 declares in its copy of nitro/rtc.h
void RTC_GetCachedDate(RTCDate *date);
void RTC_GetCachedTime(RTCTime *time);

// NitroSystem's G3D file signatures by the names of pokebw2's nnsys/g3d.h
#define NNS_G3D_SIGNATURE_BMD0 NNS_G3D_SIGNATURE_NSBMD
#define NNS_G3D_SIGNATURE_BTX0 NNS_G3D_SIGNATURE_NSBTX
#define NNS_G3D_SIGNATURE_BCA0 NNS_G3D_SIGNATURE_NSBCA
#define NNS_G3D_SIGNATURE_BVA0 NNS_G3D_SIGNATURE_NSBVA
#define NNS_G3D_SIGNATURE_BMA0 NNS_G3D_SIGNATURE_NSBMA
#define NNS_G3D_SIGNATURE_BTP0 NNS_G3D_SIGNATURE_NSBTP
#define NNS_G3D_SIGNATURE_BTA0 NNS_G3D_SIGNATURE_NSBTA

// The port's heaps for GFL (gen5/src/runtime.c), in place of GFLAppInit's (gfl_use.c's patch)
void Gen5_MemInit(void);

// An archive's file in the extracted ROM's folder once Gen5_FilesInit has run, else BW2's own path (gen5/src/files.c,
// for GFL_ArcSysGetResourcePath: gfl_use.c's patch)
const char *Gen5_ArcPath(u32 arcId, const char *romPath);

// The keys and the touch the game read this frame (gen5/src/runtime.c, from Gen5_SetInput), for key.c's and
// touchpanel.c's patches
u16 Gen5_PadRead(void);
void Gen5_TouchRead(TPData *touch);

// The extracted ROM's folder as a path from the game's folder, once Gen5_FilesInit has run (gen5/src/files.c)
const char *Gen5_FilesBase(void);

// BW2's sound effects and cries from its own sound archive, next to Platinum's (gen5/src/sound.c, runtime.c). Init
// opens the archive once; a battle's Begin and End set up and give back the sound players it borrows from Platinum;
// Enter and Leave make BW2's archive NitroSystem's current one while BW2's code runs, and Platinum's again after.
// Init also starts the cries (PokeVoice_Init)
BOOL Gen5_SoundInit(void);
void Gen5_SoundBattleBegin(void);
void Gen5_SoundBattleEnd(void);
void Gen5_SoundEnter(void);
void Gen5_SoundLeave(void);
// Each frame of a battle, as GFL_SndUpdate: the music fades' timing
void Gen5_SoundUpdate(void);

// What happened in the battle, for Gen5_TakeEvents (gen5/src/runtime.c; the events are in api/gen5.h)
void Gen5_Event(u32 event);

// 64 bits: a pointer as a 32-bit handle, and back, where BW2 keeps pointers in 32-bit values (gen5/src/pointers.c)
u32 Gen5_PtrToU32(const void *pointer);
void *Gen5_U32ToPtr(u32 handle);

// A multi-cell (NMCR) file's bank in the 64-bit layout, replacing the file (gen5/src/g2d.c, for arc_util.c's patch)
struct NNSG2dMultiCellDataBank;
BOOL Gen5_UnpackMultiCellBank(void **file, struct NNSG2dMultiCellDataBank **bank, u32 heapId);

#endif // GEN5_PORT_H

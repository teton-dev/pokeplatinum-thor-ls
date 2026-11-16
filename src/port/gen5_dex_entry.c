// The Pokédex entry Platinum's battle shows for a newly caught species (battle_script.c's catch sequence:
// ov21_021E8D48), after a battle in BW2's engine, whose own entry screen is in its field (not built). The battle sets
// up the display that screen draws into (BGs, palettes, the 3D Pokémon sprite, OAM and its transfers); this does the
// same alone, as the evolution scene does after a battle (evolution.c), then runs Platinum's screen unchanged: its
// sprite fades in, the cry plays, A or a touch closes it.
#include "port/gen5_dex_entry.h"

#include <nitro.h>
#include <stdio.h>
#include <string.h>

#include "applications/pokedex/ov21_021E8D48.h"
#include "applications/pokedex/struct_ov21_021E8E0C.h"

#include "bg_window.h"
#include "dexmode_checker.h"
#include "font.h"
#include "g3d_pipeline.h"
#include "gx_layers.h"
#include "hardware_palette.h"
#include "heap.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_sprite.h"
#include "sound_playback.h"
#include "sprite_system.h"
#include "system.h"
#include "touch_screen.h"
#include "unk_0202419C.h"
#include "unk_0207C63C.h"
#include "vram_transfer.h"

// The battle's (battle_main.c)
static const RenderOamTemplate sOamTemplate = {
    .mainOamStart = 0,
    .mainOamCount = 128,
    .mainAffineOamStart = 0,
    .mainAffineOamCount = 32,
    .subOamStart = 0,
    .subOamCount = 128,
    .subAffineOamStart = 0,
    .subAffineOamCount = 32,
};

static const CharTransferTemplateWithModes sTransferTemplate = {
    .maxTasks = 0x60,
    .sizeMain = 0x10000,
    .sizeSub = 0x4000,
    .modeMain = GX_OBJVRAMMODE_CHAR_1D_64K,
    .modeSub = GX_OBJVRAMMODE_CHAR_1D_32K
};

enum {
    DEX_ENTRY_SHOWING,
    DEX_ENTRY_FADING_OUT,
    DEX_ENTRY_DONE,
};

static struct {
    enum HeapID heapID;
    int state;
    BgConfig *bgConfig;
    PaletteData *paletteData;
    PokemonSpriteManager *monSpriteMan;
    G3DPipelineBuffers *pipelineBuffers;
    SpriteSystem *spriteSys;
    UnkStruct_ov21_021E8D48 *entry;
} sDex;

static void VBlank(void *data)
{
    (void)data;
    PokemonSpriteManager_UpdateCharAndPltt(sDex.monSpriteMan);
    VramTransfer_Process();
    PaletteData_CommitFadedBuffers(sDex.paletteData);
    Bg_RunScheduledUpdates(sDex.bgConfig);
    SpriteSystem_TransferOam();
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
}

static void InitGraphics(void)
{
    GXLayers_DisableEngineALayers();

    GXBanks banks = {
        GX_VRAM_BG_128_C,
        GX_VRAM_BGEXTPLTT_NONE,
        GX_VRAM_SUB_BG_32_H,
        GX_VRAM_SUB_BGEXTPLTT_NONE,
        GX_VRAM_OBJ_64_E,
        GX_VRAM_OBJEXTPLTT_NONE,
        GX_VRAM_SUB_OBJ_16_I,
        GX_VRAM_SUB_OBJEXTPLTT_NONE,
        GX_VRAM_TEX_01_AB,
        GX_VRAM_TEXPLTT_01_FG
    };
    GXLayers_SetBanks(&banks);

    MI_CpuClear32((void *)HW_BG_VRAM, HW_BG_VRAM_SIZE);
    MI_CpuClear32((void *)HW_DB_BG_VRAM, HW_DB_BG_VRAM_SIZE);
    MI_CpuClear32((void *)HW_OBJ_VRAM, HW_OBJ_VRAM_SIZE);
    MI_CpuClear32((void *)HW_DB_OBJ_VRAM, HW_DB_OBJ_VRAM_SIZE);

    GraphicsModes graphicsModes = {
        GX_DISPMODE_GRAPHICS,
        GX_BGMODE_0,
        GX_BGMODE_0,
        GX_BG0_AS_3D
    };
    SetAllGraphicsModes(&graphicsModes);

    // The bottom screen stays black, as the battle's touch screen goes dark behind the entry
    GXLayers_EngineBToggleLayers(GX_PLANEMASK_BG0 | GX_PLANEMASK_BG1 | GX_PLANEMASK_BG2 | GX_PLANEMASK_BG3 | GX_PLANEMASK_OBJ, 0);
    GX_SetVisibleWnd(GX_WNDMASK_NONE);
    GXS_SetVisibleWnd(GX_WNDMASK_NONE);
    GXLayers_TurnBothDispOn();
    GXLayers_EngineAToggleLayers(GX_PLANEMASK_OBJ, 1);
}

BOOL Gen5DexEntry_Start(Pokemon *mon, const Pokedex *pokedex, enum HeapID heapID)
{
    memset(&sDex, 0, sizeof(sDex));
    sDex.heapID = heapID;

    MI_CpuFill16((void *)GetHardwareMainBgPaletteAddress(), 0, GetHardwareMainBgPaletteSize());
    MI_CpuFill16((void *)GetHardwareSubBgPaletteAddress(), 0, GetHardwareSubBgPaletteSize());

    sDex.paletteData = PaletteData_New(heapID);
    PaletteData_SetAutoTransparent(sDex.paletteData, TRUE);
    PaletteData_AllocBuffer(sDex.paletteData, PLTTBUF_MAIN_BG, PALETTE_SIZE_BYTES * 16, heapID);
    PaletteData_AllocBuffer(sDex.paletteData, PLTTBUF_SUB_BG, PALETTE_SIZE_BYTES * 16, heapID);
    PaletteData_AllocBuffer(sDex.paletteData, PLTTBUF_MAIN_OBJ, PALETTE_SIZE_BYTES * 14, heapID);
    PaletteData_AllocBuffer(sDex.paletteData, PLTTBUF_SUB_OBJ, PALETTE_SIZE_BYTES * 16, heapID);

    sDex.bgConfig = BgConfig_New(heapID);
    sDex.pipelineBuffers = sub_0207C690(heapID);
    sub_0207C63C();
    sub_0207C664();
    sub_0207C730();
    InitGraphics();

    VramTransfer_New(64, heapID);
    sDex.spriteSys = SpriteSystem_Alloc(heapID);
    SpriteSystem_Init(sDex.spriteSys, &sOamTemplate, &sTransferTemplate, 16 + 16);
    sDex.monSpriteMan = PokemonSpriteManager_New(heapID);
    // The small font the entry's name and number are in, which the battle loads
    Font_InitManager(FONT_SUBSCREEN, heapID);

    PokedexEntryScreenParams params;
    params.bgConfig = sDex.bgConfig;
    params.plttData = sDex.paletteData;
    params.monSpriteMan = sDex.monSpriteMan;
    params.heapID = heapID;
    params.mon = mon;
    params.hasNationalDex = IsNationalDexObtained(pokedex);
    sDex.entry = ov21_021E8D48(&params);
    sDex.state = DEX_ENTRY_SHOWING;
    // BW2's battle leaves both screens faded out by their master brightness; the entry fades in by its palettes
    GX_SetMasterBrightness(0);
    GXS_SetMasterBrightness(0);
    SetVBlankCallback(VBlank, NULL);
    printf("Gen 5: the Pokédex entry of species %u\n", (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL));
    return TRUE;
}

BOOL Gen5DexEntry_Run(void)
{
    switch (sDex.state) {
    case DEX_ENTRY_SHOWING:
        // Shown (faded in, its cry played): A or a touch closes it, as in the battle
        if (ov21_021E8DEC(sDex.entry)) {
            BOOL close = FALSE;
            if (gSystem.pressedKeys & PAD_BUTTON_A) {
                close = TRUE;
            } else if (TouchScreen_Tapped()) {
                Sound_PlayEffect(SE_CONFIRM_sseq_3);
                close = TRUE;
            }
            if (close) {
                PaletteData_StartFade(sDex.paletteData, PLTTBUF_MAIN_BG_F | PLTTBUF_MAIN_OBJ_F, 0xFFFF, 1, 0, 16, 0);
                PokemonSpriteManager_StartFadeAll(sDex.monSpriteMan, 0, 16, 0, 0);
                ov21_021E8E04(sDex.entry, FALSE);
                sDex.state = DEX_ENTRY_FADING_OUT;
            }
        }
        break;
    case DEX_ENTRY_FADING_OUT:
        if (PaletteData_GetSelectedBuffersMask(sDex.paletteData) == 0) {
            sDex.state = DEX_ENTRY_DONE;
        }
        break;
    default:
        break;
    }
    PokemonSpriteManager_DrawSprites(sDex.monSpriteMan);
    sub_0207C770();
    G3_RequestSwapBuffers(GX_SORTMODE_MANUAL, GX_BUFFERMODE_Z);
    SpriteSystem_UpdateTransfer();
    return sDex.state == DEX_ENTRY_DONE;
}

void Gen5DexEntry_Free(void)
{
    SetVBlankCallback(NULL, NULL);
    ov21_021E8DD0(sDex.entry);
    Font_Free(FONT_SUBSCREEN);
    PokemonSpriteManager_Free(sDex.monSpriteMan);
    SpriteSystem_Free(sDex.spriteSys);
    VramTransfer_Free();
    G3DPipelineBuffers_Free(sDex.pipelineBuffers);
    Heap_Free(sDex.bgConfig);
    PaletteData_FreeBuffer(sDex.paletteData, PLTTBUF_MAIN_BG);
    PaletteData_FreeBuffer(sDex.paletteData, PLTTBUF_SUB_BG);
    PaletteData_FreeBuffer(sDex.paletteData, PLTTBUF_MAIN_OBJ);
    PaletteData_FreeBuffer(sDex.paletteData, PLTTBUF_SUB_OBJ);
    PaletteData_Free(sDex.paletteData);
    GXLayers_DisableEngineALayers();
    GXLayers_DisableEngineBLayers();
    memset(&sDex, 0, sizeof(sDex));
}

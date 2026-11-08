// NitroSystem 2D files that libntrsystem's 64-bit port doesn't convert, for BW2's battle (Gen 5 battles)
//
// The files' banks hold 32-bit offsets where NitroSystem's structures have pointers, so on 64 bits they are rebuilt in
// the structures' own layout. libntrsystem does that for cells and animations (NCER, NANR, NMAR); BW2's Pokémon sprites
// (mcss.c) also read multi-cells (NMCR), which Platinum doesn't use.

#include "types.h"

#include <string.h>

#include "gfl/heap.h"
#include "nnsys/g2d.h"

// A multi-cell bank as the file has it
typedef struct {
    u16 numMultiCellData;
    u16 pad16;
    u32 multiCellDataArray; // From the bank
    u32 hierarchyDataArray; // From the bank
    u32 stringBank;
    u32 extendedData; // From the bank
} PackedMultiCellBank;

// A multi-cell as the file has it
typedef struct {
    u16 numNodes;
    u16 numCellAnim;
    u32 hierDataArray; // From the bank's hierarchy array
} PackedMultiCell;

// The multi-cell bank in *file as NitroSystem's structures, in one block of heapId that replaces the file (which is
// freed), so the caller's free of the file frees it; arc_util.c's patch calls it in place of
// NNS_G2dGetUnpackedMultiCellBank. The bank's extended data (user attributes) and string bank are left out: the battle
// doesn't read them
BOOL Gen5_UnpackMultiCellBank(void **file, NNSG2dMultiCellDataBank **bank, u32 heapId)
{
    NNSG2dBinaryBlockHeader *block =
        NNS_G2dFindBinaryBlock((NNSG2dBinaryFileHeader *)*file, NNS_G2D_BLKSIG_MULTICELLBANK);
    if (block == NULL) {
        *bank = NULL;
        return FALSE;
    }
    const PackedMultiCellBank *packed = (const PackedMultiCellBank *)(block + 1);
    const PackedMultiCell *cells = (const PackedMultiCell *)((const u8 *)packed + packed->multiCellDataArray);
    const u8 *hierarchy = (const u8 *)packed + packed->hierarchyDataArray;

    // The hierarchy array's size: up to the end of the last cell's nodes
    u32 hierarchySize = 0;
    for (u16 i = 0; i < packed->numMultiCellData; i++) {
        const u32 end = cells[i].hierDataArray + cells[i].numNodes * sizeof(NNSG2dMultiCellHierarchyData);
        if (end > hierarchySize) {
            hierarchySize = end;
        }
    }

    const u32 cellsOffset = sizeof(NNSG2dMultiCellDataBank);
    const u32 hierarchyOffset = cellsOffset + packed->numMultiCellData * sizeof(NNSG2dMultiCellData);
    u8 *out = GFL_HeapAllocate(heapId, hierarchyOffset + hierarchySize, TRUE, __FILE__, __LINE__);
    NNSG2dMultiCellDataBank *outBank = (NNSG2dMultiCellDataBank *)out;
    NNSG2dMultiCellData *outCells = (NNSG2dMultiCellData *)(out + cellsOffset);
    NNSG2dMultiCellHierarchyData *outHierarchy = (NNSG2dMultiCellHierarchyData *)(out + hierarchyOffset);

    memcpy(outHierarchy, hierarchy, hierarchySize);
    for (u16 i = 0; i < packed->numMultiCellData; i++) {
        outCells[i].numNodes = cells[i].numNodes;
        outCells[i].numCellAnim = cells[i].numCellAnim;
        outCells[i].pHierDataArray = (NNSG2dMultiCellHierarchyData *)((u8 *)outHierarchy + cells[i].hierDataArray);
    }
    outBank->numMultiCellData = packed->numMultiCellData;
    outBank->pad16 = 0;
    outBank->pMultiCellDataArray = outCells;
    outBank->pHierarchyDataArray = outHierarchy;
    outBank->pStringBank = NULL;
    outBank->pExtendedData = NULL;

    GFL_HeapFree(*file);
    *file = out;
    *bank = outBank;
    return TRUE;
}

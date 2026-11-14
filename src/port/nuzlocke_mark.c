// The alpha mark. See include/port/nuzlocke_mark.h
#include "port/nuzlocke_mark.h"

#include "port/nuzlocke_mode.h"

#include "charcode_convert.h"
#include "font.h"
#include "string_gf.h"

#define MARK_RED 3 // the font palette's red text color
#define MARK_BG  15

// r: red, w: an eye (the background color), .: left as it is
static const char *const sMark[NUZLOCKE_MARK_SIZE] = {
    ".......rr.......",
    "......rrrr......",
    "..r..rrrrrr..r..",
    "..rr.rrrrrr.rr..",
    "..rrrrrrrrrrrr..",
    ".rrrrrrrrrrrrrr.",
    ".rrrrrrrrrrrrrr.",
    ".rwrrrrrrrrrrwr.",
    ".rwwrrrrrrrrwwr.",
    ".rwwwrrrrrrwwwr.",
    ".rrwwwrrrrwwwrr.",
    ".rrwwrrrrrrwwrr.",
    "..rrrrrrrrrrrr..",
    "...rrrrrrrrrr...",
    ".....rrrrrr.....",
    ".......rr.......",
};

void NuzlockeMark_Draw(Window *window, u16 x, u16 y)
{
    for (int row = 0; row < NUZLOCKE_MARK_SIZE; row++) {
        // Runs of one color, as rectangles
        int col = 0;
        while (col < NUZLOCKE_MARK_SIZE) {
            char c = sMark[row][col];
            int end = col + 1;
            while (end < NUZLOCKE_MARK_SIZE && sMark[row][end] == c) {
                end++;
            }
            if (c != '.') {
                Window_FillRectWithColor(window, c == 'r' ? MARK_RED : MARK_BG, x + col, y + row, end - col, 1);
            }
            col = end;
        }
    }
}

static void PrintAscii(Window *window, const char *text, int x, BOOL rightAligned, u16 y, TextColor color)
{
    charcode_t chars[24];
    CharCode_FromAscii(text, chars, 24);
    String *string = String_Init(24, HEAP_ID_SYSTEM);
    String_CopyChars(string, chars);
    if (rightAligned) {
        x -= Font_CalcStringWidth(FONT_SYSTEM, string, Font_GetAttribute(FONT_SYSTEM, FONTATTR_LETTER_SPACING));
    }
    Text_AddPrinterWithParamsAndColor(window, FONT_SYSTEM, string, x, y, TEXT_SPEED_NO_TRANSFER, color, NULL);
    String_Free(string);
}

void NuzlockeMark_PrintLine(Window *window, int level, u16 markX, u16 labelX, u16 rightEdge, u16 y, TextColor color)
{
    NuzlockeMark_Draw(window, markX, y);
    PrintAscii(window, "NUZLOCKE", labelX, FALSE, y, color);
    PrintAscii(window, NuzlockeMode_LevelName(level), rightEdge, TRUE, y, color);
}

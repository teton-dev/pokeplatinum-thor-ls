#ifndef PORT_NUZLOCKE_MARK_H
#define PORT_NUZLOCKE_MARK_H

// The alpha mark a Nuzlocke save shows on the save screen and the main menu: a 16x16 pixel
// version of the owner's alpha icon, drawn with the font palette (red, its eyes in the
// window's background color).

#include "bg_window.h"
#include "text.h"

#define NUZLOCKE_MARK_SIZE 16

void NuzlockeMark_Draw(Window *window, u16 x, u16 y);

// A line of an info window: the mark at markX, "NUZLOCKE" at labelX, the level's name
// ending at rightEdge
void NuzlockeMark_PrintLine(Window *window, int level, u16 markX, u16 labelX, u16 rightEdge, u16 y, TextColor color);

#endif // PORT_NUZLOCKE_MARK_H

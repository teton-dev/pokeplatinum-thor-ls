#ifndef POKEPLATINUM_PORT_GEN5_SUMMARY_H
#define POKEPLATINUM_PORT_GEN5_SUMMARY_H

// Black 2 and White 2's Pokemon summary screen in place of Platinum's (teton-dev fork; docs/thor/dev/gen5-screens.md).
// Built with GEN5_BATTLES. Settings > Gameplay > Gen 5 screens, with a ROM imported.

#include <nitro/types.h>

#include "overlay_manager.h"

// Called by the summary screen's Init, Main and Exit (applications/pokemon_summary_screen/main.c): Platinum's own screen
// runs when the Gen 5 one isn't wanted or can't run the request
BOOL Gen5Summary_Begin(ApplicationManager *appMan); // TRUE: the Gen 5 screen runs; Main and Exit go to it
BOOL Gen5Summary_IsActive(void);
BOOL Gen5Summary_Main(ApplicationManager *appMan); // TRUE when the screen is done
void Gen5Summary_Exit(ApplicationManager *appMan);

#endif // POKEPLATINUM_PORT_GEN5_SUMMARY_H

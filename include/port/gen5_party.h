#ifndef POKEPLATINUM_PORT_GEN5_PARTY_H
#define POKEPLATINUM_PORT_GEN5_PARTY_H

// Black 2 and White 2's party list in place of Platinum's party menu, for the modes it can do (teton-dev fork;
// docs/thor/dev/gen5-screens.md). Built with GEN5_BATTLES; Settings > Gameplay > Gen 5 screens.

#include <nitro/types.h>

#include "overlay_manager.h"

BOOL Gen5Party_Begin(ApplicationManager *appMan); // TRUE: the Gen 5 list runs; Main and Exit go to it
BOOL Gen5Party_IsActive(void);
BOOL Gen5Party_Main(ApplicationManager *appMan); // TRUE when the list is done
void Gen5Party_Exit(ApplicationManager *appMan);

#endif // POKEPLATINUM_PORT_GEN5_PARTY_H

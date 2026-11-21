#include "port/soft_reset.h"

#include <simulator/sim.h>

#include "system.h"

static BOOL sRequested = FALSE;

BOOL SoftReset_Allowed(void)
{
    return gSystem.inhibitReset == 0;
}

void SoftReset_Request(void)
{
    sRequested = TRUE;
}

BOOL SoftReset_TakeRequest(void)
{
    BOOL requested = sRequested;
    sRequested = FALSE;
    if (requested) {
        SIM_SetBattleActive(FALSE); // a battle's speed doesn't carry over to the title screen
    }
    return requested;
}

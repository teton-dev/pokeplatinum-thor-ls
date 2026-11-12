#ifndef POKEPLATINUM_PORT_STUCK_REPORT_H
#define POKEPLATINUM_PORT_STUCK_REPORT_H

// When the player has been held on the overworld for a while (a script or field task that doesn't end), what holds
// them is logged every few seconds (stdout: logcat's "pokeplatinum" tag on Android) and shown in Settings > Advanced:
// the field tasks, the running script's contexts and what each waits on, the player and the follower

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Each frame, with whether the player is free on the overworld
void StuckReport_Update(BOOL free);
// The last report, or "" while the player is free
const char *StuckReport_Text(void);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_STUCK_REPORT_H

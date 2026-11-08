// BW2's network error handling for the port (Gen 5 battles), in place of pokebw2's src/system/net_err.c: the port's
// battles are local, so there is never a network error to report.

#include "types.h"

#include "gfl/net.h"

BOOL GFL_NetErrCheck(void) {
    return FALSE;
}

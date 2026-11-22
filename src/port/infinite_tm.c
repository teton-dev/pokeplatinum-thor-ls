#include "port/infinite_tm.h"

#include "constants/items.h"
#include "port/sim_config_prj.h"

BOOL InfiniteTM_Applies(u16 item)
{
    const SIM_Config_prj_type *config = SIM_Config_prj_GetConfig();
    return config != NULL && config->infiniteTMs && item >= ITEM_TM01 && item < ITEM_HM01;
}

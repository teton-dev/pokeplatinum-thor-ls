// Nuzlocke mode's books for the Gear: port-common's Nuzlocke API (port/nuzlocke.h, the retired helper's) asks for
// them through PortGame_NuzlockeModeRun and friends (subprojects/packagefiles/port-common_nuzlocke_mode.patch) and
// shows them as it showed the helper's: the area's encounter, the run's areas and its graveyard.
#include <nitro.h>
#include <string.h>

#include "port/nuzlocke.h"
#include "port/nuzlocke_mode.h"

// Rebuilt at each call (a few kilobytes, a few times a frame): the GUI thread reads it, the game thread keeps the
// mode's record
static NuzlockeRun sRun;

const NuzlockeRun *PortGame_NuzlockeModeRun(void)
{
    if (!NuzlockeMode_On()) {
        return NULL;
    }
    int areas = NuzlockeMode_AreaCount();
    areas = areas < NUZLOCKE_MAX_AREAS ? areas : NUZLOCKE_MAX_AREAS;
    sRun.areaCount = 0;
    for (int i = 0; i < areas; i++) {
        NuzlockeArea *area = &sRun.areas[sRun.areaCount];
        if (NuzlockeMode_AreaAt(i, &area->location, &area->species, &area->result)) {
            sRun.areaCount++;
        }
    }
    int deaths = NuzlockeMode_DeathCount();
    deaths = deaths < NUZLOCKE_MAX_DEATHS ? deaths : NUZLOCKE_MAX_DEATHS;
    sRun.deathCount = 0;
    for (int i = 0; i < deaths; i++) {
        NuzlockeModeDeath death;
        if (!NuzlockeMode_DeathAt(i, &death)) {
            continue;
        }
        NuzlockeDeath *out = &sRun.deaths[sRun.deathCount++];
        memset(out, 0, sizeof(*out));
        out->personality = death.personality;
        out->species = death.species;
        out->level = death.level;
        out->location = death.location;
        _Static_assert(sizeof(out->name) >= sizeof(death.name), "a death's name fits the Gear's");
        memcpy(out->name, death.name, sizeof(death.name));
    }
    return &sRun;
}

int PortGame_NuzlockeModeEncounter(void)
{
    switch (NuzlockeMode_CurrentEncounter()) {
    case NUZLOCKE_MODE_ENC_FIRST:
        return NUZLOCKE_ENC_FIRST;
    case NUZLOCKE_MODE_ENC_USED:
        return NUZLOCKE_ENC_USED;
    case NUZLOCKE_MODE_ENC_SHINY:
        return NUZLOCKE_ENC_SHINY;
    default:
        return NUZLOCKE_ENC_NONE; // a static encounter with Normal has no line of its own
    }
}

BOOL PortGame_NuzlockeModeCatchRule(void)
{
    return NuzlockeMode_CatchRuleStarted();
}

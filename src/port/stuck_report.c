// See include/port/stuck_report.h
#include "port/stuck_report.h"

#include <nitro.h>
#include <stdio.h>
#include <string.h>


#include "field/field_system.h"
#include "port/debug_field.h"
#include "port/following_hack.h"

#include "field_script_context.h"
#include "field_task.h"
#include "map_object.h"
#include "player_avatar.h"
#include "script_manager.h"

// unk_020655F4.c: the scripted movements under way
int MapObject_PortDescribeAnimations(char *out, int size);

// After this many frames on the overworld without the player being free, the state is logged every STUCK_REPORT_EVERY
#define STUCK_REPORT_AFTER (60 * 10)
#define STUCK_REPORT_EVERY (60 * 5)

static char sText[2048];
static int sBusyFrames;

// A function as its offset from StuckReport_Update, in the same library: `nm` on the unstripped build names it
// (StuckReport_Update's address plus the offset)
static void Describe(const void *function, char *out, size_t size)
{
    if (function == NULL) {
        snprintf(out, size, "none");
        return;
    }
    snprintf(out, size, "StuckReport_Update%+ld", (long)((const char *)function - (const char *)(const void *)StuckReport_Update));
}

static int Append(int used, const char *format, const char *a, long b, long c, long d)
{
    if (used < (int)sizeof(sText)) {
        used += snprintf(sText + used, sizeof(sText) - used, format, a, b, c, d);
    }
    return used;
}

static void Report(FieldSystem *fieldSystem)
{
    char name[96];
    int used = snprintf(sText, sizeof(sText), "Stuck report (%d s not free): ", sBusyFrames / 60);

    FieldTask *task = fieldSystem->task;
    if (task == NULL) {
        used = Append(used, "no field task%s; ", "", 0, 0, 0);
    }
    for (int depth = 0; task != NULL && depth < 4; depth++, task = task->prev) {
        Describe((const void *)task->func, name, sizeof(name));
        used = Append(used, "task %s state %ld; ", name, task->state, 0, 0);
    }

    // The script manager is the first field task's environment while a script runs
    if (fieldSystem->task != NULL) {
        ScriptManager *manager = FieldTask_GetEnv(fieldSystem->task);
        if (manager != NULL && manager->magic == SCRIPT_MANAGER_MAGIC_NUMBER) {
            used = Append(used, "script %s%ld, contexts %ld, movements %ld; ", "", manager->scriptID,
                manager->numActiveContexts, manager->movementCount);
            for (int i = 0; i < NUM_SCRIPT_CONTEXTS; i++) {
                ScriptContext *ctx = manager->ctx[i];
                if (ctx == NULL) {
                    continue;
                }
                Describe((const void *)ctx->shouldResume, name, sizeof(name));
                const long offset = ctx->scripts != NULL && ctx->scriptPtr != NULL ? (long)(ctx->scriptPtr - ctx->scripts) : -1;
                const long next = ctx->scriptPtr != NULL ? (long)(ctx->scriptPtr[0] | (ctx->scriptPtr[1] << 8)) : -1;
                used = Append(used, "ctx waits on %s at %ld, next command %ld, state %ld; ", name, offset, next, ctx->state);
            }
        }
    }

    if (used < (int)sizeof(sText)) {
        used += MapObject_PortDescribeAnimations(sText + used, sizeof(sText) - used);
    }

    if (fieldSystem->playerAvatar != NULL) {
        MapObject *player = PlayerAvatar_GetMapObject(fieldSystem->playerAvatar);
        used = Append(used, "player state %s%ld status %#lx move %ld; ", "", PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar),
            player != NULL ? (long)MapObject_GetStatus(player) : -1, player != NULL ? (long)MapObject_GetMovementType(player) : -1);
    }
    MapObject *follower = FollowingHack_ScriptObject(fieldSystem, 0xFD);
    if (follower != NULL) {
        used = Append(used, "follower status %s%#lx move %ld data %#lx", "", (long)MapObject_GetStatus(follower),
            (long)MapObject_GetMovementType(follower), (long)MapObject_GetDataAt(follower, 2));
    }
    printf("%s\n", sText);
}

void StuckReport_Update(BOOL free)
{
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (free || fieldSystem == NULL || !fieldSystem->runningFieldMap || fieldSystem->processManager == NULL
        || fieldSystem->processManager->child != NULL) {
        sBusyFrames = 0;
        sText[0] = '\0';
        return;
    }
    sBusyFrames++;
    if (sBusyFrames >= STUCK_REPORT_AFTER && (sBusyFrames - STUCK_REPORT_AFTER) % STUCK_REPORT_EVERY == 0) {
        Report(fieldSystem);
    }
}

const char *StuckReport_Text(void)
{
    return sText;
}

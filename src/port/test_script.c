// Scripted test runs; see include/port/test_script.h and tests/README.md
#include "port/test_script.h"

#include <nitro.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <simulator/sim.h>

#include "field/field_system.h"
#include "field_move_tasks.h"
#include "game_options.h"
#include "save_player.h"
#include "field_task.h"
#include "player_avatar.h"
#include "port/debug_field.h"
#include "port/frame_rate.h"
#include "port/port_game.h"
#include "gui_createmon_utils.h"
#include "gui_mapjump_utils.h"
#include "script_manager.h"
#include "applications/town_map/main.h"
#include "item_use_functions.h"
#include "unk_0203D1B8.h"
#include "enc_effects.h"
#include "overlay005/encounter_effect.h"
#include "bag.h"
#include "constants/heap.h"
#include "savedata.h"
#include "party.h"
#include "pc_boxes.h"
#include "applications/pc_boxes/pokemon_storage_session.h"
#include "pokemon.h"
#include "savedata/save_table.h"
#include "pokedex.h"
#include "port/sim_config_prj.h"
#ifdef GEN5_BATTLES
#include "port/gen5_battle.h"
#include "port/gen5_rom.h"
#endif

#define MAX_COMMANDS 1024
#define DEFAULT_TIMEOUT 3000 // Frames: 100 s on the overworld, where Docker's software 3D runs at about 30 a second
#define SHOT_PAUSE_MS 1500 // How long a shot keeps the game still, for the harness to take it
#define FREE_FRAMES 20 // The player counts as free once free this many frames in a row (events start a frame late)
#define WALK_TIMEOUT 600 // Frames a walk may push without reaching a new tile before it's taken for blocked

enum TestOp {
    OP_PRESS,
    OP_HOLD,
    OP_WAIT,
    OP_TOUCH,
    OP_WALK,
    OP_UNTIL,
    OP_MASH,
    OP_PACE,
    OP_SAVE,
    OP_GEN5_IMPORT,
    OP_GEN5_BATTLE,
    OP_GEN5_SETTING,
    OP_BATTLE_STYLE,
    OP_WILD,
    OP_JUMP,
    OP_FLY,
    OP_SCRIPT,
    OP_APP,
    OP_LEADER,
    OP_ITEM,
    OP_GIVE,
    OP_PARTY,
    OP_TRAINER,
    OP_SHOT,
    OP_LOG,
    OP_EXIT,
};

// What until and mash wait for: the events, the player free on the overworld, or a map
enum TestCondition {
    COND_FREE = TEST_EVENT_COUNT,
    COND_MAP,
    COND_NONE, // mash ... for N: just the frames
};

typedef struct TestCommand {
    enum TestOp op;
    int line;
    u16 keys;
    int frames;
    int x, y;
    int condition;
    BOOL touch; // mash touches (x, y) rather than pressing keys
    u16 keys2; // pace: the way back
    int map;
    int timeout;
    char text[96];
} TestCommand;

static struct {
    BOOL loaded;
    BOOL active;
    TestCommand commands[MAX_COMMANDS];
    int count;
    int current;
    u32 frame;
    u32 commandStart; // The frame the current command started
    Uint64 commandStartMs; // And when, in SDL_GetTicks64's milliseconds
    u32 lastWaitEnd; // The frame the last until or mash ... until was met: events count from there
    BOOL started; // The current command has been started
    BOOL failed;
    Uint64 pauseUntil; // SDL_GetTicks64's milliseconds
    BOOL pausing; // A shot's pause is on
    u32 presentMs; // Test mode's TestModePresentMs, while a pause shows every frame
    u32 eventFrames[TEST_EVENT_COUNT]; // The frame each event last happened, plus one
    u16 touchX, touchY;
    BOOL touching;
    int freeFrames; // Frames in a row the player has been free on the overworld
    int walkX, walkZ, walkMap; // Where a walk started
    int walkLastX, walkLastZ; // The tile the walk was last on
    int walkStuck; // Frames the walk has pushed, free, without reaching a new tile
    Uint64 walkStuckMs; // Since when
} sTest;

static const struct {
    const char *name;
    u16 mask;
} sKeyNames[] = {
    { "A", PAD_BUTTON_A },
    { "B", PAD_BUTTON_B },
    { "X", PAD_BUTTON_X },
    { "Y", PAD_BUTTON_Y },
    { "L", PAD_BUTTON_L },
    { "R", PAD_BUTTON_R },
    { "START", PAD_BUTTON_START },
    { "SELECT", PAD_BUTTON_SELECT },
    { "UP", PAD_KEY_UP },
    { "DOWN", PAD_KEY_DOWN },
    { "LEFT", PAD_KEY_LEFT },
    { "RIGHT", PAD_KEY_RIGHT },
};

static const char *sConditionNames[] = {
    [TEST_EVENT_MENU] = "menu",
    [TEST_EVENT_NAMING_SCREEN] = "naming",
    [TEST_EVENT_BATTLE] = "battle",
    [TEST_EVENT_BATTLE_COMMAND] = "command",
    [COND_FREE] = "free",
    [COND_MAP] = "map",
    [COND_NONE] = "end",
};

static void Fail(const TestCommand *command, const char *why)
{
    printf("[test] FAIL line %d: %s\n", command != NULL ? command->line : 0, why);
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    if (fieldSystem != NULL && fieldSystem->location != NULL && fieldSystem->playerAvatar != NULL) {
        printf("[test] the player is on map %d at (%d, %d)\n", (int)fieldSystem->location->mapHeaderID,
            PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar));
    }
    printf("[test] shot failure\n");
    fflush(stdout);
    sTest.failed = TRUE;
    sTest.pauseUntil = SDL_GetTicks64() + SHOT_PAUSE_MS + 1000;
}

static BOOL ParseKeys(const char *word, u16 *out)
{
    char copy[64];
    snprintf(copy, sizeof(copy), "%s", word);
    *out = 0;
    for (char *key = strtok(copy, "+"); key != NULL; key = strtok(NULL, "+")) {
        int i;
        for (i = 0; i < (int)NELEMS(sKeyNames); i++) {
            if (strcmp(key, sKeyNames[i].name) == 0) {
                *out |= sKeyNames[i].mask;
                break;
            }
        }
        if (i == (int)NELEMS(sKeyNames)) {
            return FALSE;
        }
    }
    return *out != 0;
}

// "until" CONDITION [ARG] [timeout N], from words[0]; returns FALSE if it can't be read
static BOOL ParseCondition(char **words, int count, TestCommand *command)
{
    if (count < 1) {
        return FALSE;
    }
    command->condition = -1;
    for (int i = 0; i < (int)NELEMS(sConditionNames); i++) {
        if (strcmp(words[0], sConditionNames[i]) == 0) {
            command->condition = i;
        }
    }
    int next = 1;
    if (command->condition == COND_MAP) {
        if (count < 2) {
            return FALSE;
        }
        command->map = atoi(words[1]);
        next = 2;
    }
    command->timeout = DEFAULT_TIMEOUT;
    if (next + 1 < count && strcmp(words[next], "timeout") == 0) {
        command->timeout = atoi(words[next + 1]);
    }
    return command->condition >= 0;
}

static BOOL ParseLine(char *text, int line, TestCommand *command)
{
    char *words[8];
    int count = 0;
    char *rest = NULL;

    memset(command, 0, sizeof(*command));
    command->line = line;

    // log and shot keep the rest of the line
    char *space = strchr(text, ' ');
    if (space != NULL) {
        rest = space + 1;
    }
    char copy[256];
    snprintf(copy, sizeof(copy), "%s", text);
    for (char *word = strtok(copy, " \t"); word != NULL && count < 8; word = strtok(NULL, " \t")) {
        words[count++] = word;
    }
    if (count == 0) {
        return FALSE;
    }

    if (strcmp(words[0], "press") == 0 && count >= 2) {
        command->op = OP_PRESS;
        command->frames = count >= 3 ? atoi(words[2]) : 2;
        return ParseKeys(words[1], &command->keys);
    } else if (strcmp(words[0], "hold") == 0 && count >= 3) {
        command->op = OP_HOLD;
        command->frames = atoi(words[2]);
        return ParseKeys(words[1], &command->keys);
    } else if (strcmp(words[0], "wait") == 0 && count >= 2) {
        command->op = OP_WAIT;
        command->frames = atoi(words[1]);
        return TRUE;
    } else if (strcmp(words[0], "touch") == 0 && count >= 3) {
        command->op = OP_TOUCH;
        command->x = atoi(words[1]);
        command->y = atoi(words[2]);
        command->frames = count >= 4 ? atoi(words[3]) : 3;
        return TRUE;
    } else if (strcmp(words[0], "walk") == 0 && count >= 2) {
        // walk UP|DOWN|LEFT|RIGHT [tiles]
        command->op = OP_WALK;
        command->frames = count >= 3 ? atoi(words[2]) : 1;
        return ParseKeys(words[1], &command->keys)
            && (command->keys & ~(PAD_KEY_UP | PAD_KEY_DOWN | PAD_KEY_LEFT | PAD_KEY_RIGHT)) == 0;
    } else if (strcmp(words[0], "until") == 0) {
        command->op = OP_UNTIL;
        return ParseCondition(words + 1, count - 1, command);
    } else if (strcmp(words[0], "mash") == 0 && count >= 3) {
        // mash [KEYS] [touch X Y], until CONDITION [timeout N] or for N frames
        command->op = OP_MASH;
        int next = 1;
        if (strcmp(words[next], "touch") != 0) {
            if (!ParseKeys(words[next], &command->keys)) {
                return FALSE;
            }
            next++;
        }
        if (next + 2 < count && strcmp(words[next], "touch") == 0) {
            command->touch = TRUE;
            command->x = atoi(words[next + 1]);
            command->y = atoi(words[next + 2]);
            next += 3;
        }
        if (next >= count) {
            return FALSE;
        }
        if (strcmp(words[next], "for") == 0 && next + 1 < count) {
            command->condition = COND_NONE;
            command->timeout = atoi(words[next + 1]);
            return TRUE;
        }
        return strcmp(words[next], "until") == 0 && ParseCondition(words + next + 1, count - next - 1, command);
    } else if (strcmp(words[0], "pace") == 0 && count >= 5) {
        // pace DIR DIR until CONDITION [timeout N]: walks one way and back (two tiles each) while the player is free,
        // as in tall grass until a wild Pokémon appears
        command->op = OP_PACE;
        return ParseKeys(words[1], &command->keys) && ParseKeys(words[2], &command->keys2)
            && strcmp(words[3], "until") == 0 && ParseCondition(words + 4, count - 4, command);
    } else if ((strcmp(words[0], "item") == 0 || strcmp(words[0], "give") == 0) && count >= 3) {
        // item ITEM COUNT: into the bag; give SPECIES LEVEL: a new Pokémon into the party (port/gui_createmon_utils)
        command->op = strcmp(words[0], "item") == 0 ? OP_ITEM : OP_GIVE;
        command->x = atoi(words[1]);
        command->y = atoi(words[2]);
        return TRUE;
    } else if (strcmp(words[0], "trainer") == 0 && count >= 2) {
        // A battle against trainer ID (Platinum's trainer numbers), as its encounter starts it, once the player is free
        command->op = OP_TRAINER;
        command->x = atoi(words[1]);
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "party") == 0) {
        // Logs the party (species, level, ball, OT ID, HP) and the Pokémon in the PC, for the checks
        command->op = OP_PARTY;
        return TRUE;
    } else if (strcmp(words[0], "script") == 0 && count >= 2) {
        // script ID: runs the field script ID (2018: a Pokemon Center's PC), once the player is free
        command->op = OP_SCRIPT;
        command->x = atoi(words[1]);
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "leader") == 0 && count >= 2) {
        // leader N: the Gym Leader encounter effect N (0: Roark, 1: Gardenia...) plays, once the player is free
        command->op = OP_LEADER;
        command->x = atoi(words[1]);
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "app") == 0 && count >= 2) {
        // app bag|journal|townmap|flymap|party: opens that application over the overworld, once the player is free (leave it
        // with B, as the player would)
        command->op = OP_APP;
        command->x = strcmp(words[1], "bag") == 0 ? 1 : strcmp(words[1], "journal") == 0 ? 2
            : strcmp(words[1], "townmap") == 0 ? 3 : strcmp(words[1], "flymap") == 0 ? 4 : strcmp(words[1], "party") == 0 ? 5 : strcmp(words[1], "summary") == 0 ? 6 : strcmp(words[1], "pc") == 0 ? 7 : strcmp(words[1], "party-field") == 0 ? 8 : 0;
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return command->x != 0;
    } else if (strcmp(words[0], "jump") == 0 && count >= 4) {
        // jump MAP X Z: to map header MAP at (X, Z), as Settings' map jump does, once the player is free
        command->op = OP_JUMP;
        command->map = atoi(words[1]);
        command->x = atoi(words[2]);
        command->y = atoi(words[3]);
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "fly") == 0 && count >= 4) {
        // fly MAP X Z [SLOT]: Fly as the town map's choice starts it (the HM cut-in with party member SLOT, default the first, then the
        // landing), to the fly destination at map header MAP's tile (X, Z), once the player is free
        command->op = OP_FLY;
        command->map = atoi(words[1]);
        command->x = atoi(words[2]);
        command->y = atoi(words[3]);
        command->frames = count >= 5 ? atoi(words[4]) : 0;
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "wild") == 0 && count >= 3) {
        // A wild SPECIES at LEVEL, met as in tall grass (the field's own wild encounter), once the player is free
        command->op = OP_WILD;
        command->x = atoi(words[1]);
        command->y = atoi(words[2]);
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "gen5-screens") == 0 && count >= 2) {
        // gen5-screens on|off: Settings > Gameplay > Gen 5 screens
        command->op = OP_GEN5_SETTING;
        command->x = 1;
        command->frames = strcmp(words[1], "on") == 0;
        return strcmp(words[1], "on") == 0 || strcmp(words[1], "off") == 0;
    } else if (strcmp(words[0], "gen5-battles") == 0 && count >= 2) {
        // gen5-battles on|off: Settings > Gameplay > Gen 5 battles
        command->op = OP_GEN5_SETTING;
        command->frames = strcmp(words[1], "on") == 0;
        return strcmp(words[1], "on") == 0 || strcmp(words[1], "off") == 0;
    } else if (strcmp(words[0], "battle-style") == 0 && count >= 2) {
        // battle-style set|shift: the save's Battle Style option (Settings > Options in the game)
        command->op = OP_BATTLE_STYLE;
        command->frames = strcmp(words[1], "set") == 0;
        return strcmp(words[1], "set") == 0 || strcmp(words[1], "shift") == 0;
    } else if (strcmp(words[0], "gen5-import") == 0) {
        // Imports the Black 2 / White 2 ROM in POKEPLATINUM_GEN5_ROM, as the settings do, and waits for it
        command->op = OP_GEN5_IMPORT;
        command->timeout = DEFAULT_TIMEOUT * 20;
        return TRUE;
    } else if (strcmp(words[0], "gen5-battle") == 0 && count >= 3) {
        // A debug wild battle against SPECIES at LEVEL in the Gen 5 engine, once the player is free
        command->op = OP_GEN5_BATTLE;
        command->x = atoi(words[1]);
        command->y = atoi(words[2]);
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "save") == 0) {
        command->op = OP_SAVE;
        command->condition = COND_FREE;
        command->timeout = DEFAULT_TIMEOUT;
        return TRUE;
    } else if (strcmp(words[0], "shot") == 0 && rest != NULL) {
        command->op = OP_SHOT;
        snprintf(command->text, sizeof(command->text), "%s", rest);
        return TRUE;
    } else if (strcmp(words[0], "log") == 0 && rest != NULL) {
        command->op = OP_LOG;
        snprintf(command->text, sizeof(command->text), "%s", rest);
        return TRUE;
    } else if (strcmp(words[0], "exit") == 0) {
        command->op = OP_EXIT;
        command->frames = count >= 2 ? atoi(words[1]) : 0;
        return TRUE;
    }
    return FALSE;
}


static void Load(void)
{
    sTest.loaded = TRUE;
    const char *path = getenv("POKEPLATINUM_TEST_SCRIPT");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("[test] FAIL can't open %s\n", path);
        TestScript_ExitExpected();
        exit(1);
    }

    char line[256];
    int number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        number++;
        line[strcspn(line, "\r\n")] = '\0';
        char *text = line + strspn(line, " \t");
        if (text[0] == '\0' || text[0] == '#') {
            continue;
        }
        if (sTest.count == MAX_COMMANDS || !ParseLine(text, number, &sTest.commands[sTest.count])) {
            printf("[test] FAIL %s:%d: can't read \"%s\"\n", path, number, text);
            TestScript_ExitExpected();
            exit(1);
        }
        sTest.count++;
    }
    fclose(file);
    sTest.active = TRUE;
#if defined(SDK_BUILD_LINUX) && !defined(SDK_BUILD_ANDROID)
    TestScript_InstallCrashHandlers();
#endif
    printf("[test] running %s (%d commands)\n", path, sTest.count);
    fflush(stdout);
}

static BOOL ConditionMet(const TestCommand *command)
{
    if (command->condition < TEST_EVENT_COUNT) {
        // An event counts once: since the last wait for something ended
        return sTest.eventFrames[command->condition] > sTest.lastWaitEnd;
    }
    if (command->condition == COND_FREE) {
        return sTest.freeFrames >= FREE_FRAMES;
    }
    FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
    return fieldSystem != NULL && fieldSystem->location != NULL
        && fieldSystem->location->mapHeaderID == command->map && sTest.freeFrames >= FREE_FRAMES;
}

// A shot's pause: the game runs at about its real speed and shows every frame, so that the window has the frame the
// script is at when the harness takes it (test mode shows one every TestModePresentMs and runs thousands between)
// A wait times out after its frames, and as long in real time as they take at the DS's 30 a second: scenes that wait
// for music or a sound effect to end wait in real time (libntr plays them from its audio callback), while test mode
// runs frames as fast as it can
static BOOL TimedOut(int frames, int limit, Uint64 sinceMs)
{
    return frames >= limit && SDL_GetTicks64() - sinceMs >= (Uint64)limit * 1000 / 30;
}

static BOOL Paused(void)
{
    SIM_config_type *config = SIM_GetConfigPtr();
    if (SDL_GetTicks64() < sTest.pauseUntil) {
        if (!sTest.pausing) {
            sTest.pausing = TRUE;
            sTest.presentMs = config->testModePresentMs;
            config->testModePresentMs = 1;
        }
        SDL_Delay(16);
        return TRUE;
    }
    if (sTest.pausing) {
        sTest.pausing = FALSE;
        config->testModePresentMs = sTest.presentMs;
    }
    return FALSE;
}

static void Next(void)
{
    sTest.current++;
    sTest.started = FALSE;
}

// Runs the script for this frame: the keys to hold
static u16 Step(void)
{
    sTest.touching = FALSE;
    if (sTest.failed) {
        if (!Paused()) {
            TestScript_ExitExpected();
            exit(1);
        }
        return 0;
    }
    if (Paused()) {
        return 0;
    }

    if (FrameRate_PlayerIsFreeOnOverworld()) {
        if (++sTest.freeFrames == FREE_FRAMES) {
            FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
            printf("[test] free on map %d at (%d, %d)\n", (int)fieldSystem->location->mapHeaderID,
                PlayerAvatar_GetXPos(fieldSystem->playerAvatar), PlayerAvatar_GetZPos(fieldSystem->playerAvatar));
        }
    } else {
        sTest.freeFrames = 0;
    }

    while (sTest.current < sTest.count) {
        TestCommand *command = &sTest.commands[sTest.current];
        if (!sTest.started) {
            sTest.started = TRUE;
            sTest.commandStart = sTest.frame;
            sTest.commandStartMs = SDL_GetTicks64();
            if (command->op != OP_LOG && command->op != OP_SHOT) {
                printf("[test] %d (frame %u)\n", command->line, (unsigned)sTest.frame);
            }
        }
        const int elapsed = (int)(sTest.frame - sTest.commandStart);

        switch (command->op) {
        case OP_PRESS:
            // Held for its frames, then let go for two, so that the next press is a new one
            if (elapsed < command->frames) {
                return command->keys;
            }
            if (elapsed < command->frames + 2) {
                return 0;
            }
            Next();
            continue;
        case OP_HOLD:
            if (elapsed < command->frames) {
                return command->keys;
            }
            Next();
            continue;
        case OP_WAIT:
            if (elapsed < command->frames) {
                return 0;
            }
            Next();
            continue;
        case OP_TOUCH:
            if (elapsed < command->frames) {
                sTest.touching = TRUE;
                sTest.touchX = (u16)command->x;
                sTest.touchY = (u16)command->y;
                return 0;
            }
            if (elapsed < command->frames + 3) {
                return 0;
            }
            Next();
            continue;
        case OP_WALK: {
            // From where the player is free: held until it has moved the tiles, or a door or warp changed the map
            FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
            if (elapsed == 0 || sTest.walkMap < 0) {
                if (sTest.freeFrames < FREE_FRAMES) {
                    // A message in the way (a tip after a step, someone talking): A until the player is free
                    if (TimedOut(elapsed, DEFAULT_TIMEOUT, sTest.commandStartMs)) {
                        Fail(command, "the player was never free to walk");
                    }
                    sTest.walkMap = -1;
                    return elapsed % 8 < 2 ? PAD_BUTTON_A : 0;
                }
                sTest.commandStart = sTest.frame; // The walk's own timeout starts once it walks
                sTest.commandStartMs = SDL_GetTicks64();
                sTest.walkMap = fieldSystem->location->mapHeaderID;
                sTest.walkX = sTest.walkLastX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
                sTest.walkZ = sTest.walkLastZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
                sTest.walkStuck = 0;
                sTest.walkStuckMs = SDL_GetTicks64();
            }
            const int x = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
            const int z = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
            const int moved = abs(x - sTest.walkX) + abs(z - sTest.walkZ);
            if (moved >= command->frames || (int)fieldSystem->location->mapHeaderID != sTest.walkMap) {
                // The step may start an event a frame later: free counts again from here
                sTest.freeFrames = 0;
                Next();
                return 0;
            }
            if (!FrameRate_PlayerIsFreeOnOverworld()) {
                // A scene the step started (someone stops the player...): A through it, then on
                return elapsed % 8 < 2 ? PAD_BUTTON_A : 0;
            }
            if (x != sTest.walkLastX || z != sTest.walkLastZ) {
                sTest.walkLastX = x;
                sTest.walkLastZ = z;
                sTest.walkStuck = 0;
                sTest.walkStuckMs = SDL_GetTicks64();
            } else if (TimedOut(++sTest.walkStuck, WALK_TIMEOUT, sTest.walkStuckMs)) {
                Fail(command, "the walk is blocked");
                return 0;
            }
            return command->keys;
        }
        case OP_UNTIL:
        case OP_MASH:
        case OP_PACE:
            if (command->condition == COND_NONE && elapsed >= command->timeout) {
                Next();
                continue;
            }
            if (command->condition != COND_NONE && ConditionMet(command)) {
                printf("[test] %s after %d frames\n", sConditionNames[command->condition], elapsed);
                sTest.lastWaitEnd = sTest.frame;
                Next();
                continue;
            }
            if (command->condition != COND_NONE && TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                char why[64];
                snprintf(why, sizeof(why), "no %s in %d frames", sConditionNames[command->condition], elapsed);
                Fail(command, why);
                return 0;
            }
            if (command->op == OP_PACE) {
                // About two tiles each way (a step takes 8 frames)
                return FrameRate_PlayerIsFreeOnOverworld() ? ((elapsed / 16) % 2 ? command->keys2 : command->keys) : 0;
            }
            // Mashing: pressed (or touched) for 2 frames out of 8. Waiting for the player to be free, only while
            // not: pressed on a free frame, A would talk to whatever is in front and start it all over again
            const BOOL waitsForFree = command->condition == COND_FREE || command->condition == COND_MAP;
            if (command->op == OP_MASH && elapsed % 8 < 2 && !(waitsForFree && FrameRate_PlayerIsFreeOnOverworld())) {
                if (command->touch) {
                    sTest.touching = TRUE;
                    sTest.touchX = (u16)command->x;
                    sTest.touchY = (u16)command->y;
                }
                return command->keys;
            }
            return 0;
        case OP_SAVE:
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free to save");
                }
                return 0;
            }
            if (!PortGame_Field_Save()) {
                Fail(command, "the save failed");
                return 0;
            }
            printf("[test] saved\n");
            Next();
            continue;
        case OP_GEN5_IMPORT: {
#ifdef GEN5_BATTLES
            if (elapsed == 0) {
                const char *rom = getenv("POKEPLATINUM_GEN5_ROM");
                if (rom == NULL || rom[0] == '\0') {
                    Fail(command, "no POKEPLATINUM_GEN5_ROM to import");
                    return 0;
                }
                Gen5Rom_Import(rom, FALSE);
                return 0;
            }
            Gen5Rom_Update();
            const enum Gen5RomState state = Gen5Rom_State();
            if (state == GEN5_ROM_IMPORTING) {
                if (elapsed % 300 == 0) {
                    printf("[test] %s\n", Gen5Rom_Status());
                }
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the import is taking too long");
                }
                return 0;
            }
            printf("[test] gen5: %s\n", Gen5Rom_Status());
            if (state != GEN5_ROM_READY) {
                Fail(command, "the import failed");
                return 0;
            }
            Next();
            continue;
#else
            Fail(command, "built without the Gen 5 code (-Dpokebw2)");
            return 0;
#endif
        }
        case OP_GEN5_BATTLE:
#ifdef GEN5_BATTLES
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the battle");
                }
                return 0;
            }
            if (!Gen5Battle_StartDebug(command->x, command->y)) {
                Fail(command, "the Gen 5 battle couldn't start (no BW2 ROM imported?)");
                return 0;
            }
            sTest.freeFrames = 0;
            Next();
            return 0;
#else
            Fail(command, "built without the Gen 5 code (-Dpokebw2)");
            return 0;
#endif
        case OP_SCRIPT:
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the script");
                }
                return 0;
            }
            ScriptManager_Set(DEBUG_GetFieldSystem(), command->x, NULL);
            sTest.freeFrames = 0;
            Next();
            return 0;
        case OP_LEADER: {
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the encounter effect");
                }
                return 0;
            }
            static BOOL sLeaderDone;
            EncounterEffect_Start(ENCEFF_CUTIN_LEADER_ROARK + command->x, DEBUG_GetFieldSystem(), &sLeaderDone);
            Next();
            return 0;
        }
        case OP_APP: {
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the application");
                }
                return 0;
            }
            FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
            static ItemUseContext sItemUseCtx;
            if (command->x == 1) {
                FieldSystem_OpenBag(fieldSystem, &sItemUseCtx);
            } else if (command->x == 2) {
                sub_0203D30C(fieldSystem, NULL);
            } else if (command->x == 8) {
                static FieldMoveContext sMoveContext;
                FieldSystem_OpenPartyMenu(fieldSystem, &sMoveContext, 0);
            } else if (command->x == 7) {
                static PokemonStorageSession sPcSession;
                sPcSession.saveData = fieldSystem->saveData;
                sPcSession.boxMode = 0;
                FieldSystem_OpenPokemonStorage(fieldSystem, &sPcSession);
            } else if (command->x == 6) {
                FieldSystem_GetPartyMenuMonSummary(0, fieldSystem, 0);
            } else if (command->x == 5) {
                FieldSystem_OpenPartyMenu_SelectPokemon(0, fieldSystem);
            } else {
                TownMapContext *townMapCtx = Heap_Alloc(HEAP_ID_FIELD2, sizeof(TownMapContext));
                TownMapContext_Init(fieldSystem, townMapCtx, command->x == 3 ? TOWN_MAP_MODE_ITEM : TOWN_MAP_MODE_FLY);
                FieldSystem_OpenTownMap(fieldSystem, townMapCtx);
            }
            sTest.freeFrames = 0;
            Next();
            return 0;
        }
        case OP_JUMP:
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the jump");
                }
                return 0;
            }
            GUI_MapJump_JumpToMap(command->map, command->x, command->y);
            sTest.freeFrames = 0;
            Next();
            return 0;
        case OP_FLY:
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for Fly");
                }
                return 0;
            }
            {
                FieldSystem *fieldSystem = DEBUG_GetFieldSystem();
                FlyContext *flyCtx = FlyContext_New(HEAP_ID_FIELD2, fieldSystem,
                    Party_GetPokemonBySlotIndex(SaveData_GetParty(SaveData_Ptr()), command->frames), command->map, command->x,
                    command->y);
                FieldSystem_CreateTask(fieldSystem, FieldMoves_FlyTask, flyCtx);
            }
            sTest.freeFrames = 0;
            Next();
            return 0;
        case OP_WILD:
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the encounter");
                }
                return 0;
            }
            if (!GUI_CreateMon_StartWildEncounter(command->x, command->y)) {
                Fail(command, "the wild encounter couldn't start");
                return 0;
            }
            sTest.freeFrames = 0;
            Next();
            return 0;
        case OP_TRAINER:
            if (!ConditionMet(command)) {
                if (TimedOut(elapsed, command->timeout, sTest.commandStartMs)) {
                    Fail(command, "the player was never free for the battle");
                }
                return 0;
            }
            if (!GUI_CreateMon_StartBattle(0, 0, command->x, FALSE)) {
                Fail(command, "the trainer battle couldn't start");
                return 0;
            }
            sTest.freeFrames = 0;
            Next();
            return 0;
        case OP_ITEM:
            if (!Bag_TryAddItem(SaveData_GetBag(SaveData_Ptr()), command->x, command->y, HEAP_ID_APPLICATION)) {
                Fail(command, "no room in the bag");
                return 0;
            }
            printf("[test] item %d x%d\n", command->x, command->y);
            Next();
            continue;
        case OP_GIVE:
            if (!GUI_CreateMon_GenPokemon(command->x, command->y)) {
                Fail(command, "the party is full");
                return 0;
            }
            printf("[test] gave species %d at level %d\n", command->x, command->y);
            Next();
            continue;
        case OP_PARTY: {
            SaveData *save = SaveData_Ptr();
            Party *party = SaveData_GetParty(save);
            for (int i = 0; i < Party_GetCurrentCount(party); i++) {
                Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);
                printf("[test] party %d: species %u level %u ball %u OT %u HP %u/%u nickname %u\n", i,
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL),
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL),
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_POKEBALL, NULL),
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_OT_ID, NULL),
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_HP, NULL),
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_MAX_HP, NULL),
                    (unsigned)Pokemon_GetValue(mon, MON_DATA_HAS_NICKNAME, NULL));
            }
            Bag *bag = SaveData_GetBag(save);
            printf("[test] bag: %u Master Balls, %u Poke Balls\n",
                (unsigned)Bag_GetItemQuantity(bag, 1, HEAP_ID_APPLICATION), (unsigned)Bag_GetItemQuantity(bag, 4,
                HEAP_ID_APPLICATION));
            PCBoxes *boxes = SaveData_GetPCBoxes(save);
            for (u32 box = 0; box < MAX_PC_BOXES; box++) {
                for (u32 slot = 0; slot < MAX_MONS_PER_BOX; slot++) {
                    BoxPokemon *boxMon = PCBoxes_GetBoxMonAt(boxes, box, slot);
                    if (BoxPokemon_GetValue(boxMon, MON_DATA_SPECIES_EXISTS, NULL)) {
                        printf("[test] box %u slot %u: species %u ball %u OT %u\n", box + 1, slot,
                            (unsigned)BoxPokemon_GetValue(boxMon, MON_DATA_SPECIES, NULL),
                            (unsigned)BoxPokemon_GetValue(boxMon, MON_DATA_POKEBALL, NULL),
                            (unsigned)BoxPokemon_GetValue(boxMon, MON_DATA_OT_ID, NULL));
                    }
                }
            }
            // Caught in the Pokédex, for each species in the party and boxes (logged above)
            Pokedex *pokedex = SaveData_GetPokedex(save);
            for (int i = 0; i < Party_GetCurrentCount(party); i++) {
                const u16 species = Pokemon_GetValue(Party_GetPokemonBySlotIndex(party, i), MON_DATA_SPECIES, NULL);
                printf("[test] Pokedex: species %u %s\n", species,
                    Pokedex_HasCaughtSpecies(pokedex, species) ? "caught" : "not caught");
            }
            Next();
            continue;
        }
        case OP_GEN5_SETTING:
            if (command->x == 1) {
                SIM_Config_prj_GetConfig()->gen5Screens = command->frames;
                printf("[test] Gen 5 screens %s\n", command->frames ? "on" : "off");
                Next();
                continue;
            }
            SIM_Config_prj_GetConfig()->gen5Battles = command->frames;
            printf("[test] Gen 5 battles %s\n", command->frames ? "on" : "off");
            Next();
            continue;
        case OP_BATTLE_STYLE:
            Options_SetBattleStyle(SaveData_GetOptions(SaveData_Ptr()),
                command->frames ? OPTIONS_BATTLE_STYLE_SET : OPTIONS_BATTLE_STYLE_SHIFT);
            printf("[test] battle style %s\n", command->frames ? "set" : "shift");
            Next();
            continue;
        case OP_SHOT:
            // The harness takes the screenshot while the game runs on without input
            printf("[test] shot %s\n", command->text);
            fflush(stdout);
            sTest.pauseUntil = SDL_GetTicks64() + SHOT_PAUSE_MS;
            Next();
            return 0;
        case OP_LOG:
            printf("[test] %s\n", command->text);
            Next();
            continue;
        case OP_EXIT:
            printf("[test] %s\n", command->frames == 0 ? "PASS" : "exit");
            fflush(stdout);
            TestScript_ExitExpected();
            exit(command->frames);
        }
    }

    printf("[test] PASS (end of script)\n");
    fflush(stdout);
    TestScript_ExitExpected();
    exit(0);
}

u16 TestScript_Keys(void)
{
    if (!sTest.loaded) {
        Load();
    }
    if (!sTest.active) {
        return 0;
    }
    sTest.frame++;
    if (sTest.frame % 120 == 0 && sTest.current < sTest.count) {
        printf("[test] frame %u at %.1f s: line %d, free for %d frames\n", (unsigned)sTest.frame,
            SDL_GetTicks64() / 1000.0, sTest.commands[sTest.current].line, sTest.freeFrames);
    }
    const u16 keys = Step();
    fflush(stdout);
    return keys;
}

BOOL TestScript_Touch(u16 *x, u16 *y)
{
    if (!sTest.active || !sTest.touching) {
        return FALSE;
    }
    *x = sTest.touchX;
    *y = sTest.touchY;
    return TRUE;
}

void TestScript_OnEvent(enum TestScriptEvent event)
{
    if (!sTest.active) {
        return;
    }
    sTest.eventFrames[event] = sTest.frame + 1;
    printf("[test] event %s\n", sConditionNames[event]);
}

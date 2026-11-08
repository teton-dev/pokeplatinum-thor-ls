#ifndef POKEPLATINUM_PORT_TEST_SCRIPT_H
#define POKEPLATINUM_PORT_TEST_SCRIPT_H

// Scripted test runs (teton-dev fork): with POKEPLATINUM_TEST_SCRIPT=<file> in the environment, the game plays the
// file's keys and touches, frame by frame, and can wait for what the game does (the player free on the overworld, a
// naming screen, a battle asking for a command). It logs "[test] ..." lines and quits with 0 when the script says
// so, or 1 when a wait times out. tests/README.md has the commands; tools/docker/desktop.sh test runs the scripts.
//
// Without the variable none of this does anything.

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// What the scripts can wait for, besides the player being free on the overworld
enum TestScriptEvent {
    TEST_EVENT_MENU, // A menu or list opened (Menu_New, ListMenu_New): choices, yes/no...
    TEST_EVENT_NAMING_SCREEN, // A naming screen opened
    TEST_EVENT_BATTLE, // A battle started
    TEST_EVENT_BATTLE_COMMAND, // A battle asks the player what to do
    TEST_EVENT_COUNT,
};

// Once per frame, before the keys are read: the keys the script holds down this frame
u16 TestScript_Keys(void);

// The script's touch this frame: TRUE with its point, in the touch screen's pixels
BOOL TestScript_Touch(u16 *x, u16 *y);

// The game reached one of the events
void TestScript_OnEvent(enum TestScriptEvent event);

#ifdef __cplusplus
}
#endif

#endif // POKEPLATINUM_PORT_TEST_SCRIPT_H

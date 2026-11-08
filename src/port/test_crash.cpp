// Backtraces for scripted test runs on desktop Linux (include/port/test_script.h): gdb can't run in the Docker
// container under Rosetta, so a crash, or a run the harness stops at its time limit, prints the game thread's
// backtrace as addresses from the executable's start, which tools/docker/desktop.sh turns into functions and lines.
// C++ for POSIX's signal API: the game's C files build as C99.

#include "port/test_script.h"

#if defined(SDK_BUILD_LINUX) && !defined(SDK_BUILD_ANDROID)

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <execinfo.h>
#include <pthread.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

// Where the linker put the executable's start (GNU ld's symbol)
extern "C" char __executable_start;

// The game's thread, which a hang's backtrace is of
static pthread_t sGameThread;

static void PrintBacktrace(const char *what)
{
    char line[96];
    const uintptr_t base = (uintptr_t)&__executable_start;
    int length = snprintf(line, sizeof(line), "[test] %s, base %#lx\n", what, (unsigned long)base);
    write(STDOUT_FILENO, line, length);
    void *frames[64];
    const int count = backtrace(frames, 64);
    for (int i = 0; i < count; i++) {
        length = snprintf(line, sizeof(line), "[test] frame %#lx\n", (unsigned long)((uintptr_t)frames[i] - base));
        write(STDOUT_FILENO, line, length);
    }
}

static void OnCrash(int number, siginfo_t *info, void *context)
{
    char what[32];
    snprintf(what, sizeof(what), "CRASH signal %d", number);
    PrintBacktrace(what);
#if defined(__x86_64__)
    // Where it crashed, and the return address on top of the stack: what's left of the backtrace when the crash is a
    // call to a bad address, which the unwinder can't get past
    const ucontext_t *ucontext = (const ucontext_t *)context;
    const uintptr_t base = (uintptr_t)&__executable_start;
    const uintptr_t pc = ucontext->uc_mcontext.gregs[REG_RIP];
    const uintptr_t top = *(const uintptr_t *)ucontext->uc_mcontext.gregs[REG_RSP];
    char line[128];
    int length = snprintf(line, sizeof(line), "[test] crashed at %#lx (address %p); the stack's top %#lx\n",
                          (unsigned long)pc, info->si_addr, (unsigned long)top);
    write(STDOUT_FILENO, line, length);
    length = snprintf(line, sizeof(line), "[test] frame %#lx\n[test] frame %#lx\n", (unsigned long)(pc - base),
                      (unsigned long)(top - base));
    write(STDOUT_FILENO, line, length);
#else
    (void)info;
    (void)context;
#endif
    _exit(1);
}

// One sample of where the game is: the harness takes a few, then stops the run
static void OnHang(int number)
{
    if (!pthread_equal(pthread_self(), sGameThread)) {
        pthread_kill(sGameThread, number);
        return;
    }
    PrintBacktrace("HANG");
}

static bool sExitExpected;

void TestScript_ExitExpected(void)
{
    sExitExpected = true;
}

// Who quits: a run that ends without the script saying so shows where from
static void OnExit(void)
{
    if (!sExitExpected) {
        PrintBacktrace("EXIT");
    }
}

void TestScript_InstallCrashHandlers(void)
{
    sGameThread = pthread_self();
    atexit(OnExit);
    // On a stack of their own, so that a stack overflow's backtrace can be taken
    static char signalStack[1 << 16];
    stack_t stack = {};
    stack.ss_sp = signalStack;
    stack.ss_size = sizeof(signalStack);
    sigaltstack(&stack, nullptr);

    struct sigaction action = {};
    action.sa_flags = SA_ONSTACK | SA_RESTART;
    action.sa_handler = OnHang;
    sigaction(SIGQUIT, &action, nullptr);
    action.sa_flags |= SA_SIGINFO;
    action.sa_sigaction = OnCrash;
    const int crashes[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
    for (int number : crashes) {
        sigaction(number, &action, nullptr);
    }
}

#else

void TestScript_ExitExpected(void)
{
}

#endif

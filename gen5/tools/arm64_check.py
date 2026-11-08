#!/usr/bin/env python3
"""Compiles the Gen 5 battle code for arm64 with the Android build's SDK defines, from the Linux build's compile
commands, to find what the Android build would trip on without the NDK (the desktop image has
aarch64-linux-gnu-gcc and arm64 SDL headers). Prints each file's errors, and BW2's warnings about pointers and
integers of different sizes, which 64 bits can break; exits 1 on errors.

    arm64_check.py BUILD_DIR      (build_linux, after building gen5/libgen5_battle.a)"""
import concurrent.futures
import json
import os
import shlex
import subprocess
import sys

build = os.path.abspath(sys.argv[1])
commands = [c for c in json.load(open(os.path.join(build, "compile_commands.json")))
            if "libgen5_battle.a.p" in c["output"]]
# The desktop build's x86 defines, and Android's (subprojects/libntr/meson.build)
X86 = {"-DSDK_X86", "-DSDK_CODE_X86"}
ANDROID = ["-DSDK_AARCH64", "-DSDK_BUILD_ANDROID", "-fPIC",
           # libntr's headers include SDL's, which have a per-architecture part: Debian's arm64 one
           "-idirafter", "/usr/include/aarch64-linux-gnu"]
# Pointers and integers of different sizes: what 64 bits breaks
WARNINGS = ["-Wint-to-pointer-cast", "-Wpointer-to-int-cast", "-Wint-conversion"]


def check(entry):
    args = shlex.split(entry["command"])
    out, skip = [], False
    for arg in args[1:]:
        if skip:
            skip = False
        elif arg in ("-o", "-MQ", "-MF"):
            skip = True
        elif arg in X86 or arg in ("-MD", "-w", "-fdiagnostics-color=always"):
            pass
        else:
            out.append(arg)
    result = subprocess.run(["aarch64-linux-gnu-gcc", *out, *ANDROID, *WARNINGS, "-fsyntax-only"],
                            cwd=entry["directory"], capture_output=True, text=True)
    return entry["file"], result.returncode, result.stderr


failed = 0
with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
    for path, code, stderr in pool.map(check, commands):
        # BW2's own warnings (libntr's headers have theirs on the desktop too)
        lines = [l for l in stderr.splitlines()
                 if "error:" in l or (any(w[2:] in l for w in WARNINGS) and "subprojects/" not in l)]
        if code != 0:
            failed += 1
        if lines:
            print(f"== {os.path.relpath(path, build)}" + (" (FAILED)" if code else ""))
            print("\n".join(lines[:12]))
print(f"{len(commands)} files, {failed} failed")
sys.exit(1 if failed else 0)

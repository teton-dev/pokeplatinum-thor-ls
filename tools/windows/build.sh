#!/bin/bash
# The win64 desktop build under MSYS2 (UCRT64), the Windows counterpart of tools/docker/desktop.sh build. See
# docs/thor/dev/desktop-windows.md. Run from any shell:  C:/msys64/usr/bin/bash.exe -l <repo>/tools/windows/build.sh [-Dopt=value...]
set -e
export MSYSTEM=UCRT64
# The asset scripts read UTF-8 (Windows' Python defaults to cp1252)
export PYTHONUTF8=1 LANG=C.UTF-8 LC_ALL=C.UTF-8
export PATH=/ucrt64/bin:/usr/local/bin:/usr/bin:/bin:/c/Windows/System32:/c/Windows
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
make build_win64/build.ninja
# The Gen 5 code when a pokebw2 clone is next to the repository
if [ -d ../pokebw2 ]; then
    subprojects/meson-1.12.0/meson.py configure build_win64 -Dpokebw2=../pokebw2 "$@"
fi
make win64

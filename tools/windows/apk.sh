#!/bin/bash
# The Android APK built on Windows under MSYS2 (docs/thor/dev/desktop-windows.md): tools/android/build_apk.sh with the
# Windows NDK. Set ANDROID_NDK_HOME, ANDROID_HOME, JAVA_HOME (17+) and GRADLE_HOME (8.9+) as Windows paths with forward
# slashes (S:/...), and VERSION_CODE; POKEBW2 defaults to ../pokebw2. Run:
#   C:/msys64/usr/bin/bash.exe -l <repo>/tools/windows/apk.sh
set -e
: "${ANDROID_NDK_HOME:?}" "${ANDROID_HOME:?}" "${JAVA_HOME:?}" "${GRADLE_HOME:?}" "${VERSION_CODE:?}"
export MSYSTEM=UCRT64 PYTHONUTF8=1 LANG=C.UTF-8 LC_ALL=C.UTF-8
export PATH="/ucrt64/bin:/usr/local/bin:/usr/bin:/bin:$(cygpath -u "$JAVA_HOME")/bin:$(cygpath -u "$GRADLE_HOME")/bin:/c/Windows/System32:/c/Windows"
export ANDROID_SDK_ROOT=$ANDROID_HOME
ROOT=$(cd "$(dirname "$0")/../.." && pwd -W)
export POKEBW2=${POKEBW2:-$(cd "$ROOT/.." && pwd -W)/pokebw2}
cd "$ROOT"
"$ROOT/tools/android/build_apk.sh"

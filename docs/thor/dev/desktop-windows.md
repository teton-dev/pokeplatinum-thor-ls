# The desktop build and tests on Windows (2026-11-25)

The Windows counterpart of `desktop-docker.md`: upstream's win64 target built natively under MSYS2, with the Gen 5 code,
and the scripted tests run without Docker or WSL. It renders on the PC's own GPU (real OpenGL, not software GL), so
enhanced-graphics work can be looked at at 4x-6x quickly. First set up on Windows 11 with an RTX 5070 Ti.

## Setup (once)

1. MSYS2 in `C:\msys64` (the installer from msys2.org, or `Install_MSys2.ps1`), then in an MSYS2 shell:

   ```sh
   pacman -Syu
   pacman -S --needed make p7zip git python3 flex bison ninja patch diffutils rsync zip \
     mingw-w64-ucrt-x86_64-{gcc,SDL2,nasm,enet,arm-none-eabi-gcc,libpng,pkg-config,cmake,python,glslang}
   ```

   (`patch` is needed by the Gen 5 code's patches; `glslang` gives `glslangValidator` to check shaders as GLSL ES.)
   Leave out `setup_msys2.sh`'s `git config --global core.autocrlf true`: the `packagefiles` patches must stay LF. In
   each repository: `git config core.autocrlf false` and `git config core.filemode false`.
2. **`bash` beside ninja**: the build's shell scripts have `#!/usr/bin/env bash`, which meson runs as a bare `bash`, and
   Windows finds `C:\Windows\System32\bash.exe` (the WSL launcher) before anything on the PATH. A hard link puts MSYS2's
   bash in ninja's and Python's own folder, which Windows searches first:
   `New-Item -ItemType HardLink -Path C:\msys64\ucrt64\bin\bash.exe -Target C:\msys64\usr\bin\bash.exe` (PowerShell).
3. The layout: the repositories side by side with `pokebw2`, and `thor-dev` beside them (or one folder up). Goldenrod's
   `gen5` is a git symlink; without symlinks on Windows make it a junction to Eterna's:
   `New-Item -ItemType Junction -Path gen5 -Target ..\pokeplatinum-thor-ls\gen5` (in Goldenrod's repository). Git then
   shows `gen5` as deleted: don't commit that.

## Building

```powershell
C:\msys64\usr\bin\bash.exe -l <repo>/tools/windows/build.sh
```

Configures `build_win64` (with `-Dpokebw2=../pokebw2` when the clone is there) and builds; the game is
`build_win64/pokeplatinum/main.exe` with its DLLs (not the bare `build_win64/main.exe`). `PYTHONUTF8=1` is set by the
script (the asset scripts read UTF-8; Windows' Python defaults to cp1252). An incremental rebuild after a libntr or
shader change takes a minute or two.

Windows-only fixes made for this (2026-11-25): `libntr_win64_build.patch` (no `setenv`), `src/port/gen5_data.c`
(`mkdir` takes one argument), `gen5/meson.build` (`-Wno-error=incompatible-pointer-types`: pokebw2's `s32` is `long`,
32 bits on Windows, and GCC 14+ makes the `int *` mismatch an error), `gen5/tools/prefix_symbols.py` (the objects go
to the linker in a response file: their paths exceed Windows' command-line limit), `meson.build`
(`find_program` for `copy_pc_files.sh`) and `copy_pc_files.sh` (any MSYS2 environment, not only `MSYS_NT`).

## The data folder

`thor-dev/desktop` (and `desktop-hg` for Goldenrod) as in `desktop-docker.md`: the ROM (a hard link: `New-Item -ItemType
HardLink`) and the `.ini` files. Extract the ROM without opening the game: `py tools/windows/nds_extract.py ROM.nds
thor-dev/desktop` (the same files libntr's first start writes; checked byte for byte against it on Platinum).
Goldenrod's folder also needs `firmware.bin` (its `res/pc_port/dwc_nv.bin`; the runner copies it into test runs). The BW2 import for Gen 5 tests: run
`tests/scripts/gen5_rom.txt` and copy `thor-dev/test-run/gen5` into `thor-dev/desktop/gen5` (269 MB).

## Tests

```powershell
tools\windows\test.ps1 -Script tests/scripts/enhanced_posts.txt -From turtwig -Hour 16 -Scale 4 -Config 'EnhancedShadows=true WindowWidth=1024 WindowHeight=1536'
```

The same scripts and fixtures as `desktop.sh test`. The run is muted (`SDL_AUDIODRIVER=dummy`, `MasterVolume=0`) and as
headless as Windows allows: the window is moved off every screen as soon as it appears, without taking focus, so nothing
covers the desktop. `-Env @{SIM_SUNSHADOW_DEBUG='1'}` passes debug variables; `-Name` keeps runs apart
(`test-shots/<script>-<name>`).

**Shots are written by the game** (since 2026-11-26): on a `shot` or `stillshot` line the script asks libntr
(`libntr_test_shot.patch`, `SIM_TestShotRequest`) for the next frame it presents, read back from the main window at
its drawable size and saved as `<name>.png` in the folder `test.ps1` passes as `POKEPLATINUM_TEST_SHOTS`
(`POKEHEARTGOLD_TEST_SHOTS` for Goldenrod); `test.ps1` waits for the file. A `PrintWindow` of the off-screen window,
used before, lagged what the game showed: black or stale shots of frames that were drawn (Gen 5 battles, widescreen).
Only a run that hangs (`timeout`) is still saved with `PrintWindow`. Without the variable (Docker's `desktop.sh`) the
game writes nothing and that harness takes its own.

**Run folder:** `test.ps1` wipes and reuses `<THOR_DEV>/test-run`, so two sessions testing at once overwrite each
other's runs. Give each session its own `THOR_DEV` (a folder with a `fixtures` junction to the shared one, and
`PKMNB2.nds` if the script imports the BW2 ROM) and pass `-Data` explicitly.

Checked on win64: `gen5_rom.txt`, `gen5_battle.txt` (a whole battle in BW2's engine), the enhanced-graphics scripts.

## Goldenrod (HeartGold/SoulSilver)

Goldenrod's meson has a `win64` target since 2026-11-25 (no lobby on Windows; `-Wno-error=incompatible-pointer-types`;
a few local re-declarations of SDK functions guarded with `#ifndef SDK_PORT`, since libntr's headers declare them with
their own types). Configure it with this repository's meson, with libntr's SDK dependencies forced to the subproject (a
system lookup through cmake fails under MSYS2), in the UCRT64 environment `tools/windows/build.sh` sets up:

```sh
python3 ../pokeplatinum-thor-ls/subprojects/meson-1.12.0/meson.py setup build_win64 -Dbuild_target=win64 \
  -Dpokebw2=../pokebw2 --force-fallback-for=NitroSDK,NitroSystem,NitroWiFi,NitroDWC,libvct
python3 ../pokeplatinum-thor-ls/subprojects/meson-1.12.0/meson.py compile -C build_win64
```

The game is `build_win64/main.exe`, with its DLLs from `C:\msys64\ucrt64\bin` (the runner puts that on the PATH).
Tests: `tools\windows\test.ps1 -Repo ..\pokeheartgold-thor-ls -Data thor-dev\desktop-hg -From hg/new-game -Script
tests/scripts/...` (the runner sets the `POKEHEARTGOLD_*` variables too). Two libntr patches came out of it, for both
games: `libntr_win64_stdio.patch` (HGSS keeps more archives open than Windows' default 512 C streams:
`_setmaxstdio(8192)`), and `libntr_test_missing_file.patch` (in test mode a missing ROM file is named on stderr and the
run ends, instead of a modal dialog no one answers).

## The APK from Windows

`tools/windows/apk.sh` runs `tools/android/build_apk.sh` with the Windows NDK (which `build_apk.sh` now picks under
MSYS2, calling `clang.exe --target=aarch64-linux-android29` in place of the NDK's `.cmd` wrappers, with `S:/...` paths
that the native meson and cmake read). Set `ANDROID_NDK_HOME`, `ANDROID_HOME`, `JAVA_HOME`, `GRADLE_HOME` (forward
slashes) and `VERSION_CODE`. Built 2026-11-25 with NDK r30, SDK 35 (AGP fetched build-tools 34), JDK 21, Gradle
8.14.3: `eterna-9100084.apk`, 21 MB (not yet installed on a device). The host tools `nitroarc` and `enumproc` keep
`-Werror` off and link `shlwapi` when built on Windows (their meson tested the target, not the build machine).

## Not done

- Goldenrod's APK from Windows (Goldenrod's own `build_apk.sh` hasn't had the Windows changes).

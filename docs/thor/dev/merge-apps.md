# One app for both games: scope (2026-11-25)

The owner asked to scope combining **Eterna** (Platinum, `app.eterna`) and **Goldenrod** (HeartGold/SoulSilver,
`app.goldenrod`) into one app. Nothing is built yet; this is the plan and its costs, from the code as it is.

## What the two apps share today

| Part | State |
|---|---|
| Java (`org.pokeplatinum.port`) | `NativeSettings.java` identical; `PlatinumActivity.java` differs in 3 places (Eterna's pacing-watchdog `unstickDisplays`, Goldenrod's list of accepted game codes `IPKE,IPGE`, the multicast lock's name); `SecondaryScreen.java` ~30 lines apart (the watchdog's `unstick`). |
| Gradle | Same SDK levels and `stable.keystore` (both apps are signed with the same key). Goldenrod adds a `dev` flavour (`APK_FLAVOR`). |
| Native | Each APK is `libSDL2.so` (6 MB) + `libmain.so` (Eterna 15.0 MB, Goldenrod 15.5 MB): the decomp, its libntr build and port-common, linked into one library. |
| libntr | Same pinned revision (`teton-dev/libntr` 578c6a8); the `packagefiles` patch lists differ: Goldenrod lacks the 11 `libntr_enhanced_*` patches, `libntr_3d_premultiplied`, the Android pacing ones (`android_main_async`, `android_watchdog_status`) and `wide_subtop_bgs`; Eterna lacks `libntr_wide_edge_bg0`. |
| port-common | Same revision; Goldenrod adds `port-common_gear_option.patch`. |
| gen5 | One tree: Goldenrod's `gen5` is a symlink to Eterna's in git. |

## The one hard constraint: one game per process

The two decomps cannot live in one native library. Measured on the shipped libraries (`llvm-nm -D --defined-only` of
`libmain.so` in eterna-9100083 and goldenrod-26112305): Eterna exports 27,127 symbols, Goldenrod 31,830, and **14,637
are in both**. About half of those are libntr, NitroSDK/NitroSystem and C++ (`SIM_*`, `NNS*`, `OS_*`, `GX*`, mangled
names), but thousands are game code: the HGSS decomp uses many of Platinum's names (`AreaDataManager_*`,
`Save*`, `Sprite*`, `Heap*`...). Each game also links its own libntr, with its own patch set and globals (the emulated
DS registers, the renderer). Renaming every symbol of one game (as `gen5/tools/prefix_symbols.py` does for BW2's code)
is possible in principle but out of proportion to the gain.

So the merged app keeps **two native libraries**, `libplatinum.so` and `libheartgold.so`, each built as today from
its own repository, and **runs each game in its own process**:

- A small launcher activity (native Android views, like the settings) lists the games and their ROMs and starts
  one of two `SDLActivity` subclasses, declared with `android:process=":platinum"` and `android:process=":heartgold"`.
- Each process loads only its game's library (`getLibraries()` returns `SDL2` + that one), so the duplicate symbols
  never meet. SDL's static Java state is per process, so it is safe too.
- Switching games ends one process and starts the other (as quitting does now): no in-process reset to write.
- The JNI names (`org.pokeplatinum.port.*`) stay. Both libraries register the same natives, but only one per process.

## The data folder

libntr `chdir`s into `SDL_AndroidGetExternalStoragePath()` (`sim_android.cpp`), which is `Android/data/<package>/files`
for both processes of one package. Each game needs its own subfolder:

- `files/platinum/` and `files/heartgold/` (extracted ROM, saves, `sim_config*.ini`, `gen5/`), chosen by the activity
  (an intent extra or a per-library constant read in `sim_android.cpp`): a small libntr patch, shared by both games.
- The BW2 import (`gen5/`, 269 MB extracted, measured on the Windows desktop build) could be shared by pointing both games at `files/gen5/`. That saves
  a second import and its disk space; it needs the path in `gen5_rom.cpp` to be configurable.

## Package ID and moving existing saves

Both apps are signed with the same key, so the merged app can **be an update of `app.eterna`** (a higher version
code), keeping Eterna's data in place (moved into `files/platinum/` on first run). Goldenrod's data is in another
app's `Android/data`, which Android 11+ doesn't let one app read. The ways to bring it over:

1. **adb** (the owner's devices): `adb shell cp -r /sdcard/Android/data/app.goldenrod/files/* /sdcard/Android/data/app.eterna/files/heartgold/`.
   Fine for the owner, useless for other players.
2. **Export from Goldenrod**: one last Goldenrod update whose Saves page writes a zip to `Documents/`, imported by the
   merged app's launcher (Storage Access Framework, which the app already uses for ROM import).
3. **A signature-protected `ContentProvider`** in a last Goldenrod update that the merged app reads directly (same
   key). Seamless, but more code than (2) for a one-time move.

Recommended: (1) now for the owner, (2) if the app is released to others. A new package ID instead (say
`app.thor.pokemon`) makes both games' data a migration and loses the update path; it isn't worth it.

**Version codes**: Eterna uses `91000NN` locally, `9100000 + run` in Actions; Goldenrod uses `YYMMDDNN`
(26112305 > 9100083). The merged app continues Eterna's (as an update of `app.eterna`) and must stay above
Eterna's Actions builds. `adb install -r` refuses lower codes.

## Building it

- Each game builds its `.so` as today (`tools/android/build_apk.sh` up to its meson/ninja step). A combined Gradle
  project takes both libraries as `jniLibs` (renamed to `libplatinum.so`/`libheartgold.so`) plus SDL2 once.
- Where the Android project lives: recommended **in Eterna's repo** (`android/`, a `combined` flavour taking
  `HEARTGOLD_LIB=<path>`), so each game repo keeps tracking its upstream untouched. A third, tiny repo for the shell
  is the alternative if the owner wants the games' repos to stay single-game.
- Actions: one workflow checks out both repos (and pokebw2), builds both libraries, then the APK. About twice
  today's ~10 min Eterna build.
- APK size: ~37 MB (two 15 MB libraries + SDL2), up from ~21 MB each.

## The libntr patch sets: unify first (done 2026-11-25)

**Done:** Goldenrod's `libntr.wrap` now lists Eterna's patches with its own `libntr_wide_edge_bg0.patch` where it was
(after `libntr_carts_known.patch`); the patches the two share are byte-identical, and each series applies cleanly to
libntr 578c6a8 and reproduces its checkout exactly. Goldenrod builds and runs with it on Windows, but only a one-room
smoke test (the bedroom at 4x) has been run: outdoors, widescreen, a Gen 5 battle and the Android build are unchecked,
and so is why `libntr_3d_premultiplied` and `libntr_wide_subtop_bgs` were Eterna-only (HGSS may draw differently with
them). Nothing of it is committed yet. The enhanced passes are
in its libntr, but nothing in HGSS calls them yet: the game side is the next step. The Android pacing patches look up
`unstickDisplays` through JNI, which Goldenrod's activity doesn't have yet; libntr clears the failed lookup, so it is a
no-op there until the Java side is copied over. The original note:


The merged app doesn't require one libntr patch set (each game links its own), but keeping two diverging sets is the
real maintenance cost, and Goldenrod needs the enhanced-graphics patches anyway. Before merging: bring Goldenrod's
`packagefiles` to Eterna's list (plus its own `wide_edge_bg0` and `gear_option`) in the same order, with a script that
checks both repos' checkouts against their patches (the handoff's "apply in order to a fresh checkout and diff").

## Order and size

| Step | What | Size |
|---|---|---|
| 1 | Unify the libntr patch sets (enhanced graphics, pacing watchdog into Goldenrod) | 2-3 sessions (much of it is the enhanced-graphics port to HGSS, planned anyway) |
| 2 | One `PlatinumActivity`/`SecondaryScreen` with the game as a parameter (accepted codes, lock name, data subfolder); libntr data-subfolder patch | 1 session |
| 3 | Launcher activity (game list, Add a game, last-played), two game activities in their own processes, combined Gradle flavour | 1-2 sessions |
| 4 | Moving data: `files/` into `files/platinum/` on first run; the Goldenrod import (adb now, export/import later) | 1 session |
| 5 | Actions workflow building both libraries; version code | 1 session |
| 6 | Device checks: both games on the Thor and the Duo, both screens, switching games, settings, link play | 1 session |

**Risks**: the Duo's dual-screen hosting (`SecondaryScreen`) and the pacing watchdog are per-activity and untested
across two processes; link play's LAN discovery from either game; the Actions build time.

**Open questions for the owner**: keep `app.eterna` as the merged app's ID (recommended)? The merged app's name and
icon? Should the launcher remember the last game and start straight into it?

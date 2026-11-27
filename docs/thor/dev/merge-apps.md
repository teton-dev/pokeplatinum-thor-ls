# SinjohDS: one app for both games (scope)

**Status: scope only (first written 2026-11-25, revised 2026-11-26). Nothing of SinjohDS is built.**

The owner wants **Eterna** (`app.eterna`) and **Goldenrod** (`app.goldenrod`) merged into one app called
**SinjohDS** (name decided 2026-11-26). It launches HeartGold/SoulSilver and their hacks, and Platinum with Renegade
and Following Renegade Platinum. The two sides share the Black 2/White 2 data, and Platinum may use HGSS's follower
sprites.

## Games it launches

| Engine | Game codes | Games |
|---|---|---|
| Platinum (`libplatinum.so`) | `CPUE` | Platinum, Renegade Platinum, Following Renegade Platinum |
| HGSS (`libheartgold.so`) | `IPKE`, `IPGE` | HeartGold, SoulSilver, Sacred Gold, Storm Silver |

Hacks run only as far as the port reimplements their code changes: an unknown hack gets "Add a game"'s warning and
may not work. Storm Silver is untested, and Following Renegade is ~70% (`following-renegade.md`). The merge changes
neither. HeartGold (JP) is not a game of its own here: only the Japanese Game Corner reads it.

**Platform:** assumed **Android** (Thor, Duo). The desktop builds stay two separate programs, used for testing.

## The hard constraint: one game per process (measured 2026-11-25)

The two decomps can't share a native library. `llvm-nm` on the shipped `libmain.so`s: Eterna exports 27,127 symbols,
Goldenrod 31,830, **14,637 in both**, including thousands of game functions. Each also links its own libntr with its
own globals. Renaming one game's symbols (as `gen5/tools/prefix_symbols.py` does for BW2) is out of proportion.

So SinjohDS ships **two native libraries** and **runs each game in its own process**: a launcher activity starts one
of two `SDLActivity` subclasses (`android:process=":platinum"` / `":heartgold"`), each loading only SDL2 and its
library. SDL's static Java state is per process. The JNI names (`org.pokeplatinum.port.*`) stay.

**What this means for sharing:** the games share **files**, never code or memory. Anything Platinum "gets from HGSS"
is read from HGSS's extracted ROM on disk, by Platinum's own code.

## Data layout

libntr `chdir`s into the app's `files/` (`sim_android.cpp`). Proposed:

```
files/
  shared/gen5/          the BW2 import (~269 MB), used by both engines
  shared/gen5-moon/     Moon Black 2's a/0/6/5 (the Fairy animations), if the player has it
  platinum/             Platinum's own ROM, saves, ini files, carts.ini, carts/<id>/
  heartgold/            the same for HGSS
```

Needs a small libntr change (the engine's subfolder as the base directory, and a "shared" path beside it), the same
in both games.

### Shared BW2 data

- Today `gen5_rom.cpp` uses `SIM_SettingsPath("gen5")`, so per-engine subfolders would **split** it. It moves to
  `shared/gen5/`, found through the new shared path.
- Either engine may import or remove it. The launcher only starts one game process at a time, which is what keeps
  two imports (both use `gen5.tmp/`) or a removal under a running game from happening: the launcher must end the
  other game's process before starting one.
- `gen5_data.c` builds `gen5_data/` in each cart's folder from BW2 plus that game's data. That stays per cart
  (hacks differ). Only its Moon Black 2 input (`gen5_data/moon/a_0_6_5` today, one copy per cart) moves to `shared/`.

### Follower sprites from HGSS

**Checked 2026-11-26** (texture headers only, read-only):

| Source | Follower members | Texture |
|---|---|---|
| HGSS `a/0/8/1` (863 members) | from 297 up | `tsure_poke.N`, 8 frames, 32x32 (64x64 for big ones), format 3, 2 palettes |
| Following Renegade `data/mmodel/mmodel.narc` (1325) | appended after Platinum's | same names, sizes, format and palette count (members sampled) |
| Platinum's own (470) | none of this kind | e.g. 16 frames, 1 palette |

So HGSS's sprites are in the layout `src/port/following_hack.c`'s renderer already draws. **Plan:** one follower
renderer (the hack's), fed from a second archive when the hack isn't installed, replacing `follower.c`'s built-in
list for the species it covers. No third follower implementation, and no converter expected. Not yet proven: that
the frame order matches (compare pixels, not just headers) and that the renderer works without the hack's tables.

Still needed:
- **Species to member mapping.** HGSS's code (`src/follow_mon.c`: `sModelIndexLUT`, `sFormMaxLUT`, `sFemaleFlagLUT`;
  member = 297 + offset + form or female) can't be called across processes. Platinum gets a table **generated** from
  that file at build time, so HGSS's source stays the owner. Per-species offsets, sizes and bobbing need the same.
- **Which HGSS copy.** The base HeartGold or SoulSilver cart, never a hack (a hack may change the archive).
- **Untrusted input.** Bounds-check the archive and the texture before use, as `gen5_rom_fs.h` does for BW2's.
- **Fallback.** With no HGSS game added (or a bad archive), today's follower (built-ins and party icons) stays.
- **Scope of following.** `follower.c` and the hack differ in behaviour (scripts, hops, ball effect). First step is
  only the pictures in vanilla and Renegade Platinum.
- **Species past 493**: none in either engine, nothing to do.

## Launcher

- A native Android activity (like the settings): one list of games from both engines, "Add a game", last played.
- Each engine already keeps its carts in `carts.ini` (`Current=`, `AskAtLaunch`, `SkipPickerOnce`; 8 carts at most).
  The files stay the **owner** of the cart lists; the launcher reads both, and starts a game by writing `Current=`
  and `SkipPickerOnce=1` before starting that engine's activity. The in-game picker is then off in SinjohDS.
- "Add a game" today refuses another engine's game code (`card_game_code`). The launcher reads the ROM's code first
  and hands the file to the right engine. Open design point: adding (extraction, xdelta patching, randomizer) is
  libntr C++ inside a game process, so the launcher starts that engine in an "add this file" mode (preferred) rather
  than reimplementing it in Java. Not yet traced how to enter that mode.
- Check the 8-cart limit: Platinum 3, HGSS 4, with versions held inside a cart.

## Package ID and existing saves

**Decided by the owner (2026-11-26): SinjohDS ships as an update of `app.eterna`**, with the SinjohDS label and icon.

- Eterna's saves stay in place and are moved into `files/platinum/` on first run.
- Version codes continue Eterna's (`91000NN` locally, `9100000 + run` in Actions) and must stay above them.
- Signed with the same `stable.keystore` as both apps.
- Goldenrod stays installed beside it until its saves are moved. Android 11+ doesn't let one app read another's
  `Android/data`: adb copy for the owner's devices now; an export zip from a last Goldenrod update, imported
  through the file picker, if the app is released to others.
- Every move copies, verifies, then leaves the originals in place.

## Building

- Each game builds its library as today; one Gradle project takes both as `jniLibs` plus SDL2 once. Recommended
  home: Eterna's `android/` (a `combined` flavour taking `HEARTGOLD_LIB=<path>`), so Goldenrod keeps tracking its
  upstream. APK ~37 MB (two ~15 MB libraries), up from ~21 MB each.
- Actions: check out both repos and pokebw2, build both, package. About twice today's ~10 min.
- The Windows build PC (`S:\Engage-Port` toolchain) can produce local APKs.

## Where the two sides stand

- Java: `NativeSettings.java` identical; `PlatinumActivity.java` differs in 3 places (pacing watchdog, accepted game
  codes, multicast lock name); `SecondaryScreen.java` ~30 lines (the watchdog). (2026-11-25)
- libntr: same pin (578c6a8). The patch lists were unified on 2026-11-25 (Goldenrod has Eterna's plus
  `libntr_wide_edge_bg0`), **uncommitted**; only a one-room smoke test on Windows then. Outdoors, widescreen, a
  Gen 5 battle and the Android build of Goldenrod with them are unchecked, as is why `libntr_3d_premultiplied` and
  `wide_subtop_bgs` had been Eterna-only. Goldenrod's activity lacks `unstickDisplays` (a no-op until copied).
- `libntrdwc_android.patch` differs (Goldenrod has an extra `nonport.c` hunk; 2026-11-26). Each library links its
  own, so it doesn't block the merge.
- port-common: same pin; Goldenrod adds `port-common_gear_option.patch`.
- Shared game code is copy-forked (root `PLAN.md` B-1), and `gen5` is a symlink in git but a junction on Windows (B-6).

## Order and size (sizes are estimates in working sessions)

| # | Milestone | Needs | Size |
|---|---|---|---|
| M0 | Commit the uncommitted work in both repos (PLAN.md D-1); check Goldenrod with the unified patches (outdoors, widescreen, Gen 5 battle, APK) | owner's OK to commit | 1 |
| M1 | One `PlatinumActivity`/`SecondaryScreen` with the engine as a parameter; libntr base and shared directories | M0 | 1 |
| M2 | Combined APK: two activities in their own processes, a minimal two-button launcher, Gradle flavour. **First thing to try on the Thor and Duo**, as the riskiest part | M1 | 1-2 |
| M3 | Shared BW2 and Moon data in `shared/` | M1 | 0.5-1 |
| M4 | Full launcher: carts of both engines, "Add a game" routed by game code, last played | M2 | 2 |
| M5 | Save migration (first-run move, Goldenrod copy-in), version codes, label and icon | M1 | 1 |
| M6 | HGSS follower pictures in Platinum (works without the merge on desktop; in the app needs M1's paths) | pixel check first | 2-3 |
| M7 | Actions workflow; device pass over every game, both screens, switching, link play | all | 1-2 |

Moving the shared source files into one place (B-1) is not required to ship the merge, but `gen5_rom.cpp` should
move before M3, which edits it in both copies. Total: about 10-13 sessions.

## Risks

- The Duo's second-screen hosting and the pacing watchdog are per activity and untried across two processes.
- Android may keep a game process alive in the background after switching; ending it must be explicit, after its
  save is flushed.
- Link play's LAN discovery from either engine.
- Fork-only (GPL, pokebw2): SinjohDS can't go upstream as it is.

## Candidates, not in scope

Moving Pokemon between Platinum and HGSS saves (the save editor's trade box is a possible base); one settings page
for options both engines have; shared texture packs; HGSS's follower behaviour (not just pictures) in Platinum.

## Open questions for the owner

1. An icon for SinjohDS?
2. Start straight into the last game, or always show the launcher?

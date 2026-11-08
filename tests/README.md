# Scripted tests (teton-dev fork)

The desktop build plays a script of keys and touches and checks that the game gets where it should: through the
new game's introduction, around Twinleaf Town, to a battle. They run in Docker on the Mac
(`tools/docker/desktop.sh`, [docs/thor/dev/desktop-docker.md](../docs/thor/dev/desktop-docker.md)), as fast as the game
goes (libntr's test mode), in a data folder of their own, and need the Platinum ROM extracted once
(`desktop.sh start`).

```sh
tools/docker/desktop.sh test tests/scripts/new_game.txt --keep before-starter
tools/docker/desktop.sh test tests/scripts/first_battle.txt --from before-starter
```

| Script | From | Checks | Time |
|---|---|---|---|
| `new_game.txt` | nothing | a new game (a boy named Claude, the rival Barry) to Prof. Rowan's briefcase on Route 201, saved there | ~20 s |
| `first_battle.txt` | `before-starter` | Turtwig from the briefcase, then Barry's battle, until it asks what to do | ~10 s |
| `gen5_rom.txt` | nothing | imports the Black 2 or White 2 ROM as the settings do, and BW2's code reads it (Gen 5 builds) | ~10 s |
| `turtwig.txt` | `before-starter` | Turtwig and Barry's battle, then home: keep it as the fixture `turtwig` | ~1 min |
| `gen5_battle.txt` | `turtwig` | a wild battle in BW2's engine, as the debug entry starts it: Tackle, run away, back on the overworld (Gen 5 builds) | ~1 min |

`first_battle.txt` is the check for the Gen 5 battles: with them on, the same battle runs in Black 2 and White 2's
engine, and the test passes once that asks for a command (BW2's battle raises the same `command` event as Platinum's).
`gen5_battle.txt` plays a whole battle in BW2's engine; BW2 reads the keys and touches the game read, the script's
among them.

## Running

`desktop.sh test SCRIPT [--from FIXTURE] [--keep FIXTURE]`:

- A run's data folder is `~/thor-dev/test-run`: the extracted ROM linked from `~/thor-dev/desktop` (not its Gen 5
  files: `gen5_rom.txt` imports its own), no saves, and
  the default settings, with the 3D at the DS's own size (the default, ten times larger, is far too slow in software).
- `--from FIXTURE` starts it from `~/thor-dev/fixtures/FIXTURE/save.bin`; `--keep FIXTURE` keeps the save it ends
  with there, when it passes. Fixtures are saves: never commit them.
- It prints the script's progress (`[test] ...`): each time the player is free, the map and tile
  (`free on map 342 at (114, 854)`), and the screenshots. They and the whole log are in
  `~/thor-dev/test-shots/SCRIPT/`; a failure takes `failure.png`.
- It exits 0 when the script passes and 1 when it fails: a wait that times out, a walk that is blocked, or a run
  longer than `TEST_LIMIT` seconds (600).
- When the game crashes, quits by itself, or is still running at the limit, the run prints where, as functions and
  lines (`crash.txt`): the backtrace of a crash or of the exit, or three samples a second apart of where the game
  thread is (`src/port/test_crash.cpp`; gdb can't run under Rosetta).

## Writing scripts

One command a line; `#` starts a comment. The game reads the script (`src/port/test_script.c`) from
`POKEPLATINUM_TEST_SCRIPT` and plays it frame by frame, so a script does the same every time.

| Command | Does |
|---|---|
| `press KEYS [frames]` | holds the keys (2 frames), then lets go for 2. Keys: `A B X Y L R START SELECT UP DOWN LEFT RIGHT`, joined with `+` |
| `hold KEYS frames` | holds them |
| `wait frames` | waits |
| `touch X Y [frames]` | touches the touch screen at (X, Y), in its 256×192 pixels (3 frames) |
| `walk DIR [tiles]` | walks the tiles once the player is free; ends early when the map changes (doors, stairs). A through any scene it starts on the way |
| `until EVENT [timeout N]` | waits for an event |
| `mash KEYS [touch X Y] until EVENT [timeout N]` | presses (and touches) 2 frames out of 8 until the event |
| `mash KEYS [touch X Y] for N` | the same for N frames |
| `save` | saves the game, as the menu does, once the player is free |
| `gen5-import` | imports the Black 2 or White 2 ROM in `POKEPLATINUM_GEN5_ROM` (the harness passes `GEN5_ROM`, or `~/thor-dev/PKMNB2.nds`) and waits; fails if it is refused or the self-check fails |
| `shot NAME` | a screenshot, `NAME.png` |
| `log TEXT` | prints the text |
| `exit [code]` | ends the run: 0 passes |

Events:

| Event | When |
|---|---|
| `free` | the player is free on the overworld (no scene, menu, warp or battle) for 20 frames |
| `map N` | free on map N (the `MAP_HEADER_*` numbers in `build_linux/generated/map_headers.h`) |
| `menu` | a menu or list opened: choices, yes/no |
| `naming` | a naming screen opened |
| `battle` | a battle started |
| `command` | a battle asks the player what to do |

An event counts once, from where the last `until` or `mash ... until` ended. `mash A until free` presses A only while
the player isn't free (A on a free frame would talk to whoever is in front). Timeouts are 3000 frames by default,
and as long again in real time: scenes that wait for music or a sound wait in real time, while test mode runs
frames as fast as it can.

Tips from writing the two scripts:

- `free on map ... at (x, z)` lines and the event files (`res/field/events/events_MAP.json`: warps, triggers,
  people) give the tiles. Stairs take a push towards their side once on them.
- Signs show while the player faces them and keep the player from being free: walk past them, not into them.
- Mashing A types on naming screens and picks the first choice in menus: wait for `naming` or `menu`, then act.

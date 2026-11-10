# Scripted tests (teton-dev fork)

The desktop build plays a script of keys and touches and checks that the game gets where it should: through the
new game's introduction, around Twinleaf Town, to a battle. They run in Docker
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
| `gen5_win.txt` | `turtwig` | a wild battle in BW2's engine that Turtwig wins (a level 1 Pichu); its experience in the log goes up (Gen 5 builds) | ~1 min |
| `gen5_wild.txt` | `turtwig` | with *Gen 5 battles* on, a wild Bidoof met as in tall grass (the test command `wild`) runs in BW2's engine; run away (Gen 5 builds) | ~1 min |
| `lake_grass.txt` | `turtwig` | to the tall grass at Lake Verity, through Cyrus's scene: keep it as the fixture `verity-grass` | ~2 min |
| `gen5_grass.txt` | `verity-grass` | with *Gen 5 battles* on, back and forth in the tall grass until a wild Pokémon appears, in BW2's engine; run away (Gen 5 builds) | ~1 min |
| `gen5_catch.txt` | `turtwig` | with *Gen 5 battles* on, a Bidoof caught into the party and, with the party full, a Starly into the PC (Master Balls); keep it as the fixture `gen5-caught` | ~2 min |
| `gen5_caught.txt` | `gen5-caught` | logs what that save has: the party, the PC, the bag and the Pokédex (the check is the log's lines) | ~30 s |
| `gen5_trainer.txt` | `turtwig` | with *Gen 5 battles* on, Youngster Tristan (trainer 1, the test command `trainer`) in BW2's engine with Platinum's AI: won, the prize money in the log (Gen 5 builds) | ~1 min |
| `pc.txt` | `turtwig` | a Pokémon Center's PC (Sandgem Town): the box screen shows, then back to the overworld (the check is the shot) | ~30 s |
| `gen5_loss.txt` | `turtwig` | a wild battle in BW2's engine that Turtwig loses (a level 100 Charizard): the blackout, back home (Gen 5 builds) | ~1 min |
| `new_game_girl.txt` | nothing | `new_game.txt` with a girl: keep it as `before-starter-girl`, then `turtwig.txt --from before-starter-girl --keep turtwig-girl` | ~20 s |
| `gen5_player_back.txt` | `turtwig` or `turtwig-girl` | a wild and a trainer battle in BW2's engine: the player's back sprite is the fork's own art of Lucas or Dawn through the throw (the check is the shots) (Gen 5 builds) | ~1 min |
| `gen5_widescreen.txt` | `turtwig` | with widescreen on (`TEST_CONFIG`, below): the overworld, then a wild and a trainer battle in BW2's engine fill the 16:9 window, the sprites and text in the 4:3 middle (the check is the shots) (Gen 5 builds) | ~2 min |
| `widescreen_weather.txt` | `turtwig` | with widescreen on: rain, a blizzard (and its start menu), a sandstorm, fog, ash and Snowpoint City, each across the whole 16:9 window (the check is the shots) | ~30 s |
| `widescreen_screens.txt` | `turtwig` | with widescreen on: Surf's cut-in band and the wall Town Map across the whole 16:9 window (the check is the shots) | ~30 s |
| `widescreen_bag.txt`, `widescreen_journal.txt`, `widescreen_flymap.txt`, `widescreen_party.txt` | `turtwig` | with widescreen on: that screen opened by the test command `app` fills the 16:9 window, its contents in the 4:3 middle (the check is the shot) | ~20 s each |
| `widescreen_journal_load.txt` | a save with journal entries | the Journal page that opens on loading it, with its paper and text (the check is the shots; the fixture is your own save) | ~30 s |
| `widescreen_gen4_battle.txt` | `turtwig` | with widescreen on, Gen 5 battles off: a wild and a trainer battle in Platinum's engine, the backdrop across the 16:9 window and the HP boxes' end caps (the check is the shots) | ~1 min |
| `widescreen_leader.txt` | `turtwig` | with widescreen on: Roark's "VS" encounter effect across the 16:9 window (the check is the shots) | ~20 s |

`first_battle.txt` is the check for the Gen 5 battles: with them on, the same battle runs in Black 2 and White 2's
engine, and the test passes once that asks for a command (BW2's battle raises the same `command` event as Platinum's).
`gen5_battle.txt` plays a whole battle in BW2's engine; BW2 reads the keys and touches the game read, the script's
among them.

## Running

`desktop.sh test SCRIPT [--from FIXTURE] [--keep FIXTURE]`:

- A run's data folder is `~/thor-dev/test-run`: the extracted ROM linked from `~/thor-dev/desktop` (not its Gen 5
  files: `gen5_rom.txt` imports its own), no saves, and
  the default settings, with the 3D at the DS's own size (the default, ten times larger, is far too slow in software).
- `TEST_ID=name` gives the run a container and a data folder (`test-run-name`) of its own, so that runs from
  two worktrees can go side by side; without it they stop each other.
- `TEST_CONFIG='Key=value ...'` adds libntr settings to the run's `sim_config.ini`. Widescreen:
  `TEST_CONFIG='SecondaryScreen=true Widescreen=true WindowWidth=768 WindowHeight=432'` (the bottom screen in its own
  window, the top one 16:9; the shots are of the top window).
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
| `pace DIR DIR until EVENT [timeout N]` | walks one way and back, about two tiles each, while the player is free: tall grass until `battle` |
| `save` | saves the game, as the menu does, once the player is free |
| `gen5-import` | imports the Black 2 or White 2 ROM in `POKEPLATINUM_GEN5_ROM` (the harness passes `GEN5_ROM`, or `~/thor-dev/PKMNB2.nds`) and waits; fails if it is refused or the self-check fails |
| `gen5-battles on\|off` | *Settings > Gameplay > Gen 5 battles* |
| `wild SPECIES LEVEL` | once the player is free, a wild SPECIES at LEVEL met as in tall grass (the field's own wild encounter, which the setting sends to the Gen 5 engine) |
| `item ITEM COUNT` | puts items into the bag (the item's number: 1 is the Master Ball, 4 the Poké Ball) |
| `give SPECIES LEVEL` | a new Pokémon into the party |
| `jump MAP X Z` | once the player is free, to map header MAP at (X, Z), as *Settings*' map jump does (420: Sandgem Town's Pokémon Center) |
| `script ID` | once the player is free, runs the field script ID (2018: a Pokémon Center's PC, as facing it does) |
| `app NAME` | once the player is free, opens `bag`, `journal`, `townmap`, `flymap` or `party` over the overworld; nothing returns to the field from it, so the script ends in it |
| `leader N` | once the player is free, plays Gym Leader encounter effect N (0: Roark, 1: Gardenia...) over the overworld |
| `trainer ID` | once the player is free, a battle against Platinum's trainer ID, as its encounter starts it |
| `party` | logs the party, the Pokémon in the PC, the Master and Poké Balls in the bag and, for the party, the Pokédex |
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

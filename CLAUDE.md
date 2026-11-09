# CLAUDE.md (teton-dev fork)

Read `AGENTS.md` first: it is the upstream project's guide (layout, building, the `SDK_PORT` rule, branches).
This file adds what is specific to this fork and to the Gen 5 battles work.

## The fork

`teton-dev/pokeplatinum-thor-ls`, a fork of `SilasMayes/pokeplatinum-thor`. The owner plays on an AYN Thor
(Android) and works from an M2 Mac.

- `platinum-thor`: the owner's personal build. Its `build_android.yml` sets
  `VERSION_CODE: 90000${{ github.run_number }}` so its APKs install over the official ones (keep that line).
  A push there builds an APK in Actions (artifact `pokeplatinum-android`), installed with `adb install -r`.
- Upstream pull requests, each from its own branch based on upstream `platinum-thor`:
  - `SilasMayes/libntr#1` from `teton-dev/libntr` `left-stick-dpad`: the left stick as a second D-pad, with
    *Settings > Controls > Left stick as D-pad*.
  - From this fork's `left-stick-dpad`: the libntr pin bump (points at `teton-dev/libntr` until libntr#1 is merged;
    then the `url` goes back to `SilasMayes/libntr`) and the manual.
  - From this fork's `fix-catch-double-free`: the crash after catching a new species (a double free in
    `ov21_021E9AC8`, the Pokédex entry screen).
  Keep upstream PR branches to one change each and rebase them on upstream `platinum-thor` when it moves.
- `gen5-battles`: the Gen 5 battles project (below). Fork-only.

## Commits and pull requests

- Commit as the owner. End messages with `Co-Authored-By: Claude <noreply@anthropic.com>` (or the model's name).
  **No `Claude-Session:` lines and no session links**, in commits or in pull request descriptions.
- Pull request descriptions say what changed, why, and what was tested on hardware (the upstream CONTRIBUTING asks
  for that), and end with "🤖 Generated with [Claude Code](https://claude.com/claude-code)".
- Never commit ROMs, saves, extracted game files, or anything from a BW2 ROM.

## Gen 5 battles

The plan is `docs/thor/dev/gen5-battles.md`: read it before working on this, and keep it current (mark milestones
done, record decisions and findings).

In short: compile the battle engine and battle view of [pokebw2](https://github.com/fuddlesworth/pokebw2) (the
Black 2/White 2 decompilation, GPLv3) into the app, and with *Settings > Games > Gen 5 battles* on, run each
battle Platinum sets up (`FieldSystem_StartBattleProcess`, `src/unk_0203D1B8.c`) in BW2's engine, converting the
`FieldBattleDTO` to BW2's `BtlSetup` and the results back.

Decisions made with the owner:
- The player's own **Black 2 or White 2 (USA)** ROM supplies the Gen 5 files (not Black/White: only BW2 is
  decompiled).
- **Vanilla Platinum first.** Renegade Platinum (its data, the Fairy type) is the last milestone.
- **Fork-only** because of the GPL; ask upstream's developer before proposing any of it there.
- Out of scope: following Pokémon, visible wild Pokémon, BW2 Phenomena, name capitalisation.

Status: **milestones 1 to 4 are done on the desktop build** (the plan's "Milestone 1" to "Milestone 4"): `gen5/`
builds 207 of pokebw2's files, renamed `bw2_...`, into one object the game links whole (`-Dpokebw2=../pokebw2`);
*Settings > Games > Gen 5 battles* imports a Black 2 / White 2 ROM into the data folder's `gen5/`, and BW2's code reads
it; with the switch next to the import on, wild single battles run in BW2's engine with the player's party, bag and
money, and the results (experience, levels, catches, evolutions, blackouts) come back. Still silent, and with black
patches in the arena's distant background until libntr's renderer clips polygons. Next is milestone 5, trainers.
An NDK build of the APK with the option is still to do. pokebw2 is never changed: the port's shims, patches and
boundary code are in `gen5/`. Keep pokebw2 in its own clone next to this repository, not inside it.

## Testing locally on the Mac

The owner's ROMs are in `~/thor-dev` (`PKMNPLAT.nds`, `PKMNB2.nds`). `tools/docker/desktop.sh` builds and runs the
desktop build in Docker (x86 under Rosetta) and shows it over VNC (`vnc://localhost:5900`); its `shot`, `key`,
`hold` and `click` commands let Claude check things on screen. The game's data folder is `~/thor-dev/desktop`.
`docs/thor/dev/desktop-docker.md` has the details and the known problems (WrapDB outages, the AppImage step).

Scripted tests (`tests/README.md`): `desktop.sh test tests/scripts/new_game.txt --keep before-starter` plays a new
game to Prof. Rowan's briefcase and keeps the save as a fixture; `first_battle.txt --from before-starter` picks
Turtwig and passes once Barry's battle asks for a command. That is the check that the Gen 5 engine loads and runs a
battle (milestone 3 on); `gen5_battle.txt --from turtwig` plays a debug battle in BW2's engine to the end. A crash,
hang or unexpected exit in a test run prints where (functions and lines), since gdb can't run under Rosetta.

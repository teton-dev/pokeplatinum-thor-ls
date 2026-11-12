# HeartGold and SoulSilver (draft plan, 2026-11-12)

The goal: the owner's HeartGold (and SoulSilver) ROM played natively on the Thor the way Platinum is here, with full
widescreen, BW2's battle engine (`gen5-battles.md`), 60 FPS and HD texture packs.

Status: **not started; this plan is waiting on the decisions at the end.**

## What there is to build on

- **Upstream's HGSS port.** `SilasMayes/pokeheartgold`, branch `thor-android`, is a native port of the HGSS
  decompilation (pret's, fully decompiled; HG and SS both build matching ROMs), on the same `libntr` and
  `port-common` as this repository, with its own APK (`org.pokeheartgold.port`). As of 2026-11-05 it is at bring-up:
  playable from Elm's lab to Sprout Tower, with a Linux test rig (`tools/port/testrig`). This is where HGSS work
  starts: a `teton-dev` fork of it, the way this repository forks `pokeplatinum-thor`.
- **This repository's Gen 5 work.** `gen5/` (BW2's engine, the shims, `gen5/api/gen5.h`) knows almost nothing about
  Platinum: only `trainers.c`, `sound.c` and `runtime.c` mention it. The Platinum side of the bridge is
  `src/port/gen5_battle.c` (party, bag, catching, trainers, results), written against Platinum's headers.
- **`hg-engine`** (`~/thor-dev/hg-engine-main`) is *not* the HGSS engine a native port needs. It is a ROM hack
  framework: C and assembly compiled for the DS's ARM and patched into a HeartGold ROM through hooks
  (`hooks`, `armhooks`, `bytereplacement`, `repoints`), as with Renegade Platinum's followers. The port can't run
  that code; using it means rewriting what's wanted in C against the decompilation. Its main features (newer
  moves, abilities and mechanics in battles, Fairy) are what BW2's engine already brings, and the rest (dex up to
  Gen 6, Megas) would need BW2's engine changed, which the project avoids. Its licence also asks that nothing made
  from it involve money. Treat it as a later, optional "hg-engine hacks" target, like Renegade Platinum, not a
  base.

## How the pieces fit

- `gen5/` becomes shared between the two games, as `port-common` is: its own repository (or a subproject pinned by
  commit), built with `-Dpokebw2=../pokebw2`, with each game supplying its bridge (`gen5_battle.c` for Platinum, a
  new one for HGSS) and its own AI-level table (`gen5-battles.md`, "The trainer AI").
- HGSS's battle setup is Platinum's with changes (`BattleSetup` in the decompilation, Safari and Bug Contest, the
  Pokéathlon-free parts, Apricorn and Park balls, the Pokéwalker's Pokémon, following Pokémon in battle entry).
  The bridge converts it to BW2's `BtlSetup` and the results back, as for Platinum.
- Widescreen, 60 FPS and texture packs are mostly in `libntr` and `port-common`, which HGSS shares; the per-game
  parts (the overworld camera and culling, the speed fixes listed in `features.md` "60 FPS", the battle screens'
  widescreen) are redone against HGSS's code.

## Milestones (proposed)

1. **The base.** Fork `SilasMayes/pokeheartgold` to `teton-dev`, build the desktop build in Docker and the APK
   with this repository's recipe, play from a new game to Sprout Tower on the desktop and the Thor. Record what
   upstream's port can't do yet.
2. **Bring-up to the end of the game.** Whatever upstream hasn't reached (Johto, Kanto, the Battle Frontier,
   the Pokéwalker, link features stubbed), fixing crashes found by scripted tests (a `new_game` and fixtures,
   as `tests/README.md` here). Coordinate with upstream so the work isn't done twice.
3. **`gen5/` shared.** Move it out to its own repository with no Platinum headers; Platinum keeps working
   unchanged (`first_battle.txt`, `gen5_battle.txt`).
4. **Gen 5 battles in HGSS:** wild battles, catching (Apricorn balls mapped to BW2 balls), trainers with HGSS's AI
   levels, doubles and partners, the Safari Zone and Bug-Catching Contest (or left to HGSS's own engine), with
   HGSS's music.
5. **Widescreen** on the overworld and in battles.
6. **60 FPS** on the overworld with HGSS's speed fixes, and texture packs (game code `IPKE`/`IPGE`).
7. **Following Pokémon** (HGSS's own) and BW2 battles together.

## Decisions needed from the owner

- Base the work on upstream's `thor-android` HGSS port (recommended), rather than porting the decompilation anew.
- Whether HGSS now moves ahead of Renegade Platinum (CLAUDE.md and `gen5-battles.md` put Renegade, milestone 8,
  first).
- Whether `hg-engine` matters at all for this (recommended: not as a base; maybe later as a hack target).
- "HD 3D models": the port has HD *texture* packs (Dolphin-style PNG replacement); replacing models themselves is
  new work, with no format or source for HD models yet. Texture packs for HGSS are milestone 6.
- Fork-only, as with Gen 5 in Platinum (GPL); ask upstream's developer before proposing any of it there.

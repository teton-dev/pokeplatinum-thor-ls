# Handoff: the Frontier after Gen 5 battles, Goldenrod's half (2026-11-18)

For the next session. Read `gen5-battles.md`, "The Battle Frontier", first (its "No Battle Video of a Gen 5 battle"
bullet describes the change), and `hgss.md`'s "The Battle Tower in Gen 5, ported from Eterna" bullet.

## What was done (2026-11-17)

1. **Goldenrod's Frontier crash, fixed** (`~/pokeheartgold-thor-ls`, `thor-android` `860d78bed`): HGSS's Frontier
   script commands read their relative offsets as `u32`; on 64-bit a backward jump went 4 GB astray. Now `s32`.
   `gen5_tower.txt` plays the Tower's single battle in BW2's engine. The older handoff,
   `handoff-2026-11-17-frontier-crash.md`, is marked fixed.
2. **Palmer's test hook in Goldenrod** (`e37700c14`): `TEST_ENV=POKEHEARTGOLD_TOWER_OPPONENT=305` makes Palmer the first
   opponent; `gen5_tower_palmer.txt` battles him in the light-pools arena.
3. **No Battle Video question or RECORD entry after a Gen 5 Frontier battle.** Platinum's and HGSS's videos are their
   own engine's recordings, so after a battle in BW2's engine YES would save an empty video. The scripts must stay as on
   the DS, so it's done in C, behind `GEN5_BATTLES`:
   - a flag, `Gen5Battle_SetFrontierUnrecorded` / `Gen5Battle_FrontierUnrecorded`, set when the Tower starts a battle
     in BW2's engine, cleared by a battle in the game's own engine or a new recording;
   - the command that checks the video slot before the question (Platinum `FrontierScrCmd_70`, HGSS
     `FrtCmd_CheckBattleRecordSlotValid`) steps over the "CompareVarToValue + CallIf" pairs and the yes/no that follow
     and answers NO (it leaves the script alone, and logs so, if the commands aren't the expected ones);
   - the room menu leaves out RECORD and moves down a line (Platinum `FrontierMenuManager_AddListMenuEntry`, entry
     `MenuEntries_Text_Record`; HGSS `FrontierListMenu_AddItem`, `msg_0191_00263`);
   - closing a message box that isn't open does nothing (the skipped question never opened it, and the scripts close
     it after).
   The log says "Gen 5: no Battle Video of a battle in BW2's engine: the question skipped".

## Where it stands

**Eterna (Platinum), done:** `gen5-battles` `b3aa76b19`, tested on the desktop:
- `gen5_tower.txt` (from `full100`): forfeit, then straight to the lobby's "Thank you for playing!".
- `gen5_tower_win.txt` (new, from `full100`): a Spiritomb given Wonder Guard (new test command `ability ABILITY`, on the
  first Pokémon) and Dark Pulse in all four slots wins by mashing A (BW2's result 1); the room menu shows KEEP GOING,
  REST, RETIRE, no RECORD.

**Goldenrod (HGSS), written, not committed** (`~/pokeheartgold-thor-ls`, `thor-android`, working tree):
- Changed: `include/port_sdk/port_gen5_battle.h`, `lib/port/gen5_battle.c` (the flag),
  `src/frontier/frontier_script_commands_misc.c` (set at the Tower battle), `src/frontier/frontier_script_commands_ext.c`
  (the skip; cleared in `FrtCmd_CreateBattleRecord`), `src/frontier/frontier_script_ui.c` (RECORD, the message box),
  `lib/port/test_editor.c` (new test commands `editor ability SLOT N` and `editor allmoves SLOT MOVE`),
  `tests/README.md`, `tests/scripts/gen5_tower.txt`, `gen5_tower_palmer.txt` (last shot renamed `after-battle`) and the
  new `tests/scripts/gen5_tower_win.txt`.
- `gen5_tower.txt` passes: "the question skipped", then the lobby's "Thank you for playing!".
- **`gen5_tower_win.txt` doesn't pass yet:** the battle starts (against trainer 48) and asks for a command, then no
  "battle ended" line in 600 s. Not looked into. Things to check, in order:
  1. Is the Spiritomb leading, with Wonder Guard and Dark Pulse, *in BW2's battle*? The log's `editor ability 0 25` and
     `editor allmoves 0 399` passed, but the party screen picks three Pokémon in an order of its own, and HGSS's bridge
     (`lib/port/gen5_battle.c`) might not carry the ability across. Add `shot` lines inside the battle (after
     `mash A until command`: `wait 60`, `shot battle-command`; then every few thousand frames) to see. Note: the
     `timeout.png` of the failed run showed the lobby's "ETHAN saved the game.", not the battle, so the failure shot may
     not capture BW2's screens; plain `shot` lines did in Eterna.
  2. `allmoves` writes the moves with PP 15; if BW2 rejects a moveset with one move four times, try Dark Pulse, Shadow
     Ball, Ominous Wind and Sucker Punch (all damaging; only a Disable of the first could stall mashing A).
  3. Trainer 48 had a Beautifly in an earlier run: a status move (poison, Leech Seed) or weather can still wear a Wonder
     Guard Pokémon down. If so, a lead holding Leftovers (`hold`... HGSS's editor has no hold; add one, or pick a
     different first opponent with `TEST_ENV=POKEHEARTGOLD_TOWER_OPPONENT=N`).
  Then commit Goldenrod's change (one commit, as Eterna's `b3aa76b19`; nothing to push without asking).

## Next steps

1. **Goldenrod's win test** (above), then commit.
2. **Notes:** `hgss.md` (the Tower bullet: no video question or RECORD after a Gen 5 battle, the new editor commands),
   `plan.md` step 4 (the Battle Video is settled: skipped; next the Tower's doubles and the other facilities), and the
   memory note `hgss-frontier-crash.md`.
3. **The Tower's doubles** in BW2's engine (`Gen5Battle_WantsFrontier` takes only `BATTLE_TYPE_FRONTIER_SINGLES` in
   Eterna, `BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER` in Goldenrod). The video skip already covers any facility
   whose scripts use the same "slot check, CompareVar/CallIf, yes/no" pattern, and their `StartBattle` commands need the
   same `Gen5Battle_SetFrontierUnrecorded` calls as the Tower's.
4. **The other facilities** (Factory, Hall, Castle, Arcade), then Pal Park (`plan.md`).
5. **An APK of each** for the Thor: none of the Frontier work has been on hardware.

## Found on the way, not fixed

- **PP in facility battles:** BW2 showed Earthquake (with PP Ups) as "PP 16/10": the current PP is kept but the
  maximum drops the PP Ups. The bridge passes both (`gen5/src/runtime.c` sets `PKM_PARAM_MOVE1_PP_UP` and
  `PKM_PARAM_MOVE1_PP`), so it's likely BW2's level-50 copy of the party for facility battles. Not investigated.
- **Exit 135 with no backtrace:** twice an Eterna test run ended with exit code 135 (signal 7) and nothing from the crash
  handler (which does catch SIGBUS), once at the Tower's save, once mid-battle; both passed on a rerun. Probably Rosetta
  or Docker killing the process, not the game. Rerun before investigating.
- **Winning by mashing A** depends on the opponent's random team (a Disable on the first move, a Shedinja against
  Earthquake): use the Wonder Guard Spiritomb (the owner's suggestion), not a strong attacker.

## Testing

```bash
SCREEN=1920x1080 tools/docker/desktop.sh test tests/scripts/gen5_tower_win.txt --from full100
```

(Eterna, in `~/pokeplatinum-thor-ls`.) Goldenrod's, in `~/pokeheartgold-thor-ls`, after `tools/docker/desktop.sh build`:

```bash
SCREEN=1920x1080 tools/docker/desktop.sh test tests/scripts/gen5_tower_win.txt --from new-game
```

A run takes 3 to 10 minutes: start it in the background, and don't run two Docker tests at once.

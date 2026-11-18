# Handoff: Goldenrod's Battle Frontier crashes after the Tower's save (2026-11-17)

> **Fixed (2026-11-17):** the `u32` offsets were the cause. Read as `s32`, the Frontier runs and `gen5_tower.txt`
> (Goldenrod) plays the Tower battle in BW2's engine to the Battle Video question. Palmer's test hook (`POKEHEARTGOLD_TOWER_OPPONENT`,
> `gen5_tower_palmer.txt`) is done too, and `hgss.md` updated.

For the next session picking this up. The goal: Goldenrod's Battle Tower single battles run in BW2's engine, as
Eterna's do. The Gen 5 bridge for it is written and committed; the Frontier itself crashes before the battle is
reached. The fix below hasn't been tried yet.

## Where things stand

- **Eterna (Platinum):** the Battle Tower's single battles run in BW2's engine, and Palmer battles in the light-pools
  arena (the owner confirmed it). Tested on the desktop (`tests/scripts/gen5_tower.txt` and `gen5_tower_palmer.txt`,
  from the `full100` fixture). See `gen5-battles.md`, "The Battle Frontier".
- **Goldenrod (HGSS), commit `cae433aa6` on `thor-android`:**
  - The same bridge is ported (`lib/port/gen5_battle.c`: `Gen5Battle_WantsFrontier`, facility battle, Frontier
    lines from `msg_0197_00724`, a forfeit is a loss, World Tournament arena, light pools for the Brains), launched
    from `FrtCmd_TowerStartBattle` (`src/frontier/frontier_script_commands_misc.c`).
  - A port bug is fixed: `FrontierFieldSystem_ReadPartySelection` (`src/frontier_field_system.c`) read the party
    menu's result at the DS's byte offsets (0x26, 0x27, 0x30). Those sit behind pointers that are twice as long on
    64-bit, so every slot came out 255 and the Tower said "Your Pokémon must all be different". It now reads
    `PartyMenuArgs`' fields (port only). `PartyMenuArgsSlotView` (the Union Room's battle choice) had the same
    problem and is fixed too.
  - `tests/scripts/gen5_tower.txt` (Goldenrod) gets as far as "ETHAN saved the game." and passes.
- **The crash:** the next step after the save is the Frontier's own scene (the elevator up to the battle room). It
  crashes there, so Goldenrod's Frontier has never run on the port, and the Gen 5 Tower battle is never reached.

## Reproducing it

In `~/pokeheartgold-thor-ls`:

```bash
tools/docker/desktop.sh build
```

Then extend `tests/scripts/gen5_tower.txt` past the save (the version that crashed added, after `stillshot p5`:
`press A`, `wait 200`, `stillshot p6`, `mash A until command timeout 9000`) and run:

```bash
SCREEN=1920x1080 tools/docker/desktop.sh test tests/scripts/gen5_tower.txt --from new-game
```

`~/thor-dev/test-shots/hg/gen5_tower/crash.txt` has the backtrace:

```
FrontierScriptContext_ReadHalfWord at src/frontier/frontier_script_context.c:111
FrontierScriptContext_Run at src/frontier/frontier_script_context.c:66
FrontierSystem_Main at src/frontier/frontier_system.c:45
Frontier_Main at src/frontier_runtime.c:92
OverlayManager_Run ... FieldSystem_Main ... NitroMain
```

So `ctx->scriptPtr` points at memory that isn't the script (`FrontierScriptContext_ReadByte` is
`*(ctx->scriptPtr++)`).

## The likely cause: unsigned jump offsets on 64-bit

HGSS's Frontier script commands read their relative jump offsets as **`u32`** and add them to the script pointer.
On the DS (32-bit pointers) a backward jump, an offset of 0xFFFFxxxx, wraps around to the right address. On the
64-bit port, `ptr + (u32)0xFFFFFFF0` lands about 4 GB past the script instead of 16 bytes before it.

Platinum's Frontier, which works on the port, reads the same offsets as **`s32`**
(`src/overlay104/frscrcmd.c`: `s32 offset = FrontierScriptContext_ReadWord(ctx);` in GoTo, GoToIf, Call...).

The `u32` sites in HGSS (`~/pokeheartgold-thor-ls`):

| File | Line | Command |
|---|---|---|
| `src/frontier/frontier_script_commands.c` | 228 | `FrtCmd_GoTo` |
| `src/frontier/frontier_script_commands.c` | 235 | `FrtCmd_GoToIf` |
| `src/frontier/frontier_script_commands.c` | 243 | `FrtCmd_Call` |
| `src/frontier/frontier_script_commands.c` | 471 | `FrtCmd_LoadObjectGfx` (offset to its data) |
| `src/frontier/frontier_script_commands.c` | 513 | `FrtCmd_CreateObjects` (offset to its data) |
| `src/frontier/frontier_cmd_create_sprites.c` | 23 | the sprite creation command (offset to its data) |
| `src/frontier/frontier_system.c` | 158 | `FrontierScriptContext_GoToScript` (`scriptPtr += ReadWord(ctx)`, the script table's entry; forward only, probably fine) |

`FrtCmd_CallIf` (line 253) already uses `int` and is fine.

**Suggested fix:** read these offsets as `s32` (or cast: `ctx->scriptPtr + (s32)offset`). To keep the DS build
matching, either check it still matches with `s32` (it should compile the same on ARM), or guard the change with
`#ifdef SDK_PORT` as the party-selection fix does. Then rerun the extended test: it should reach the elevator, the
battle room, and "Gen 5: a single facility battle against trainer ...".

## If that isn't it

- **Movement scripts:** `FrontierScript_StartMovement` (`frontier_script_commands.c`, declared near line 71) takes a
  script pointer from an offset too: check how it's computed.
- **Other raw offsets:** HGSS's decomp still has places that read structs at their DS byte offsets (`u8 *unk = ...;
  unk[0x..]`). Any struct with pointers in it is laid out differently on 64-bit. Grep `src/frontier*` and
  `src/frontier/` for `[0x` on byte pointers.
- **Which command crashed:** log `cmdCode` and the script offset (`ctx->scriptPtr - ctx->scripts`) in
  `FrontierScriptContext_Run` before each command, and compare with the script's disassembly (NARC `a/1/8/2`, member
  from `FrontierMap_GetParam(mapId, MAP_PARAM_SCRIPT_ID)`).
- gdb can't run under Rosetta: the crash handler's backtraces (`lib/port/test_crash.c`) are what there is.

## After the fix

1. Extend `gen5_tower.txt` to the battle: `mash A until command`, then RUN (`mash touch 128 175 for 20`) and YES
   (`mash touch 128 55 for 20`), as Eterna's test does. Check the log for `battle ended (BW2's result 3` and the Battle
   Video question.
2. Add the Palmer test hook (Eterna's `POKEPLATINUM_TOWER_OPPONENT` is in `src/unk_0204AEE8.c`'s
   `BattleTower_GetTrainerIDForRoomAndOpponentNum`). HGSS's equivalent picks the Tower's trainer IDs in its Frontier
   code. Pass the variable through `tools/docker/desktop.sh` (HGSS uses `POKEHEARTGOLD_*` names).
3. Update `hgss.md` (the "Battle Tower in Gen 5, ported from Eterna" bullet) and `plan.md`.

## Other state to know

- **Not pushed:** Platinum `gen5-battles` after `be72f1d25` (the Gear notes, the Tower, Palmer, the arena
  confirmation, the HGSS Tower notes, this handoff); Goldenrod `thor-android` after `e254e4e8d` (the empty slot's
  number, the Gear parts 1-2, the Tower port). `platinum-thor` last merged at `79943b135` (Eterna build #36).
- **The Thor:** disconnected over USB since the Eterna #36 install attempt. Waiting: Eterna Actions run #36 (downloaded
  to `~/thor-dev/apk/run-44211043475/`) and a fresh Goldenrod APK (rebuild it: 26111703 predates the Tower port), with
  `VERSION_CODE` above 26111701.
- **Open on the Frontier, both games:** the Battle Video question after a Gen 5 battle (YES may save an empty or broken
  video), the Tower's doubles and multi battles, the other facilities, Pal Park.

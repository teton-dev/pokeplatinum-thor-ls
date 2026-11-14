# Nuzlocke mode (draft plan, 2026-11-13)

An enforced Nuzlocke mode, per save, in Platinum first and then HeartGold/SoulSilver. Fork-only for now (it touches
the Gen 5 bridge, which is). **Milestone 1 done** (2026-11-13): the state and the switch; no rule is enforced yet.

## The rules (as the owner set them)

**Per save.** A save turns the mode on behind a warning pop-up that spells out the rules, can move between the three
levels (Simple, Normal, Hard) and can turn it off. **Once off, it can never be turned on again on that save**: the
save remembers that it was a Nuzlocke and was dropped. A save with the mode on shows the alpha mark: on the in-game
save screen, and pixelated under the slot's info on the main menu's Continue box, whose border turns red.

**Every level.**
- **IVs are 15** for every Pokémon made while the mode is on: wild, starter, gifts, legendaries, enemy trainers'.
- **Crit guard** (a checkbox, optional, not with Hard): no critical hits, except from moves that always land one
  (Storm Throw and Frost Breath in BW2's engine; Platinum has none).
- **Level cap**: the next gym leader's strongest Pokémon. Pokémon gain no EXP past it until the next badge.
- **Fainted is dead**: a Pokémon that faints gets a flag that keeps its HP at 0 for good. No Revives, potions,
  healing moves, Pokémon Centers or NPC heals bring it back; it can never battle again.
- **No items in battle** (Poké Balls excepted, or nothing could be caught), and enemy trainers lose their items.
- **Nicknames are required**: a caught Pokémon goes straight to the naming screen, which won't take a blank name
  ("You must name your new Pokémon!").
- **Whiting out ends the run**: losing a battle for good turns the mode off on that save, for good.

**Normal** adds the catch rule, from the first time the player has Poké Balls: an area's first wild encounter decides
its species. If that Pokémon fled, was run from or fainted, only that species may be caught there, when it turns up
again. Once something is caught in an area, no ball can be thrown there again: in both engines the ball is knocked
back as a trainer does it, with "You've already used your catch for this area!". **Shinies can always be caught.**

**Hard** is Normal with the cap at the next gym leader's *weakest* Pokémon, and no crit guard.

## What is already there

- **The Nuzlocke helper** (`subprojects/port-common/src/nuzlocke.c`, *Settings > Gameplay > Nuzlocke*): upstream's,
  a global setting that keeps books and enforces nothing. Each area's (location name's) first encounter and whether
  it was caught, deaths by personality, dupes and shiny clauses, fainted Pokémon optionally sent to the PC; shown on
  the Gear. Its books live beside the slot (`save_slotN.nuzlocke`), go along with save sync, and hook the game in
  `src/encounter.c` (`Nuzlocke_OnBattleStart`/`OnBattleEnd`, run for both engines). The area tracking is most of the
  Normal catch rule's bookkeeping.
- **The hard level cap** (*Settings > Gameplay > Level cap*): `Gear_LevelCap()` (`src/port/gear_data.c`) is the next
  gym boss's ace level (the Champion's after 8 badges), read from the game's trainer data, applied in
  `BtlCmd_GetExp` (`BattleScript_PortCapExp`, `src/battle/battle_script.c`). Only in Platinum's engine: BW2's
  battles don't apply it yet. The boss table already has every Pokémon, so the weakest is a small addition.
- **Save slots** (`port-common` `save_slots.c`): four, switched on the main menu (`< SLOT n/4 >` in
  `src/main_menu/main_menu.c`), each with a summary (`SaveSlotInfo`) the menu and Settings show.
- **The Gen 5 bridge** (`src/port/gen5_battle.c`) already runs Platinum's naming screen after a BW2 catch, so the
  nickname rule is one change for both engines. BW2's engine is changed only through `gen5/patches/` and the
  `Gen5Battle` struct.
- **HGSS** (`~/pokeheartgold-thor-ls`) has no `port-common`, no helper and no level cap. "Mostly ports over" means the
  rules' state and logic go in game-neutral code, and each game supplies its hooks.

## How it fits

**Where the state lives.** In the save itself, not in a file beside it: it then goes with exports, imports and save
sync, and deleting a file can't undo "removed". A small port block (mode, crit guard, the "was a Nuzlocke" flag,
"has had Poké Balls", the area books) in space the DS game doesn't use, so the save still works on a DS or an
emulator (there it just plays as a normal game). Where exactly is to be found (the 512 KB image's unused tail, or
spare bytes of a save block); the existing helper's books move in too, or the mode keeps its own.

**The dead flag** goes on the Pokémon, so it survives the PC, the Day Care and trades between slots: a spare field of
the Pokémon data (`BoxPokemon` has `unused2` at 0x1A, in a block both games keep; to check against HGSS's struct).
Every healing path then checks it: `Party_HealAllMembers` (Centers, NPCs, the Pokémon League), item use (Revive, Max
Revive, Revival Herb, Sacred Ash, potions), PC withdrawal and Day Care return (which recalculate HP), level-up stat
recalculation, and the Gen 5 bridge's results.

**One hook per rule, both engines.**

| Rule | Platinum's engine | BW2's engine (bridge / `gen5/patches`) |
| --- | --- | --- |
| IVs 15 | where `Pokemon_InitWith` sets IVs (covers all) | arrives already set from Platinum's parties; check BW2-built trainer parties (`gen5/src/trainers.c`) |
| Level cap (high/low) | `BattleScript_PortCapExp` | new: a cap in `Gen5Battle`, applied in BW2's EXP step (patch) |
| Crit guard | the crit roll in `battle_lib.c` | the crit roll in `btl_calc.c`/`btl_server_flow.c` (patch), always-crit moves spared |
| No items in battle | the battle bag: only the Poké Ball pocket | the bag the bridge hands over: balls only |
| Trainers' items | cleared when the trainer is loaded | the same data, cleared before conversion |
| Catch lock | the ball-use command: the trainer "blocked the ball" path, new message | BW2's trainer-blocks-ball path, behind a flag (patch) |
| Nickname | the in-battle "nickname?" prompt is skipped to the naming screen | the bridge's naming screen |
| Whiteout | Platinum's blackout task | the bridge's lost result reaches the same task |
| Dead flag | set on fainting (end of battle, from the party) | the same, from the results |

**The marks.** The alpha mark is redrawn as an original pixel sprite (the PNG in `~/thor-dev/art` is a fan's
drawing, by jormxdos, and isn't committed). The save screen's info window (start menu > Save) gets the icon; the
main menu's Continue box draws it under the slot's info and swaps its frame palette to red. `SaveSlotInfo` gains the
mode so the menu knows without loading the save.

## Milestones (proposed)

1. ~~**The state and the switch**~~ (done, below): the save block, the dead flag, turning the mode on, changing the level, turning it
   off (and never on again), the warning pop-up, the crit guard checkbox (refused with Hard).
2. **The rules in Platinum's engine** (mostly done, below): IVs, caps, the dead flag everywhere HP is restored, no items, trainers' items,
   forced nicknames, whiteout ends it.
3. ~~**The catch rule**~~ (Normal and Hard) in Platinum's engine (done, below), with the message and the ball knocked back.
4. ~~**The same in BW2's engine**~~ through the bridge and patches (cap, crit guard, items, catch lock) (done, below).
5. **The marks** on the save screen and the main menu.
6. **Scripted tests** (`tests/scripts`): a Nuzlocke fixture; a faint then a Center visit (still 0 HP); a whiteout
   (mode off, can't be turned on); a second encounter on a used route (ball refused); the same with a shiny; the
   level cap in both engines.
7. **HGSS**: the shared part moved out (as `gen5/` will be), HGSS's hooks (its first Poké Balls come from Elm's aide;
   Kanto's gyms after Johto's eight, then Red).

## Milestone 1: the state and the switch (done, 2026-11-13)

- **The core** is `src/port/nuzlocke_mode.c` (`include/port/nuzlocke_mode.h`), game-neutral; what it needs from a
  game is `include/port/nuzlocke_mode_game.h`, Platinum's in `src/port/nuzlocke_platinum.c`. `NuzlockeMode_Update`
  runs once a frame in the main loop (`src/main.c`), on the game thread.
- **The record** (64 bytes, CRC-32, version 1, room left in its sector for the books later) is keyed by the save's
  trainer ID (`TrainerInfo_ID`, 32 bits), in flash sectors 60-63 of the primary half and 124-127 of the backup half,
  one record per sector: four saves' worth, the oldest reused. Platinum's own data stops at sector 43 of each half
  (the body is 0-31, the extra saves 32-43); erasing the save clears the records with the rest. The newer valid copy
  wins. A record for another trainer ID is ignored: a new game started over a Nuzlocke save starts off. On the port,
  the flash is the slot's file (`SIM_Backup_Write` writes and flushes at once), so a change is on disk before the
  game saves, and reloading an older game doesn't undo "dropped". Port save files are often shorter than 512 KB;
  the backup record makes them 0x7D040 bytes, which export pads as before. A DS or an emulator ignores the records.
- **Writes wait** while the game is saving (`gSystem.inhibitReset`), since a save spans frames.
- **The settings pause the game thread** while they are open: the GUI applies a change to what it shows at once
  (the same rules: Off drops for good, Hard clears crit guard, a dropped save stays as it is) and hands the whole
  wanted state to the game thread, which writes it when it runs again.
- **The dead flag** is bit 0 of `BoxPokemon` block B's `unused2` (`MON_DATA_UNUSED_114`, a u16), free in both games
  (HGSS uses only `unused1`'s bits, for Shiny Leaves): `NuzlockeMode_IsDead`, `NuzlockeMode_BoxIsDead`,
  `NuzlockeMode_SetDead`. Nothing sets it yet (milestone 2).
- **Settings > Gameplay > Nuzlocke mode**: with no game running, a note; off, a choice of level that opens the
  warning (every rule of that level, Simple's first) with *Turn it on* and *Cancel*; on, the level, *Crit guard*
  (greyed out with Hard) and *Turn Nuzlocke mode off* (tap twice); dropped, "This save was a Nuzlocke run and left
  it: it can't be one again." Upstream's helper stayed under it as *Nuzlocke helper* until milestone 3 retired it.
- **Tests**: the test command `nuzlocke simple|normal|hard|off`, `nuzlocke crit on|off`, `nuzlocke state`.
  `nuzlocke_on.txt` (from `turtwig`, keeps `nuz-normal`), `nuzlocke_drop.txt` (keeps `nuz-dropped`: turned off
  without saving), `nuzlocke_state.txt` (from `nuz-dropped`: still "Off, dropped" with the older game loaded) pass;
  a record edited to another trainer ID reads as Off. The settings page was checked on the desktop over VNC.
- **Not done**: save sync uploads a slot when it is saved, so a run turned off without saving reaches other devices
  only with the next save. The main menu doesn't know a slot's mode yet (milestone 5 reads the record there).

## Milestone 2: the rules in Platinum's engine (mostly done, 2026-11-13)

Each hook is behind `SDK_PORT` and does nothing with the mode off. The per-game rule functions are in
`src/port/nuzlocke_platinum.c` (declared in `nuzlocke_mode.h`).
- **IVs 15**: `sub_02073E18` (under `Pokemon_InitWith`, so wild, gift, starter, trainers' and egg Pokémon) sets all six
  after drawing the random ones, so the RNG advances as before; `Pokemon_InitAndCalcStats` does the same for roamers
  made before the mode was on. NPC trades keep their fixed IVs, and Day Care eggs inherit their parents' IVs.
- **Level cap**: `NuzlockeMode_LevelCap()` is `Gear_LevelCap()` (the next boss's ace), or `Gear_LevelCapLow()` (new in
  `gear_data.c`: the weakest of the boss's party) with Hard. `BattleScript_PortCapExp` takes the lower of it and the
  *Level cap* setting's. Rare Candies are refused at the cap (below).
- **The dead flag**: `Pokemon_SetValue(MON_DATA_HP)` is where it all goes through. HP set to 0 on a real Pokémon
  (stats, not an egg) sets the flag: the battle's party updates (`BtlIOCmd_UpdatePartyMon`) and the Gen 5 bridge's
  results both write HP that way. A dead Pokémon's HP stays 0 whatever is written (`Party_HealAllMembers` for Centers,
  NPCs and the League; PC withdrawal and the Day Care, whose stat recalculation sets full HP), and
  `Pokemon_IncreaseValue(MON_DATA_HP)` does nothing to it. Items: `Pokemon_CheckItemEffects` and
  `Pokemon_ApplyItemEffects` refuse every item on a dead Pokémon ("It won't have any effect"). Platinum has no move
  that revives. Turning the mode on doesn't make already-fainted Pokémon dead.
- **No items in battle**: the battle bag (`BattleBag_Init`) keeps only the Poké Balls pocket, and its "last used
  item" shortcut only a ball.
- **Trainers' items**: `Trainer_Encounter` clears the trainers' bag items in the `FieldBattleDTO`, which the Gen 5
  bridge converts too; their Pokémon keep held items.
- **Crit guard**: `BattleSystem_CalcCriticalMulti` returns no critical hit (after its roll, so the RNG is unchanged),
  for both sides.
- **Nicknames**: after a catch in Platinum's engine, no "Give a nickname?" question: straight to the naming screen.
  The naming screen (`NAMING_SCREEN_TYPE_POKEMON`, with the mode on) refuses a blank or all-spaces name with a buzz and
  "You must name your new Pokémon!" in its prompt box; the Gen 5 bridge's naming screen after a BW2 catch is the same
  screen. **Not done**: the starter's (Sandgem lab) question is in its field script, still a yes/no; other Platinum
  gifts don't offer a name.
- **Whiteout**: `FieldTask_BlackOutFromBattle` calls `NuzlockeMode_Drop()` first, so the party is then healed as in a
  normal game (dead flags stay on the Pokémon but no longer count).
- **Tests** (pass on the desktop): the test command `party` also logs IVs, EXP and the dead flag; `faint SLOT` sets a
  party member's HP to 0 as a battle does. `nuzlocke_rules.txt` (from `nuz-normal`): Pokémon given with the mode on
  have IVs of 15; Turtwig at 0 HP is dead; a level 14 Bidoof (at Roark's cap) beats a wild one and gains no EXP; a
  Pokémon Center's nurse heals Bidoof and leaves Turtwig at 0 HP. `nuzlocke_whiteout.txt`: Turtwig faints to a wild
  Charizard in Platinum's engine (dead), the player whites out, the save is "Off, dropped" and refuses Normal. A
  switch after a faint was too timing-dependent to script (battle messages wait in real time).
- **Not checked yet**: the battle bag showing only balls, trainers' items, the crit guard, Rare Candy at the cap, the
  Hard (low) cap. All were built, none was seen working. (The forced naming screen and its refusal were seen in
  milestone 3's `nuzlocke_catch.txt`.)

## Milestone 3: the catch rule (done in Platinum's engine, 2026-11-13)

- **The books** are in the save's record, now version 2 (1028 bytes, still one per sector): up to 160 areas, each a
  location name (the map's label, as the old helper used) with its first encounter's species and result (in battle,
  caught, missed). Version 1 records (milestone 1 saves) read as version 2 with no areas; the header is read first,
  since a port save file can end right after a version 1 record.
- **When it starts**: the record's "has had Poke Balls" is set the first time the bag's Poke Balls pocket has
  anything, with Normal or Hard on (checked twice a second and at each wild battle). Encounters before that don't
  count.
- **Encounters** (`FieldTask_RunBattle`, `src/encounter.c`, so both engines): a wild battle (not trainers, links,
  the Frontier, the Pal Park or the catching tutorial) in a new area writes the area at once (in battle), so that
  stopping the game during it doesn't give it back; the end of the battle makes it caught or missed. A shiny never
  uses an area. Static encounters are the scripts' `Encounter_NewVsSpeciesAtLevel` and its fateful variant
  (legendaries, Snorlax-like Pokemon), marked with `NuzlockeMode_MarkStatic`: exempt with Normal, counted with Hard.
  Honey trees and roamers are wild encounters. Only the first wild Pokemon counts (a double wild battle beside a
  partner is judged by the first).
- **The throw** (`BattleScript_CatchMonTask`): a ball at a Pokemon the area doesn't allow (caught already, or not
  the species that was missed) takes the trainer path: the knock-back sound and animation, then "You've already used
  your catch for this area!" (`BattleMessage_PortPrintAscii`, new in `battle_system.c`) instead of "The trainer
  blocked the Ball!" and "Don't be a thief!". The ball is used up, as against a trainer. Shinies and exempt statics
  can always be caught.
- **The old helper is retired**: its settings are gone and `PortGame_Config_GetNuzlocke` reports it off, so its
  hooks and the Gear's display of its books do nothing. The Gear doesn't show the mode's books yet: its UI is
  port-common's (upstream), which this fork doesn't change; that waits for a port-common change.
- **BW2's engine** keeps the books (the hooks are around both engines); milestone 4 refuses the ball there too.
- **Tests** (pass on the desktop): `nuzlocke_catch.txt` from `nuz-normal`, in the fixture's area (location 1): Poke
  Balls start the rule; a Starly run from leaves the area "missed 396"; a Bidoof there is refused (the shot shows the
  message, and the Master Ball doesn't catch it); a second Starly is caught, the naming screen comes up without the
  question and refuses a blank name; the area is then "caught 396" and a third Starly is refused; a static Bidoof
  (the new test command `static SPECIES LEVEL`) is caught with Normal and refused with Hard. `nuzlocke state` logs
  "has had balls" and the current area's state. `nuzlocke_rules.txt` and `gen5_catch.txt` still pass.
- **Not checked**: a shiny (the test commands can't make one), a double wild battle beside a partner, the Safari
  Zone (Great Marsh: its balls are the Safari Balls, the same throw).

## Milestone 4: the rules in BW2's engine (done, 2026-11-13)

What the bridge (`src/port/gen5_battle.c`, `PortNuzlockeToGen5`) hands BW2's engine in `Gen5Battle` (`gen5/api/gen5.h`),
which `gen5/src/runtime.c` keeps for the patches (declared in `gen5/include/gen5/port.h`):
- **Level cap**: `levelCap`, the lower of Nuzlocke mode's cap and the *Hard level cap* setting's (which BW2's battles
  now apply too). `AddExpAndEVs` (`btl_server_flow_sub.c`'s patch) cuts each Pokémon's EXP at the cap's
  (`Gen5_PortCapExp`, from `PML_UtilGetPkmLvExp`).
- **Crit guard**: `critGuard`. `ServerEvent_CheckCritical` (`btl_server_flow.c`'s patch) throws away a rolled
  critical hit (after the roll, so BW2's random numbers go as before); moves that always land one (Storm Throw,
  Frost Breath: `PML_MoveIsAlwaysCrit`) still do.
- **Catch rule**: `wildCatchRefused` / `wild2CatchRefused`, from `NuzlockeMode_CatchAllowed` (the books were already
  kept around both engines, milestone 3). A ball at a refused wild Pokémon (matched by its personality) queues
  `SC_BALL_THROW_TRAINER`, BW2's knock-back, in a wild battle (`btl_server_flow_sub.c`'s patch); `btl_client.c`
  (newly patched: taken out of the unpatched list) shows `catchRefusedText` for it in a wild battle instead of the
  trainer's line, through `BtlvCore_PortStartMsgCatchRefused` (`btlv_core.c`'s patch).
- **Items**: with the mode on, the bridge passes only the Poké Balls pocket (the other pockets stay in the bag as
  they are). Trainers' items were already cleared in the `FieldBattleDTO` (milestone 2), which the bridge converts.
- **Already shared**: IVs (the parties are Platinum's), the dead flag (the bridge writes HP back through
  `Pokemon_SetValue`), the naming screen (the bridge's is Platinum's), whiteouts (the bridge's loss reaches
  `FieldTask_BlackOutFromBattle`).
- **Tests** (pass on the desktop): `nuzlocke_gen5.txt` from `nuz-normal` with Gen 5 battles on: a Starly run from
  marks the area; a Master Ball at a Bidoof is knocked back (the shots show "You've already used your catch / for
  this area!" in BW2's text box); a level 14 Bidoof at Roark's cap wins and keeps 2744 EXP; the log's "Gen 5: level
  cap 14, crit guard" shows what the battle got. `gen5_catch`, `gen5_trainer`, `gen5_loss` and `nuzlocke_catch` still
  pass.
- **Not checked**: the crit guard's effect (only that the battle has it), the balls-only bag on screen, a double wild
  battle's second Pokémon.

## Decisions

With the owner (2026-11-13):
- **The switch is in *Settings > Gameplay***, for the loaded save, at any point of a run (turning on, changing level,
  turning off). The warning pop-up shows there.
- **The mode replaces upstream's Nuzlocke helper**: it takes over the books and the Gear's display; the global
  toggle and its clauses go.
- **Static encounters** (legendaries, Snorlax-style Pokémon on the map): in **Hard** they are an area's encounter like
  any other (used up if first, refused if the area is used); in Normal they are exempt and can always be caught.
- **Trainers lose their bag items only** (Full Restores and the like); their Pokémon keep held items.

Taken as defaults, to change if the owner says otherwise:
- Poké Balls are the only items usable in battle.
- Rare Candies (and other level-raising items) don't work past the cap, like EXP.
- Gift Pokémon that offer a nickname (the starter among them) must be named too, not only caught ones.

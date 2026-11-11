# Gen 5 screens: the Summary, the PC and the party (plan)

Working notes for replacing Platinum's Pokémon Summary, PC (box) and party screens with Black 2 and White 2's own,
the way the Gen 5 battles replace Platinum's battles (`gen5-battles.md`: read it first for the `gen5/` layer, the
renamed `bw2_` symbols, the ROM import and widescreen). Fork-only for the same reason (pokebw2 is GPLv3).

The owner's decisions: **the Summary first, then the PC, then the party**; Gen 4's PC and Summary get **no widescreen
work of their own**: the Gen 5 screens are how they get a wide-capable UI (BW2's 256x192 screens, centred in the 16:9
top panel with their backgrounds continued across the sides, as the Gen 5 battle and the party screen's panels are).
Setting: **Settings > Gameplay > Gen 5 screens** (`gen5Screens`, off by default, offered once a BW2 ROM is imported:
one switch for all three screens as they arrive; while one isn't done yet the game's own opens).

## Where BW2's screens are (pokebw2, `src/ovNNN`)

| Screen | Overlay | Files | Lines | Entry |
| --- | --- | --- | --- | --- |
| Summary | 207 | `p_status.c` (the proc), `p_sta_sys.c`, `p_sta_info.c`, `p_sta_skill.c`, `p_sta_ribbon.c`, `p_sta_sub.c`, `p_sta_oam.c`, `ribbon.c` | 5,400 | `PSTATUS_PROC_FUNCTIONS` with a `PStatusParam` (`include/app/p_status.h`) |
| PC box | 255 | `box2.c` (proc), `box2_main.c`, `box2_seq.c`, `box2_ui.c`, `box2_bmp.c`, `box2_bgwfrm.c`, `box2_obj.c`, `box_search.c`, `box_search_graphic.c` | 21,100 | `BOX2_PROC_FUNCTIONS` with a `Box2Param` (`include/app/box2.h`) |
| Party | 165 | `pokelist.c` (proc), `plist_sys.c`, `plist_plate.c`, `plist_menu.c`, `plist_message.c`, `plist_item.c`, `plist_battle.c`, `plist_demo.c`, `status_rcv.c` | 6,700 | `EventPokeList_Create` (`ov020/event_poke_status.c`) runs it with a `PokeListParam` |

All three are ordinary `GFL_PROC`s on the same GFL, the same Pokémon library (`pml/poke_party.h`) and the same
printing and sprite systems the battle already uses, so they run the way the battle does (`Gen5_BattleFrame`).

## What they need beyond what `gen5/` builds

- **Summary: almost nothing.** Compiled against the port's headers, the eight files leave five unresolved symbols:
  `GameData_Create`, `GameData_Free`, `GameData_IsShortcutRegistered`, `GameData_SetKeyItemRegistration` (the port's
  `game_data.c`: one `GameData`, no shortcut menu) and `pokerus.c` (a small system file). Everything else (MCSS for the
  3D-in-2D Pokémon, the HP gauge, `app_menu_common`, `pokegra`, message data, fonts) is built for the battle already.
- **PC box:** the box's own UI framework (`app/ui/*`: `frame_list`, `touchbar`, `ui_scene`, `print_msg`), `name_entry`
  (its box rename), the Bag app (giving items), `app_taskmenu`, `bmp_oam`, the box save (`system/box_savedata.c`, built
  already) and the Dream World save. Not measured yet: do it as the Summary's was (compile the overlay with the
  `gen5/` flags and list the undefined symbols; `Docker` run in the notes of this branch).
- **Party:** `status_rcv.c` (an item's effect on a Pokémon) and the item and field-move logic reach into BW2's field:
  `PlayerActionPossibilities`, zones, Join Avenue, the musical, hidden events, the trainer card and shortcut saves.
  These need stand-ins like `gen5/src/field.c`'s. Not measured.

## What comes from the BW2 ROM

All already extracted by the import (`gen5/a/...`): the Summary reads three archives (`ARCID_P_STATUS`, `ARCID_FONT`,
`ARCID_SYSTEM_MESSAGE`: its graphics, the font and the message tables); the PC adds `ARCID_BOX2`, `ARCID_POKEICON`,
`ARCID_ITEMGRA` and `ARCID_POKEDEX_DATA`; the party `ARCID_PLIST` and the icons. Pokémon names, moves, items and the
rest are in the data the battle loads. Nothing new to extract; nothing is committed.

## How Platinum's data goes in and out

The battle bridge's `Gen5Pokemon` (`gen5/api/gen5.h`) is what both games keep; `gen5/src/runtime.c` makes BW2's
`PartyPkm` from it and `src/port/gen5_battle.c` fills it from a Platinum `Pokemon` (`Gen5_PokemonToGen5`, made
non-static for this). The Summary reuses both, unchanged.

- **Summary (done, below):** a view. What leaves it is only which Pokémon was last shown and, for the move-forgetting
  mode, the slot chosen. Markings (changed on the Summary's marking page) are not copied back yet.
- **PC:** the hard one. BW2's box app edits BW2's box save (24 trays of 30), moves Pokémon between box and party,
  releases, renames boxes, sets wallpapers, gives and takes held items, changes markings. Converting Platinum's 18
  boxes into BW2's save and back through `Gen5Pokemon` would lose what it doesn't carry (ribbons, contest stats,
  Platinum's met data, Pokérus, forms...). Plan: keep Platinum's `BoxPokemon` data as the master. Convert each Pokémon
  into BW2's format tagged with its place, run BW2's app, then replay the *differences* (a Pokémon's new place, a
  release, a swapped item, a changed marking or nickname) onto Platinum's data by tag, instead of converting back.
  Wallpapers and tray names map to BW2's tables. Eggs and the party/box moves need the same tag handling.
- **Party:** BW2's party list is also the item-use screen and the field-move menu (Fly, Cut, Surf, Sweet Scent...),
  where Platinum's party menu (`src/applications/party_menu`, `callbacks.c`, `context_menu.c`) drives its own field-move
  context (`FieldMoveContext`), item effects (`PartyMenu` modes), evolutions and form changes. Replacing it whole means
  re-implementing those callbacks against BW2's screen; replacing only the *picture* (BW2's plates and layout, driven
  by Platinum's logic) means patching `plist_*` drawing, which is a different (larger) kind of change. Decide after the
  PC, when the PC has shown how much of BW2's logic can be kept.

## How the screens are entered in Platinum

- **Summary:** every caller goes through `gPokemonSummaryScreenApp` (`src/applications/pokemon_summary_screen/main.c`):
  the party menu (`FieldSystem_OpenSummaryScreen`, `FieldSystem_StartChildProcess`), the PC, the evolution (to pick a
  move to forget), move reminders and tutors, the Battle Frontier, trades, the GTS, the Poffin case. One template, so
  one hook: `main.c`'s template is built from `PortSummary_Init/Main/Exit` (under `GEN5_BATTLES`), which run BW2's
  screen when `Gen5Summary_Begin` accepts the request and Platinum's own otherwise. The args are `PokemonSummary`
  (`dataType` mon, party or box mon; `mode`; `monIndex`, `monMax`; `move`).
- **PC:** `FieldSystem_OpenPokemonStorage` (`src/unk_0203D1B8.c`) runs `BoxAppMan_*` with a `PokemonStorageSession`
  (from the PC terminal script, `ScrCmd`). One entry: the hook goes there.
- **Party:** many entries (`FieldSystem_OpenPartyMenu*`, the start menu's POKéMON, items, scripts) through
  `gPokemonPartyAppTemplate` with a `PartyMenu` (`mode`, `type`, results in `selectedMonSlot`,
  `menuSelectionResult`, `learnedMove`...). Modes would be taken over one at a time, the plain field menu first.

## Touch and input

The Gen 5 code reads the keys and touch the port gives it (`Gen5_SetInput`, as in the battle: `key.c` and
`touchpanel.c` patched to read them). BW2's screens are touch-first with a button fallback (`GCTX_HIDIsUsingTouch`);
the Thor's buttons and its touch work through the same path as in the battle. Nothing to add for the Summary.

## Widescreen

None done yet. The plan is as for Platinum's party screen: the scene wide while the screen runs, the 2D layers'
edge columns continued (`SIM_SetWidescreenBgEdgeExtend`) or mirrored (`SIM_SetWidescreenBgEdgeMirror`) across the sides,
the OBJ edge wrap for sprites that touch the edges. BW2's Summary draws the Pokémon as an MCSS sprite (3D) over its
2D layers, which the battle's widened projection (`Gen5_Widen`) already handles. The Summary is the Gen 5 screen with
the most 2D BG detail (the page background); look at it on the Thor first.

## Risks

- **The PC's data round trip** (above): the biggest. Losing a Pokémon's data on a save is the worst outcome in the
  project; the replay-the-differences design and a test that saves, runs the box and compares every box Pokémon byte
  for byte before and after are the guard.
- **Size of the PC** (21,000 lines, UI framework): more 64-bit and DS-specific surprises than the 5,000-line Summary.
- **The party's tie to the field:** BW2's party list assumes its field; stand-ins are a growing list.
- **Two games' screens in one**: Platinum's field task state (`FieldSystem_StartChildProcess`, the party menu's cursor,
  `monIndex` handoff) must see the same results as from its own screens; found case by case.
- **Text:** BW2's messages are its own English text: names of moves, items and abilities are Gen 5's (a few differ
  from Platinum's). Platinum's nicknames and OT names go through the ASCII conversion the battle uses (Japanese
  characters, ♂ and ♀ left out for now).

## Milestones

1. **Summary** (in progress): BW2's Summary opened from Platinum's party menu, the PC and the evolution's move choice.
2. **PC.**
3. **Party.**
4. **Widescreen for the three** (the Summary's first).

## Summary: where it stands (2026-11-10)

Built: `gen5/meson.build` has overlay 207's eight files and `pokerus.c`; `gen5/src/game_data.c` the four `GameData`
calls; `gen5/src/runtime.c` `Gen5_SummaryBegin`/`Gen5_SummaryEnd` (`gen5/api/gen5.h`'s `Gen5Summary`: the party, the
index, the mode, the result), running `PSTATUS_PROC_FUNCTIONS` on the battle's systems, frame and VBlank
(`Gen5_BattleFrame`, `Gen5_BattleVBlank`); `src/port/gen5_summary.c` and the hook in `main.c`; the setting
(`gen5Screens`, `sim_config_prj`, `gui_settings.cpp`); the test command `gen5-screens on|off` and `app summary`;
`tests/scripts/gen5_summary.txt`.

Taken: a Pokémon or a party to look at, and the move-forgetting choice. Left to Platinum's: Poffin feeding, the contest
condition page's mode, box Pokémon (until the PC is done), locked moves.

Result (desktop, `gen5_summary.txt` passes): BW2's Summary opens from `app summary` and draws correctly: the info page (nature, memo, dex number, OT, ID, experience, ball, HP/EXP bars) and, with RIGHT, the stats and moves pages (Tackle 35/35 PP, Overgrow's text, the HP bar) from Platinum's Turtwig, in BW2's English text. **Known problem: the Pokémon's own picture (MCSS sprite, bottom right) is garbled** (green noise in the right shape); the battle's sprites are fine, so it is how the Summary loads the sprite (a different archive or texture layout in `ARCID_P_STATUS`/pokegra: next thing to look at). Not yet played through: the party menu's SUMMARY entry, the evolution's forget-move choice, B/X leaving.

Not done: markings written back, widescreen, the Thor, ribbons from Platinum's data (Platinum's ribbon bits aren't
mapped to BW2's), Platinum's contest and condition data, the Pokémon's cry on opening (BW2's, if the sound works).

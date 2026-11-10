# Gen 5 battles: plan

Working notes for an optional Gen 5 battle engine in this port: Black 2 and White 2's battle engine and battle
view (3D arenas, the moving camera, animated sprites, Gen 5 move animations and rules), run on the battles the
game sets up, with the player's own Black 2 or White 2 ROM supplying the Gen 5 files. This is a fork-only
project for now (see Licensing).

## Decisions

- **Code: [pokebw2](https://github.com/fuddlesworth/pokebw2)**, the Black 2/White 2 decompilation. Every function is
  C and matches the ROM, including the battle engine (`src/ov167`: `btl_main`, `btl_server_flow`, `btl_calc`,
  the AI) and the battle view (`src/ov168`: `btlv_camera`, `btlv_bg`, `btlv_effvm` for move animations,
  `btlv_clact`). Black/White aren't decompiled, so **the ROM is Black 2 or White 2 (USA)**, not Black/White.
  (Platinum Reforged does the same on the DS, but its game code isn't published, and it ships as DS code, which
  this port can't run.)
- **Games: vanilla Platinum first.** BW2's engine has 17 types (no Fairy) and its own Pokémon, move and item data.
  Renegade Platinum support (its data and Fairy) is a later phase.
- **Licensing: pokebw2 is GPLv3, this repository MIT.** Built with pokebw2's code, the app is GPLv3. Fine in this
  fork (its source is public); the main repository's developer would have to agree before anything goes upstream.
- **Testing: a local Linux desktop build** (Docker), not only cloud builds and the Thor.
- **Out of scope:** following Pokémon, visible wild Pokémon, BW2 Phenomena, name capitalisation.
- **Setting:** *Settings > Gameplay > Gen 5 battles*, off by default, only offered once a BW2 ROM is added.

## How it fits together

Platinum starts every battle in one place: `FieldSystem_StartBattleProcess(fieldSystem, dto)`
(`src/unk_0203D1B8.c`) runs `gBattleApplicationTemplate` with a `FieldBattleDTO` (parties, trainers, terrain,
weather, time, bag, Pokédex...) and reads the results back from the same DTO afterwards. With Gen 5 battles on,
it runs a **Gen 5 battle application** instead:

1. **Setup:** convert the `FieldBattleDTO` into BW2's `BtlSetup` (`include/battle/btl_setup.h`): parties
   (Gen 4 Pokémon to Gen 5 `POKEMON_PARAM`: the formats are close, and species, moves, abilities and items keep
   their Gen 4 IDs in Gen 5), trainers and their AI, battle type and style, the field situation (terrain to a BW2
   background, weather, time of day), and BW2-shaped stand-ins for the bag, Pokédex, config and records it reads.
2. **Run:** BW2's battle process (`GFL_PROC`) under BW2's own systems (GFL heaps, tasks, graphics, sound, files)
   until it ends. On the DS the two games never share the machine; here the battle borrows the emulated DS from
   Platinum the way any application does, and hands it back.
3. **Results:** copy them back into the DTO, the way Platinum's battle leaves them: party HP, status, PP,
   experience and levels (and pending evolutions), a caught Pokémon, money, items used, Pokédex seen and caught,
   the result (win, loss, run, caught).

### Building BW2's code into the app

- **What comes in:** the battle overlays (`ov167`, `ov168`, `ov169`: about 74,000 lines), BW2's GFL library
  (`src/gfl`, about 20,000), the parts of `src/system` the battle uses, and stubs for the field and network calls
  it makes. BW2's copies of the DS SDK (`lib/nitro`, `lib/nnsys`, `lib/twl`) are not used: libntr provides the SDK,
  with what BW2 needs from the DSi SDK added where missing.
- **Names:** BW2's code is compiled into its own library and its symbols are renamed with a prefix (objcopy), so
  BW2's `Heap_*`, `Sound_*` and the like can't collide with Platinum's.
- **Files:** BW2's ROM is extracted next to Platinum's, and the Gen 5 code's file paths resolve to it.
- **64 bits:** pokebw2 was written for the DS's 32-bit ARM. First checks are good: no assembly in the battle code
  (three small functions elsewhere, to be rewritten in C) and almost no pointers stored in 32-bit integers. The
  remaining issues will show at runtime, which the desktop build is for.
- **Generated headers:** BW2's constants (species, moves, abilities...) and text IDs are generated from pokebw2's
  committed lists, by its own scripts, at build time (`gen5/tools/gen_headers.py`); no ROM is needed.

## The BW2 ROM

*Settings > Games > Gen 5 battles > Add Black 2 or White 2*: choose the `.nds`; the app checks it (game code
`IREO`/`IRDO`, SHA1 of pokebw2's table) and extracts the files the battle uses. Nothing from it is committed or
redistributed. Done in milestone 2; see "Milestone 2" below.

## Milestones

1. **Builds:** BW2's battle code and GFL compile into the desktop and Android builds, unused. *Done on the desktop.*
2. **ROM:** the import screen extracts a BW2 ROM; the Gen 5 code reads files from it. *Done on the desktop.*
3. **First battle:** a debug menu entry starts one wild single battle (a fixed Pokémon against the first party
   Pokémon) in BW2's engine and returns to the overworld. Arena, camera, sprites, moves, running away. *Done on the
   desktop, but for the arena's distant background* (see "Milestone 3"; the sound came later, see "Sound").
4. **Bridge:** real wild battles through the setting, with results written back: HP, experience, levels, catching
   (Pokédex entry, nickname, box), fainting and whiting out. *In progress:* 4a (the Pokémon and the player, in and
   out), 4b (the setting, real wild encounters) and 4c (catching, the bag) are done on the desktop; see "Milestone 4".
5. **Trainers:** single trainer battles: trainer sprites, AI, prize money, win and lose messages. *In progress:* a
   single trainer battle runs in BW2's engine with Platinum's AI level, the trainer's name, class and defeat line,
   and the prize money (see "Milestone 5"); still to come: evolutions, new moves and items after it, and the scripted
   trainers' own texts.
6. **Widescreen:** with *Settings > Display > Widescreen* on, BW2's battle fills the Thor's 16:9 top panel (it was
   4:3, seen on the Thor). Unlike Platinum's battles, whose backdrop is a 256-pixel picture (they stay 4:3), BW2's
   arena is 3D: its camera is widened as the field camera is (`src/port/widescreen.c`), and the sky and ground
   meshes reach past the sides (whole since libntr clips polygons). The Pokémon and trainer sprites, gauges, message
   window and effects stay where they are, centered in the 4:3 middle. *Done on the desktop* (see "Milestone 6"); the
   overworld's weather went wide with it.
7. **The rest:** doubles, tag battles (Cheryl, Riley, Buck, Barry...: the partner's back sprite is Platinum's
   sprite of that trainer for now; the owner will supply Gen 5 style back sprites for them later), multi battles, the catching tutorial, the Safari Zone,
   Pal Park, legendaries and roamers, the Battle Frontier; items from the bag; weather and terrain arenas.
8. **Renegade Platinum:** the game's own Pokémon, move and item data in the Gen 5 engine, and Fairy.
9. **HeartGold and SoulSilver:** the same app for the owner's HGSS ROM (from the HGSS decompilation, as this port
   is from Platinum's), with the same Gen 5 battles: wild and trainer battles, catching, the AI and its levels,
   widescreen. After milestone 8, and once widescreen works with every trainer and its dialogue.

### The trainer AI

Platinum gives each trainer an AI level: flags (`TrainerHeader.aiMask`) choosing which of the AI's scripts run,
from the basic one (no useless moves) to the expert's (type matchups, setup, switching) and the special ones for
weather, Baton Pass, double battles, harassment. BW2's AI (`src/ov170/tr_ai.c`) is Platinum's, reworked for Gen 5,
with its scripts in the same order, so **a trainer keeps the level Platinum gave it**: the port passes Platinum's
flags to BW2 (`Gen5_TrainerAIFlags`, `gen5/src/trainers.c`), and BW2's own trainers' flags are never used. The flags
pass through a table per game, so Renegade Platinum (whose trainers are harder and may set more flags) and HGSS
(Platinum's numbering) each get their own if theirs differ, and the AI's scripts stay BW2's.

## Milestone 1: where it stands (2026-11-08)

Done on the desktop build; the Android build is checked by compiling for arm64, not yet built with the NDK.

- **pokebw2's version:** built and patched against commit `2c3961614d` (2026-11-08). The patches apply to that
  commit; after updating the clone, a patch that no longer applies fails the build and needs remaking
  (`gen5/tools/make_patch.py`).
- **Building it:** `-Dpokebw2=../pokebw2` (`tools/docker/desktop.sh build` sets it when the clone is there) builds
  `gen5/libgen5_battle.a` and, from it, `libgen5_battle_bw2.a`, which the game will link. Not linked into the game yet.
- **What's in it:** 207 of pokebw2's files, the battle's whole closure: overlays 167-170 (engine, view, tables, trainer
  AI), 285-289 (the battle's bag, party list, scenario and choice screens), 41 GFL files, 8 SPL files (pokebw2's
  own build of Nintendo's particle library) and 93 of `src/system`. The list is in `gen5/meson.build`.
- **pokebw2 is never changed.** In `gen5/`: `include/` comes first on the quote path with `types.h` (pokebw2's `u32`
  is `unsigned long`, 64 bits here), the SDK header names and `gfl/overlay.h` (overlays are numbers);
  `include/gen5/port.h`, included first in every file, has what libntr's SDK 4 and NitroSystem lack or name otherwise;
  `patches/` has five small source changes, applied to copies at build time (the assembly of `arc_tool.c` in C,
  libntr's `PAD_Read`, three name differences); `src/` has the port's own files. pokebw2's headers are on the quote
  path only (`-iquote`), so its copies of MSL's `string.h` and the like never replace the host's; its DWC, NitroWiFi
  and TwlSDK headers are there for their declarations, but those libraries aren't built.
- **The boundary:** BW2's field, save and network systems stay out (`game_data.c`, `game_comm.c`, `game_beacon.c`,
  `save_control.c`, `net_define.c`...). What the battle calls in them is in `gen5/src/`: `game_data.c` (GameData and
  SaveControl: the party, bag, box, records... which milestone 4 answers from Platinum's save; for now they return
  nothing), `field.c` (beacons, Festival Missions, network devices, IR check, seasons: none), `net.c` (no network
  errors), `gfl_overlay.c`, `sdk.c`.
- **Names:** every symbol the library defines becomes `bw2_<name>` (`tools/prefix_symbols.py`, objcopy), except the
  port's interface to Platinum, which will be named `Gen5_...`. Without it 48 would clash: 6 with Platinum's game
  (`PlayTime_*`, `SaveData_Load`, `SaveData_Erase`) and 42 with its libraries (Platinum's SPL, three SDK internals).
- **Checks** (`tools/docker/desktop.sh gen5-check`): `tools/link_check.sh` lists what neither the library, libntr's
  libraries nor the C library define: nothing. libntr has every SDK and NitroSystem function the battle calls.
  `tools/arm64_check.py` compiles the 209 files with an arm64 compiler and Android's SDK defines (`SDK_AARCH64`): all
  compile. `-fsigned-char`, since BW2 was built with a signed `char` and arm64's isn't.
- **64 bits:** the arm64 check lists 88 places in BW2's code where a pointer goes through a 32-bit integer or back
  (Pokémon data, move handlers, gauges, bitmaps...). Some are harmless (alignment tests); the rest are the first
  suspects when the battle runs (milestone 3, which fixed those the battle runs).
- **Left for milestones 1 and 2:** an NDK build of the APK with `-Dpokebw2` (`tools/android/build_apk.sh` now gives meson
  the NDK's `llvm-nm` and `llvm-objcopy`; the CI would need a pokebw2 clone at a pinned commit).

## Milestone 2: the ROM (2026-11-08)

- **Import:** *Settings > Games > Gen 5 battles* (`src/port/gui_settings.cpp`, with `GEN5_BATTLES`) picks the
  `.nds` with libntr's file picker (on Android a copy, deleted afterwards). `src/port/gen5_rom.cpp`, on a thread of its
  own, checks the game code and the SHA1 against pokebw2's two dumps (Black 2 `e51e6dfb...`, White 2 `b5d7490b...`;
  anything else, patched ones included, is refused with the reason), then reads the ROM's file system (FNT and FAT)
  and extracts its archives (`a/`, 308 files) and sound data (`swan_sound_data.sdat`): 270 MB of the 512 MB ROM, in
  about 3 s on a desktop computer. Download Play's and Wi-Fi's files are left out. They go into `gen5.tmp/`, which becomes the
  data folder's `gen5/` once complete, with `rom.txt` (the game and the self-check's result). The ROM isn't kept.
- **Shared by every game:** `gen5/` is in the data folder, while a game added through *Add a game* runs in
  `carts/<id>/`, and libntr opens files by paths relative to the current game's folder. `Gen5_FilesInit`
  (`gen5/src/files.c`) gives BW2's GFL its archive table (`GFL_ArcSysInit`) with paths from there to `gen5/`
  (`gen5/a/0/1/6`, or `../../gen5/a/0/1/6`).
- **The self-check:** after an import, `Gen5_SelfCheck` reads Bulbasaur's entry of BW2's Pokémon data (`ARCID_PERSONAL`)
  through BW2's own archive code (`GFL_ArcSysGetDataLength`, `GFL_ArcSysRead`, the C version of `ArchiveMoveImageTop`)
  and checks his base stats; the settings show the result. A failed check deletes the files. libntr asks for a ROM
  when a file it opens is missing, so the Gen 5 code checks that a file exists before reading it.
- **Linked:** the game now links the Gen 5 code whole: `gen5_battle_bw2.o`, one relocatable object (`ld -r`) of
  every renamed BW2 symbol (5263), rather than a library, which meson won't link into the desktop's native `main` and
  which would only bring in what is called. So the game's own link checks that everything BW2's code calls exists.
  The interface is `gen5/api/gen5.h`; its functions are named `Gen5_...`, which the renaming leaves alone.
- **Tests:** `tests/scripts/gen5_rom.txt` imports `~/thor-dev/PKMNB2.nds` (`GEN5_ROM=...` for another) with the
  test command `gen5-import`, as the settings do, and passes on the self-check. A Platinum ROM is refused (game code
  `CPUE`). The settings import was also tried on the desktop, with the ImGui file picker.
- **Checked on the Thor** (milestone 3's "On the Thor"): the Android build and the import, where 270 MB more goes
  into the app's folder.

## Milestone 3: the first battle (2026-11-08)

Done on the desktop: *Settings > Advanced > Gen 5 battles > Wild battle in the Gen 5 engine* starts a battle against a
level 5 Bidoof in BW2's engine with the first party Pokémon. The intro, the camera, both Pokémon and the trainer, the
gauges, the messages, the touch screen's menus, moves (their animations and damage) and running away all work, and the
game is back on the overworld afterwards. `tests/scripts/gen5_battle.txt` plays it: Tackle, then RUN until it works.

- **How it runs:** the debug entry asks for the next battle in BW2's engine (`Gen5Battle_StartDebug`,
  `src/port/gen5_battle.c`) and starts an ordinary Platinum wild battle; `FieldSystem_StartBattleProcess` then runs
  `gGen5BattleApplicationTemplate` instead of Platinum's battle. Its Init builds the `Gen5WildBattle` (species and
  levels) from the DTO, Main runs a frame of BW2 (`Gen5_BattleFrame`), Exit maps the result back (no other result is
  written yet: milestone 4). BW2's VBlank work runs in the game's VBlank callback (`Gen5_BattleVBlank`).
- **BW2's systems** (`gen5/src/runtime.c`) start the first time and stay up: GFL's heaps in a 24 MB block of their
  own (`GFLAppInit` takes them from `Gen5_MemInit`, `gfl_use.c`'s patch), the archives, text, species names, Pokémon
  data, zone data. Each battle gets a heap (512 KB) for its `BtlSetup`, parties and the stand-ins for the player's
  info, config, bag, Pokédex and records, freed at the end (BW2 checks that a deleted heap is empty).
- **The save** (`gen5/src/game_data.c`): `SaveControl_GetBlockPtr` hands out the 17 blocks of BW2's save the battle
  reads (Chatot's cry, the box, trainer data...), each created and initialized from `save_table.c`'s definitions the
  first time; nothing is written back.
- **Input:** the keys and touch Platinum read each frame (with the test scripts' own) go to BW2 through
  `Gen5_SetInput`: `key.c` and `touchpanel.c` read them instead of the hardware. The command selection raises
  `GEN5_EVENT_COMMAND` (`Gen5_TakeEvents`), which the test scripts' `command` event waits for.
- **Sound** was silent at first (`gen5/src/sound.c` kept only the music fades' timing); BW2's sound effects and cries
  play since 2026-11-09, see "Sound".
- **What 64 bits and libntr needed** (each found by running the battle):
  - libntr: `G3_MultMtx33`/`44`, `G3_MultTransMtx33` and `G3_Shininess` had no simulator path and wrote the
    matrices into the registers, over `GXSTAT` (the geometry engine then looked busy forever). Fixed in libntr
    (`subprojects/packagefiles/libntr_g3_mult.patch`, the wrap's `diff_files`; for libntr itself later). libntr's
    `VCOUNT` stays 0: the battle's VBlank work runs with it at 192, the line BW2's sprite loads wait for.
  - NitroSystem files: libntrsystem rebuilds cells and animations in the 64-bit layout but not multi-cells (NMCR),
    which BW2's Pokémon sprites use: `gen5/src/g2d.c` (`arc_util.c`'s patch).
  - Pointers through 32-bit values: the battle's event variables (a handler's string), animation callbacks' `u32`
    parameter, and the like get a handle from `gen5/src/pointers.c`; address arithmetic uses `uptr` (gauges, touch
    screen, particle work); the effect VM's call stack holds `uptr` return addresses. The arm64 check's list went
    from 88 to 33 places, all harmless (alignment tests, pointer differences) or in code the battle doesn't run (Join
    Avenue, Wi-Fi, trade evolutions).
  - The particle library's structures are larger on 64 bits: the effects' particle work is twice the DS's, and its
    allocator says when it runs out instead of writing past the end (that overwrote the send-out's effect script).
  - What the DS tolerates: reading through NULL (a cry's channel that never started, `pm_voice.c`), `exit` as the
    debugger's stop when a heap is deleted with blocks left (`heap.c`: listed and carried on).
  - Sprites' texture coordinates were written to the `TEXCOORD` register, which the simulator doesn't read: as
    commands (`mcss.c`).
- **Debugging it:** gdb can't run under Rosetta, so test runs print backtraces themselves (`src/port/test_crash.cpp`):
  for a crash (on a stack of its own, with the faulting address when the unwinder can't go further), for an exit the
  script didn't ask for, and, when the harness stops a run at its time limit, three samples of where the game thread
  is. `desktop.sh test` turns them into functions and lines.
- **Not done yet:**
  - ~~The arena's distant background~~ (fixed): libntr's renderer had no polygon clipping (its clipper's segment code
    was commented out) and dropped polygons that came out far off screen, so BW2's large sky and ground meshes, which
    reach behind the camera, left black patches when the camera was close. Platinum's scenes don't have such
    polygons. `subprojects/packagefiles/libntr_g3_clip.patch` clips each polygon in clip space before the divide by
    W, as the DS's geometry engine does (and melonDS): against -w <= x, y, z <= w, adding the points where its edges
    cross a plane with the texture coordinates and colours interpolated there, then draws the result as a fan of
    triangles. The ±50 off-screen hack and the truncation of W to 24 bits before the divide are gone; the box test
    uses the same clipper. `gen5_battle.txt` now shows the whole sky and ground; in Platinum's tests the only
    visible change is the starter briefcase's lid, which reaches past the top of the screen and is now drawn in a
    consistent perspective (its texture's lines no longer bend), and stray edge pixels on the overworld. A checkout
    that already has `subprojects/libntr` needs it fetched again for the patch (delete it, or
    `meson subprojects update --reset libntr`). Not yet: the far plane's "draw or drop" bit of the polygon attributes
    (the DS drops a polygon that crosses the far plane unless bit 12 is set; libntr clips it).
  - ~~Sound~~ (done, see "Sound"). A block of 0 bytes is left in one of BW2's heaps at the end (listed, harmless).
  - ~~The Android build~~ (done, below).
  - The player's name in BW2's messages: the stand-in `PlayerInfo` has none ("??????????? is out of usable
    Pokémon"). Milestone 4.
- **On the Thor (2026-11-08):** the first Android build with the Gen 5 code (NDK r27c, `-Dpokebw2`; one fix: the
  library is compiled with `SDL_MAIN_HANDLED`, since Android's SDL_main.h renames `main`, which `system/main.h`
  declares). On the device: a new game to the starter, Barry's battle in Platinum's engine, the ROM import in the
  settings, and a wild battle in BW2's engine all work. Losing it crashed:
  - `CalcLossMoney` reads the money from the trainer card, which the port's `getTrainerCardDataBlkAddress` returned
    as NULL. It is the trainer info block of the port's save now, as in BW2 (0 money until milestone 4).
  - Then the screen stayed black: the debug battle starts a scripted encounter (`Encounter_NewVsSpeciesAtLevel`),
    which leaves the map closed after a loss for its script to black out, and the debug battle has no script. It
    blacks out itself now (`src/port/gui_createmon_utils.c`; Platinum's debug battles had the same problem).
  - `tests/scripts/gen5_loss.txt` loses to a level 100 Charizard and checks the player is back home.

## Milestone 4: the bridge (in progress)

In slices, each tested on the desktop:

- **4a, the Pokémon and the player (2026-11-08, done on the desktop):** the debug battle hands BW2 the whole party
  and the wild Pokémon as they are in Platinum, and writes back what the battle changed.
  - `gen5/api/gen5.h`'s `Gen5Pokemon` and `Gen5Player` carry what both games keep, in neither game's format (their
    headers can't meet): species, form, level, experience, PID, OT ID and name, item, ability, friendship, gender,
    moves, PP and PP Ups, HP, status, IVs, EVs, ball, Pokérus, nickname. `src/port/gen5_battle.c` fills them from
    the battle's `FieldBattleDTO`; `gen5/src/runtime.c` makes BW2's `PartyPkm`s from them (the PID first, as it is
    the encryption key; the nature from the PID, the origin game Platinum, so the ability slot is the PID's), and BW2
    computes the stats.
  - After the battle: experience and levels, moves and PP, EVs, HP, status, friendship, held item and Pokérus go back
    into the DTO's party, which Platinum copies into the save; `leveledUpMonsMask` gets the Pokémon that gained a
    level (Platinum evolves them after the battle), and the money a loss costs or a win gives
    (`BtlSetup.moneyChange`) goes to the player's trainer info.
  - Status: BW2 numbers the major conditions (1 paralysis to 5 poison) where Platinum has bits. A sleeping Pokémon
    keeps its sleep turns, and a badly poisoned one stays so while poisoned.
  - The player's name, ID and gender go into BW2's `PlayerInfo`: BW2 gives 1.5 times the experience to a Pokémon
    whose OT isn't the player, which every Pokémon was with an empty one. The money is in the trainer info save
    block (a loss can't cost more than the player has), the badges in `BtlSetup.badgeCount`.
  - Names: Platinum's characters to Unicode through the port's ASCII table, and é; other characters (Japanese,
    ♂ and ♀) are left out for now. A Pokémon without a nickname shows BW2's name for its species.
  - **Pass Powers:** BW2's `main.c` calls `PassPower_Init`, which the port didn't. Its "no power" isn't 0, so every
    power was on at 0 %: no experience, no prize money, and the capture rate times 0. `Start` calls it now.
  - Tests: `gen5_win.txt` (a level 1 Pichu: Turtwig's experience goes from 204 to 208), `gen5_loss.txt` (the
    money a loss costs, 48), `gen5_battle.txt` (running away). Their logs have a line per Pokémon after the battle.
- **4b, the setting (2026-11-08, done on the desktop):** *Settings > Gameplay > Gen 5 battles* (off by default,
  `Gen5Battles` in the game's config, offered once a ROM is imported) sends single wild battles to BW2:
  `Gen5Battle_Wants` (`FieldSystem_StartBattleProcess`) takes a `BATTLE_TYPE_WILD_MON` battle with no status flag but
  a honey tree's, so grass, caves, water and honey trees; not the scripted ones (legendaries, the first battle, the
  catching tutorial, Giratina) nor roamers, doubles, partners, the Safari Zone or Pal Park, which stay in Platinum's
  engine until milestone 7. The debug entry's request still works without the setting.
  - After the battle, the Gen 5 application does what Platinum's does at its end (`battle_main.c`): the Pokémon that
    gained a level evolve (`Battle_FindEvolvingPartyMember`, `Evolution_Begin`). Not seen yet: no test levels a
    Pokémon to its evolution.
  - Tests: `gen5_grass.txt` walks the tall grass at Lake Verity (`pace UP DOWN until battle`) until a wild Pokémon
    appears, from the fixture `verity-grass` that `lake_grass.txt` makes (from `turtwig`: Route 201 west to Verity
    Lakefront, the gap right of the sign, Cyrus's scene; Barry keeps the player out of the grass east of Twinleaf
    for now). `gen5_wild.txt` starts the field's own wild encounter with the test command `wild SPECIES LEVEL`, so
    it doesn't depend on the map. The test command `gen5-battles on|off` sets the setting.
- **4c, catching and the bag (2026-11-08, done on the desktop):**
  - The bag: the player's Items, Medicine, Poké Balls, Berries and Battle Items pockets go into BW2's bag
    (`Gen5WildBattle.items`, `BagSave_AddItem`; the same item numbers), and what the battle used comes out of the
    DTO's bag, which Platinum saves. Items BW2 doesn't have stay out of its bag and keep their count.
  - A caught Pokémon (`BTL_RESULT_CAPTURE`, `BtlSetup.capturedSlot` in the wild party) comes back as the DTO's wild
    Pokémon (Platinum's own data, intact) with what the battle changed and the ball it was caught in. Then, as
    Platinum's battle does after a catch: `Pokemon_SetCatchData` (the player as its trainer, the place, the ball,
    the date and level), the Pokédex (caught; a wild Pokémon is also registered as seen when the battle starts), the
    Pokétch's history, the TV's capture record, the game records, Platinum's naming screen (left empty, it keeps its
    species' name), then the party, or the first box with room (its PP restored). BW2 does the same in its field
    after the battle (`event_battle_return.c`), which the port doesn't build.
  - BW2 records a catch in Join Avenue, which the port has none of: `resonance_resort_data.c`'s patch returns when
    Join Avenue isn't loaded (it crashed).
  - Not yet: Platinum's "Give a nickname?" question before the naming screen and its message that a Pokémon went
    to the PC; the Pokédex entry screen for a new species; a catch with every box full (Platinum doesn't let a ball
    be thrown then; BW2 doesn't know Platinum's boxes, and the Pokémon isn't kept).
  - Tests: `gen5_catch.txt` throws Master Balls (the test command `item`): a Bidoof into the party, then, with the
    party filled to six (`give`), a Starly into box 1; saved as the fixture `gen5-caught`. `gen5_caught.txt` loads it
    and logs (`party`) the party, the boxes, the bag and the Pokédex: both in Master Balls with the player's trainer
    ID, both caught in the Pokédex, no Master Balls left.
- **Next:** 4d, the Pokédex, records and options in BW2's stand-ins, the rest of the player's name characters, and
  the naming question.

## Milestone 5: trainers (in progress)

- **A single trainer battle (2026-11-08, done on the desktop):** `Gen5Battle_Wants` also takes a
  `BATTLE_TYPE_TRAINER` battle with no status flag, so the trainers met on routes; doubles, tag battles, the scripted
  ones with flags stay in Platinum's engine. `Gen5Trainer` (`gen5/api/gen5.h`) carries the trainer: its party, its
  class, AI flags, items, name, class name and defeat line in Unicode, and its prize.
  - **Class and sprite:** `gen5/src/trainers.c` gives each of Platinum's 105 classes BW2's class of the same name
    (20 of them) or the nearest one, for the sprite: Sinnoh's Gym Leaders and Cynthia have their own in BW2 (its
    Pokémon World Tournament), Team Galactic is shown as Team Plasma. The name shown is Platinum's
    (`BtlSetup.trainerNames`), "Youngster Tristan".
  - **Names:** Platinum keeps trainer names compressed (`CHAR_COMPRESSED_MARK`); `String_ConcatTrainerName` unpacks
    them.
  - **Text:** BW2 reads a trainer's lines from its own data by trainer ID, except for trainers from 0x800 on
    (`BtlSetup.trainerWinMsg`/`trainerLoseMsg`, its Pokémon World Tournament's), so the port numbers Platinum's
    trainers from 0x800 and passes Platinum's defeat line (`Trainer_LoadMessage`, `TRMSG_DEFEAT`). Platinum's
    line breaks and scrolls become BW2's. BW2 shows that line after the battle from its trainer message files
    whatever the ID, so `btlv_core.c`'s patch takes it from the setup for those trainers. The line before it,
    "Player defeated Youngster Tristan!", is BW2's own English text.
  - Names were cut to 4 characters ("Clau got $80"): the conversion kept room for a page break at the end of every
    string; it does only where one goes now (the player's and OT names, nicknames).
  - **Prize money:** Platinum's: the last Pokémon's level times 4 times the class's multiplier
    (`sTrainerClassPrizeMul`); `btl_calc.c`'s patch takes the multiplier from the port for those trainers.
  - **The AI:** Platinum's flags for the trainer (see "The trainer AI"). BW2's AI hung the battle at first: its
    jump tables read their offset and the program counter in one expression (`vm->pc + VM_Read32(vm)`), whose order
    C leaves open; the DS's compiler read the offset first, GCC and Clang read the counter first, and the jump landed
    4 bytes early, in the middle of the table. `tr_ai.c`'s patch reads the offset first.
  - Tests: `gen5_trainer.txt` battles Youngster Tristan (Platinum's trainer 1, the test command `trainer`): won with
    Tackle, money +80 in the log.
- **Next:** the trainer's items in battle (BW2's AI uses them: Potions), evolutions and new moves after the battle
  (as for wild battles), the rival's and Gym Leaders' battles (scripted, with flags), double battles (milestone 7).

## The player's back sprite (2026-11-09)

In BW2's engine the player was BW2's own trainer from the back (Hilbert or Hilda: a battle in a game other than BW2
gives the player BW2's classes 182 and 183, `TrainerData_SetFromPlayer`). It is now the owner's own art of Lucas or
Dawn, by the player's gender, commissioned for this fork: Dawn by PokemonBrendan, Lucas by tebited15, included with the
owner's permission (`licenses/player-back-sprites.txt`, `THIRD_PARTY_LICENSES.md`).

- **The art:** the sheets as delivered are in `gen5/art/sheets`. Lucas's has two rows, Diamond and Pearl's outfit
  (not used) and Platinum's (used): four frames, standing, the ball up, winding up, thrown. Dawn's has three poses
  (standing without her near arm, the arm over her head, leaning into the throw) and five arm pieces, worked out as:
  the near arm with the ball in her fist (standing), the near arm bringing the ball up and then to her head (both on
  the standing pose, in front), and her far arm swung back (behind the leaning pose). The fifth, an arm reaching
  behind her, isn't used.
- **The conversion** (`gen5/tools/player_back.py convert`, plain Python): keys out each sheet's backing colour (its
  top-left pixel), reduces the colours of the parts used to 15 plus transparency in the DS's 15-bit colour (a median
  cut refined by k-means; Lucas has 45, Dawn 24), packs the parts into one 256x128 4-bit texture (the size MCSS gives
  each sprite) and writes `gen5/art/<name>_back.png` (the texture, indexed) and `<name>_back.json` (each pose's quads).
  Where the arm pieces go and each pose's anchor (under the feet) are in the script. `player_back.py preview DIR`
  draws the poses as the game does. The build turns the converted files into C data (`player_back.py header`).
- **How BW2 draws it:** the battle view adds the player's back as an MCSS sprite (`BtlvMcss_AddTrainer`, position 8)
  from `ARCID_TRSPRITE_BACK`: a 256x128 image, a palette, cells, a multi-cell animation (its sequence 1 is the throw)
  and MCSS's bin file of each cell's quads. All of BW2's player back sprites have the same five multi-cells:
  standing, ball up, ball higher, winding up, thrown. `btlv_mcss.c`'s patch asks MCSS to draw the next sprite with the
  art (`PlayerBack_ForTrainerClass`, `gen5/src/player_back.c`: BW2's male player classes Lucas, the female Dawn).
  `mcss.c`'s patch then still reads and animates BW2's sprite, but puts the art's image and palette in its place when
  it loads (so fades and colour blends use the art's colours) and, each frame, draws the art's pose for the multi-cell
  the animation shows, moved, turned and scaled as the multi-cell is, with its shadow. So the throw keeps BW2's own
  timing and motion: for Hilbert and Hilda, 2 frames standing, 4 with the ball up, 24 with it higher (sliding back
  7 pixels), 8 winding up, then thrown (turning, sliding forward), held for 50.
- Platinum's battles don't change: this is all in the Gen 5 library.
- Tests: `tests/scripts/gen5_player_back.txt` (from `turtwig`, and from `turtwig-girl`, made by
  `new_game_girl.txt` and `turtwig.txt`) shoots a wild and a trainer battle's send-out a frame apart. On the desktop
  both show Lucas and Dawn standing, with the ball up, winding up and thrown (a run catches some of the poses: BW2
  moves several frames between shots). The other Gen 5 tests and `first_battle.txt` still pass, and the arm64 check
  compiles the library for Android. Not yet seen on the Thor.
- Not done: the art is drawn at its own size (103 to 117 pixels tall where BW2's sprites fill 128), and MCSS's
  mosaic effect (no trainer uses it) leaves the art alone.

## Sound (2026-11-09)

The owner's decision: the Gen 5 battles use BW2's sound effects and cries entirely, from the BW2 ROM the player
imported, and Platinum's music is left alone (the battle music Platinum starts plays on). On the Thor the gen5-m5-pc
build's battles were silent: no move sounds, no cries, no menu sounds. Done on the desktop:

- **BW2's sound archive next to Platinum's** (`gen5/src/sound.c`): the import already extracts
  `gen5/swan_sound_data.sdat` (89 MB, most of it music and streams). `Gen5_SoundInit` opens it with
  `NNS_SndArcInit` and a 2 MB sound heap of its own, then makes Platinum's archive the current one again. NitroSystem
  answers every question about sound data from its *current* archive, so `Gen5_SoundEnter`/`Leave` make BW2's the
  current one around each battle frame (`Gen5_BattleFrame`): what BW2's code asks (a sequence's player, a cry's wave
  archive) comes from BW2's data, and Platinum's sound system, which runs outside that, still sees its own.
- **Sound effects:** `GFL_SndSEPlay` and `GFL_SEPlayKeepVol` load the sequence with its bank and wave archives into
  that heap the first time (`NNS_SndArcLoadSeq`; it stays there, and a battle's start empties the heap when less than
  512 KB is left) and start it with `NNS_SndArcPlayerStartSeqEx` on one of Platinum's players: NitroSystem's players
  were set up once from Platinum's archive, and its music plays on them. BW2's sound effect players 1 to 5 go to
  Platinum's `PLAYER_SE_1` to `PLAYER_SE_4` (3 to 6) and `PLAYER_PV` (0), which get BW2's channel masks (12 to 15)
  for the battle and Platinum's back at its end (`Gen5_SoundBattleBegin`/`End`; libntr ignores the masks for now,
  see the gaps below, and the cries' locked channels are what keeps the sound effects off them). Platinum's music players (1, 2, 7)
  are left alone. Handles, `GFL_SndIsPlaying` (by handle: Platinum's music has sequence numbers of its own), pitch,
  pan, tempo and the mute calls work as `snd_sys.c`'s. The music calls (`GFL_SndBGM...`) still keep only the fades'
  timing: BW2's battle music, the victory music, the level-up and catch fanfares aren't played.
- **Cries:** `pm_voice.c` (BW2's own) reads a cry's wave archive from the current archive and plays it through
  NitroSystem's wave output on channels 14 and 15 (10 to 13 for more at once). `gen5/src/sound.c` has main.c's
  callbacks (the wave archive of each species and form, the volume from archive 191). Chatot's recording isn't
  played. `pm_voice.c`'s patch also clears a channel handle once freed (a cry that failed to start freed it twice).
- **libntr: wave output made no sound** (`subprojects/packagefiles/libntr_snd_waveout.patch`, for libntr itself
  later). The simulated ARM7 (`libraries/sim7/src/pxi.c`) left the direct channel commands empty: locking a channel,
  `SETUP_CHANNEL_PCM`, starting and stopping a channel (`START_TIMER`/`STOP_TIMER`), its volume, timer and pan. The
  sequences don't use them (their channels are driven inside the simulator), and Platinum's cries are sequences, so
  only NitroSystem's wave output (`NNS_SndWaveOut...`) was affected. And nothing kept the channels' status
  (`SND_GetChannelStatus`, the ARM7's `SND_UpdateSharedWork`), so `NNS_SndWaveOutIsPlaying` said a cry had ended 3
  frames in and `PokeVoice_Update` stopped it. The patch carries those commands out on the simulated channels
  (`SND_SetupChannelPcm7`, `SND_StartChannel7`...) and updates the status after each batch of commands and after the
  driver's work in the audio callback. Channels started together with a capture (Platinum's reverb on the title
  screen and the ending) aren't started: the simulator doesn't capture, so they would play an unwritten buffer.
  Platinum's own wave output (Chatot's recorded cry) plays too now.
- **Checked** (the container has no audio; `SDL_AUDIODRIVER=dummy` still runs the mixer): each battle's end logs
  what its sound did, e.g. `gen5_wild.txt`: "6 sound effects started, one playing on 404 of 1064 frames, a cry on
  444, Platinum's music on 1064" (before the libntr patch: a cry on 6). `gen5_trainer.txt`: 52 sound effects. No
  sequence failed to load or was a music sequence; the heap held under 100 KB. All the Gen 5 tests and
  `first_battle.txt` pass. Not heard yet: on the Thor.
- **libntr gaps found on the way** (not fixed here; they affect Platinum's sound as much): the simulated ARM7 also
  ignores `PLAYER_PARAM`, `TRACK_PARAM`, `MUTE_TRACK`, `ALLOCATABLE_CHANNEL` and the master volume and pan, so a
  player's or a sequence's volume, a track's pan and pitch (BW2's move effects pan their sounds), and channel masks
  set at run time do nothing in the port.
- **Next:** hear it on the Thor; perhaps the victory music and fanfares as Platinum's (`GFL_SndBGMPlay` with BW2's
  win BGM).

## Risks

- **64-bit layouts** in BW2 data loaded from files or shared with the SDK: found case by case at runtime.
- **Two games' systems in one process:** BW2's GFL and Platinum's systems both expect to own the DS's VRAM, OAM,
  3D engine, sound and interrupts. The battle must leave them as Platinum expects.
- **The DSi SDK:** BW2 is a DSi-enhanced game; anything it uses from the TWL SDK that libntr lacks needs adding.
- **Data mapping:** Platinum's trainer classes, terrains and battle types onto BW2's.

## Milestone 6: widescreen (2026-11-09)

- **The camera:** `Gen5_SetWidescreen(ratio)` (`api/gen5.h`), called by `src/port/gen5_battle.c` before each frame with
  libntr's `SIM_WidescreenRatio()` while its wide scene is on, makes `Gen5_Widen` scale the horizontal extent of BW2's
  projections: the camera's aspect ratio in `GFL_G3DSysMtxSetProjection` (`patches/src/gfl/g3d_system.c`; BW2's battle
  camera is 1.35, `btlv_camera.c`) and the left and right of the orthographic projection MCSS draws some sprites with
  (`patches/src/system/mcss.c`). When the ratio changes, `Gen5_G3DReproject` sets the camera's projection again (BW2
  sets it only when the camera moves). The Pokémon sprites are drawn in 3D with the camera's projection, so they stay
  unstretched and in place.
- **The scene:** `Widescreen_Update` asks libntr for a wide scene while `Gen5Battle_IsShown()` (from
  `Gen5_BattleBegin` to `Gen5_BattleEnd`; the naming screen and evolutions after it are Platinum's, 4:3).
- **Checked on the desktop** (`tests/scripts/gen5_widescreen.txt`, with `TEST_CONFIG` turning widescreen on and the
  top window 16:9): a wild and a trainer battle fill the window, the arena's sky and ground reaching the edges; the
  sprites, gauges and text box are as in 4:3; Tackle's effect is in place.
- **The overworld's weather** went wide at the same time (`src/port/widescreen.c`): the weather's sprites through
  libntr's `SIM_SetWidescreenObjWrap` (`subprojects/packagefiles/libntr_wide_obj.patch`) and its BG2 picture through
  the wide BG layers libntr already had; only Flash's darkness stays 4:3 (`tests/scripts/widescreen_weather.txt`).

## Widescreen: field moves, Bag, maps, Journal (2026-11-09)

- **Child applications** (`WideApplication` in `src/port/widescreen.c`): while the process manager's child is the Bag,
  the party screen, the Town Map (item, wall and Fly modes) or the Journal (recognised by the child's `init`), the scene is wide and all
  four BG layers' edge columns continue across the sides (`SIM_SetWidescreenBgEdgeExtend`, 0x0F). The apps draw for 256
  pixels, so the lists, text and map stay in the middle; their backgrounds (the Bag's blue and green, the Town Map's blue
  frame, the Journal's page) fill the sides. The Journal's load-time page opens before the field map exists, which is why
  this doesn't depend on `ShownFieldSystem`.
- **The HM cut-in** is a band: BG3 (the rings) behind hardware window WIN0, whose X range grows from the middle to 0..255.
  While `HMCutIn_PortIsPlaying()` BG3 is drawn wide (`G2SIM_DrawBGWide`) and libntr's window test treats a window that
  starts at 0 or reaches 255 as continuing across the wide layer's sides (`libntr_wide_window.patch`). The rings are mirrored
  at the sides (`SIM_SetWidescreenBgMirror`, `libntr_wide_mirror.patch`) and the cut-in's sprites (wind, the Pokemon)
  use the OBJ wrap, so they continue to the edges. This works with clear weather too (an early return in
  `WeatherLayers` used to skip it).
- **The Journal's black pages** (4:3 too, found on the Thor): libntr's `G2_GetBG0CharPtr`, `G2_GetBG1CharPtr` and
  `G2_GetBG2CharPtr` masked the character base to 128 KB (`& 131071`, `% 8`) while the renderer reads the real one. The
  Journal's text windows on BG1 (char base 0x20000) were therefore loaded at 0, over the page's tiles (BG2 and BG3 at 0):
  the pages with text lost their paper. The masks are gone (`libntr_bg_charbase.patch`). `widescreen_journal_load.txt`
  shows the page that opens on loading a save (it needs a save with journal entries).
- **Tests**: `tests/scripts/widescreen_screens.txt` (Surf's cut-in, the wall map), `widescreen_bag.txt`,
  `widescreen_journal.txt`, `widescreen_flymap.txt`, `widescreen_party.txt`: the test command `app bag|journal|townmap|flymap|party` opens the app over
  the overworld; it has no field task to return to, so each such script ends in the app.

## Widescreen: Platinum's own battles (2026-11-09)

The backdrop (BG3, `battle_main.c`) is drawn wide and mirrored at the sides (`SIM_SetWidescreenBgMirror`); the Pokemon,
bases, HP boxes and text box stay in the 4:3 middle. The HP boxes' end caps (the owner's 5x33 texture, pure white
transparent; enemy: at the box's left end, player: mirrored at its right end) are stamped onto BG3's wide picture each
frame at the boxes' sprite positions (`UpdateHealthboxCaps`, `SIM_SetWidescreenStamps`, `libntr_wide_mirror.patch`), so
they follow the boxes as they slide. A box's sprite position is its centre: enemy box 116 wide and 33 high (cap at
x - 63, y - 16), player box 128 wide, 42 high with the EXP bar (cap at x + 64, y - 21, stretched; the player's HP
band is a row lower than the enemy's). Double battles' player boxes (33 high assumed) are not checked.
`tests/scripts/widescreen_gen4_battle.txt` has the shots.

## Widescreen: the Gym Leader encounter effect (2026-11-09)

`EncounterEffect_GymLeader` (`encounter_effect_core.c`) is the "VS" banner: BG3 (the banner, scrolled every frame)
behind a window opened line by line, sprites (the sliding bars, priorities 1 to 3) and the leader's mugshot (priority 0,
enters from x = 272). While it plays (`EncounterEffect_PortGymLeaderPlaying`) BG3 is drawn wide and mirrored and the OBJ
wrap covers priorities 1 to 3 only, so the mugshot doesn't reappear at the left as it enters. The other encounter effects
(wild, trainer, Elite Four...) are not widened. `tests/scripts/widescreen_leader.txt` (test command `leader N`).

## Particles (2026-11-09)

The Poké Ball's throw and opening and the moves' particles weren't drawn. They were lost in libntr's renderer: it keeps
translucent polygons (alpha under 31, A3I5 and A5I3 textures: every particle) for the end of the frame and drew them
with the culling and depth test that the frame's last polygon attributes left in GL, so particle quads drawn without
culling were culled. Fixed in libntr (`subprojects/packagefiles/libntr_g3_translucent_state.patch`, in the wrap's
`diff_files`): each deferred polygon gets its own culling and depth test.

## Widescreen on the Thor, first look (2026-11-09)

On the Thor (combined2 build): the enemy and the player's Pokémon stay on their platforms as the camera moves (BW2's
screen-position maths, `MCSS_Draw`'s and `BtlvEffvm_ProjectToScreen`'s 128 and 4, now use `Gen5_Widen`), the Poké Ball
is round (the particle cameras' orthographic extents are widened in `GFL_G3DSysMtxSetProjection`), and the text box
(BG1) reaches the screen's edges (libntr_wide_edge.patch). Seen, not fixed:
- The arena's sky and ground show stair-stepped diagonal creases in their gradient, which shift as the camera idles
  (left of the enemy's spot, and at the right). The desktop's 4:3 shots show the same rays on a different arena, so it
  isn't widescreen's: the guess is that libntr draws each quad as a fan of triangles and interpolates the vertex
  colours across them, where the DS rasterises a quad as a whole. Not looked into yet.
- ~~The shadows under both Pokémon flickered~~ (fixed, confirmed on the Thor): their overlapping translucent quads were
  blended twice, in an order that changed from frame to frame. libntr now has the DS's polygon ID rule (a translucent
  polygon doesn't blend over a pixel another one with its ID drew; `libntr_translucent_id.patch`,
  `SIM_SetTranslucentPolygonIdRule`, on only during a Gen 5 battle, stencil-based).

## The arena's warping (2026-11-09)

The sky and ground polygons were warped on the Thor (stair-stepped, sheared textures) after the clipper was in. Cause:
libntr's vertex shader had W fixed at 1, so textures and colours were interpolated in screen space, not
perspective-correct as on the DS; large polygons warp. Fix: each vertex carries its clip-space W
(`libntr_g3_perspective.patch`), the shader outputs `(xyz*w, w)`, and the fog depth is recovered as screen-linear in the
fragment shader (`coordsW`; GLSL ES has no `noperspective`). Checked on the Thor: wild battle, no shader errors, sky and
ground correct. Open: the stat raising and lowering particles look a bit off (not investigated).

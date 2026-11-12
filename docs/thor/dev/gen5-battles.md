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
   *In progress:* double battles against one or two trainers and beside a partner run on the desktop (see
   "Milestone 7").
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
  the NDK's `llvm-nm` and `llvm-objcopy`; the CI would need a pokebw2 clone at a pinned commit). Done 2026-11-12:
  `build_android.yml` fetches pokebw2 at `2c3961614d0f` (the commit the port is tested with) and builds with it, so the
  Actions APK has Gen 5 battles. Bump that commit there when the local clone moves.

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
- **The story's trainers (2026-11-09, done on the desktop):** most need nothing more. Platinum's scripts start Gym
  Leaders, the rival, Cynthia, Team Galactic and the rest with `Encounter_NewVsTrainer` and no status flag, as for a
  route trainer, and the script reads the result (`CheckWonBattle`, `BlackOutFromBattle`, badges, `SetTrainerFlag`), so
  the bridge's WON/LOST result is all they need. The flags that exist: `BATTLE_STATUS_FIRST_BATTLE` (Barry on
  Route 201: **accepted now**; Platinum there also keeps critical hits out and shows only FIGHT, which BW2 doesn't
  do), `DISTORTION` (the Distortion World, with Giratina: still Platinum's), and on wild battles `NO_RUNNING`,
  `LEGENDARY`, `GIRATINA`, `HONEY_TREE`, `RECORDING`/`RECORDED` (link battles). Doubles and tag battles are milestone 7.
  - **Battle style:** Platinum's Set/Shift option goes to BW2's config (`Gen5Battle.setStyle`). BW2 asks "Will you
    switch?" after each of a trainer's Pokémon in Shift, as the scripted tests found out at Roark's second.
  - **Level 100:** BW2's gauge divided by the experience still to go, 0 at level 100 (the DS's divider returns
    garbage; x86 traps). `btlv_gauge.c`'s patch guards its four divisions.
  - **Prize and loss:** Roark pays 1680 (his last Pokémon's level 14 times 4 times 30) and a loss with no badges and a
    level 6 lead takes 48, both as Platinum's; the party is healed at the blackout.
  - Tests: `gen5_roark.txt` (`turtwig`) plays Roark's real script from in front of him to the blackout;
    `gen5_gym.txt` (the owner's finished save, `full100`) plays Roark and Route 201's rival through `trainer`, to the
    end, with the prize money in the log; `gen5_first_battle.txt` (`before-starter`) Barry's first battle and the
    script after it. The badge and TM scripts after a win are Platinum's own and weren't played here (the finished
    save has the badge: a test would need a fresh game's Gym).
- **Arenas (2026-11-09):** the arena was always the default grass one. `ArenaFromDTO` (`src/port/gen5_battle.c`) now
  maps where Platinum puts the battle to BW2's arena table (`BtlFieldEnv.bgType` and `terrain`; the table is file 0 of
  archive 151 in the ROM, 21 backgrounds with 20 terrain floors each): Gym Leaders get BW2's gym hall (background 4,
  terrain 19: dark walls, a stone floor), Platinum's Elite Four and Champion their own rooms (12 to 16), the
  Distortion World the purple void (11), and the rest from `dto->background` and `dto->terrain` (sea, city,
  mountain, snow, indoors and caves, sand, grass, water, ice). Found by rendering every background and terrain
  (`POKEPLATINUM_GEN5_ARENA=bg,terrain` overrides it; `tests/scripts/gen5_arena.txt`). The Elite Four's rematches (trainers 866 to 869) get BW2's World Tournament arena with the coloured rim lights (19);
Cynthia keeps her room (16) in every fight. The tournament arena's coloured light pools are its alternate variant
(`BtlvField_Create`'s `alternateFieldVariant`, set by `BtlSetup.unkDD_6`, which the World Tournament's final uses): only
field 46 has one (the arena table's file 1 lists each field's variants), so the lights can't go on the other rooms. Terrain 7 has no floor in the
  dark backgrounds and crashes: never use it. Not every combination was looked at.
- **Team Galactic's arena (2026-11-11):** Galactic Grunts, Commanders and Cyrus (`IsGalacticClass`) battle in an edited
  copy of BW2's Champion room, background 16 terrain 1 (Platinum never uses that terrain there). `src/port/gen5_galactic.c`
  changes a copy of the room's model resource (member 115 of archive 11, a BMD0 with its textures) made from the
  player's own ROM at the first Gen 5 battle (nothing of BW2's is stored): the streaks are ribbons (shapes `polygon3` and
  `polygon4`, whose vertex colours give each its rainbow; the texture `batt_field21n` is white) and each ribbon gets Crobat
  purple or Team Galactic's hair blue, alternating, at the brightness its vertices had; the sky's texture `batt_sky21n`
  (A3I5, 128x64, sharing its palette with the streaks') is redrawn with gleaming four-armed sparkles and a scatter of
  fainter points (fixed places, so the sky is always the same). The background is darker (`DarkenBackground`): the sky's and
  the ground's palettes (`batt_sky21_pl`, `batt_field21_pl`) have their levels stretched per channel (the darkest colour
  is black, the brightest stays) and squared, so the mids fall toward black. The stars' shape (`polygon2`) has its bluish
  vertex colours made grey and brighter (`WhitenSky`) so the stars shine at full contrast. Jupiter and Saturn (pixel art,
  `Jupiter` and `Saturn`, after the Commanders) are in the same texture, in colours 2, 5, 6 and 7 of its 8-colour palette;
  the texture is stretched about 1.8 times wider than high and wraps around the room twice, so they are drawn narrow and
  each shows twice in widescreen; only the texture's top 22 rows or so are visible. The platforms (the stage's ground
  model, member 38, `batt_stage21_pl`) are dark indigo (`Gen5Galactic_MakeStage`), a stage of their own (35). `Gen5Data_PrepareGalacticArena` (`gen5_data.c`) puts the
  models in a copy of archive 11 as new members (572, 573) and, in a copy of archive 151, adds field 47 (row of file 1: the
  Champion room's, with the new model) and points the Champion background's terrain 1 at them (field and stage); Cynthia's room (terrain 0) is untouched; the animations (the
  scrolling, found by material name) are the Champion room's. The copies are in
  `gen5_data/`, redone when the ROM's archive changes size or `GALACTIC_VERSION` is raised. Test:
  `gen5_galactic_arena.txt` (`turtwig`; or `POKEPLATINUM_GEN5_ARENA=16,1` with `gen5_arena.txt`). Not done: the stars
  don't twinkle (a static texture; a twinkle needs a second texture or a palette animation); the colours are constants in
  `gen5_galactic.c` (`RecolorRibbons`, the stars' `COLOR_ICE` and `COLOR_WHITE`). Not checked on the Thor.
- **Options:** Platinum's Text Speed goes to BW2's config (`Gen5Battle.textSpeed`: BW2's 0, 1, 2 are slow, normal,
  fast too), both the battle's own and the save's the text printing reads; the first was forced to 0 before.
- **Sprites of Sinnoh's special trainers:** BW2 has its own for the eight Sinnoh Gym Leaders (Roark to Volkner) and
  Cynthia, already used (`gen5/src/trainers.c`); where a trainer is in both games, BW2's is used. BW2 has no Sinnoh Elite
  Four (Aaron, Bertha, Flint, Lucian), Barry, Cyrus or the Galactic commanders: they show another class's sprite for
  now (Unova's Elite Four, Plasma's grunts) and need Sinnoh-style sprites made for them, as the player's back sprite
  was (the owner will supply them).
- **Still to do for the story's trainers:** the Thor (an APK built with these changes), critical hits in the first
  battle, Cynthia and the Galactic bosses' own texts and sprites (the nearest class is used).
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

## The Elite Four's animated Platinum sprites (2026-11-11)

The Elite Four's Platinum front sprites (BW2 has no Sinnoh Elite Four) were drawn in their first cell only. Platinum keeps
a trainer as a cell bank (Aaron 9 cells, Bertha 3, Flint 4, Lucian 5, Cynthia 5) over a character file whose tiles are
copied into place cell by cell (the bank's VRAM transfer table: a source offset and size for each cell, the tile numbers
of an OBJ relative to that block), and an animation bank: sequence 0 holds one pose, sequence 1 plays through the poses
once (Lucian: cells 0, 1, 2, 3, 4 for 30, 20, 2, 2 and 50 ticks) and ends on the pose the sprite stays in.
- **Conversion** (`TrainerArtToGen5`, `src/port/gen5_battle.c`, for each battle): every pose becomes a list of quads;
  rectangles with the same pixels are shared, and the distinct ones (10 to 12 for the five trainers, about half the
  texture) are packed in rows into the 256x128 texture. A quad is placed as Platinum places it, from the cell's centre
  with the feet 40 below it (anchoring on the union of the poses made Aaron float: one of his poses reaches lower).
- **Playing it** (`mcss.c`'s patch, `Gen5BackSprite.seqPose/seqTicks`): the first sprite of the trainer that is added
  (the introduction) plays sequence 1 by time (`Gen5_ArtTicks`, one a VBlank; Platinum's ticks of its 30 Hz logic count
  as two), then stands in the last pose; the later ones (the trainer slides in again for his words after the battle) show
  the last pose only (`useFixed`). Checked on the desktop (`tests/scripts/gen5_elite_four_anim.txt`; Lucian's win
  through his defeat words: the same last pose as the end of the introduction). Not seen on the Thor yet. Open: whether
  Platinum's ticks are 30 Hz as assumed.
- **Barry** (2026-11-12): his class mapped to BW2's rival, so he showed Hugh's sprite (seen on the Thor). He now gets
  his Platinum sprite the same way (`TrainerClassHasPlatinumArt`); checked on the desktop against his Route 203 team
  (trainer 248). Platinum's PK and MN characters (`CHAR_PK`, `CHAR_MN`), which `Gen5_TextToGen5` dropped ("You are
  challenged by  Trainer Barry!"), are written "Poké" and "mon", as BW2 writes its class names.

## Pokémon sprite animations (2026-11-10)

Reported on the Thor: a foe Bronzong's far arm still after the first cycle; a foe Houndoom's tail tip, the player's Suicune's
head and feet likewise: **each part moves during the first cycle after the Pokémon appears, then stops for good**.

- **Cause:** libntrsystem's `SequenceEdgeHandleCommon_` (`g2d_Animation.c`), at the end of a sequence, had an `SDK_PORT`
  guard that stops the animation if `pCurrent->frames == 0`. At that point `pCurrent` is one past the sequence's end, so
  it read the next sequence's first frame, or for the last sequence of a bank the memory after the frame array (the port's
  `malloc` in `NNS_G2dUnpackNAN`). When that is 0 the animation is stopped for good at the end of its first cycle. On
  Linux that memory is non-zero, so the desktop build never showed it (a counter in the guard never fired); on Android's
  allocator it was zero for some sequences: the last of Bronzong's three cell animations (the arm), and so on.
  BW2's sprites are one multi-cell animation whose nodes each play a cell animation sequence, so it is the parts that
  stop, the rest going on.
- **Shown on the desktop:** allocating one zeroed frame after the array (`calloc(n + 1)`) made the far arm of Bronzong
  stop after its first cycle exactly as on the Thor; with the fix below, the same run moves both arms.
- **Fix:** `subprojects/packagefiles/libntrsystem_anim_edge.patch` (the wrap's `diff_files`): the guard stops only a
  sequence in which no frame lasts any time (the case it was written for: it would loop forever). Platinum uses the same
  library: its animations behave as before wherever the old read was non-zero. **Checked on the Thor by the owner (2026-11-10): the
  Bronzong, Suicune and Houndoom parts keep moving after the first cycle.**
- **Not defects** (checked, for the record): BW2's sprites have one multi-cell sequence each in the ROM, so
  `BtlvMcss_OnAnimationEnd`'s alternate animation (sequence 1) never exists for them; their `.bin` idle-node count is 0, so
  BW2's idle task freezes the whole sprite for about a second every two or three loops; Houndoom's loop is five frames
  of 50 then 90 on the multi-cell with the open mouth, so its mouth opens every loop in the data (how the real game makes
  it occasional isn't known: not seen); Bronzong's arms have mirrored rotation sequences of about +-30 degrees.
- **Open:** the battle runs at Platinum's 30 Hz logic rate (`FrameRate`, unless 60 FpsEverywhere) where BW2 runs at
  60 Hz: BW2's animation timing, not measured against a real BW2.
- **Testing tip (from the owner):** choosing FIGHT and backing out locks the battle camera in place.

## Pokémon sprite animations (2026-11-10): findings, not fixed

Reported on the Thor: a foe Bronzong's far arm static and one cycle repeated forever; a foe Houndoom's mouth opening on
every cycle; the player's Suicune's head still and its feet lifting and shifting before it sits, after the send-out (the
send-out itself is right); the tip of the player's Houndoom's tail not following the rest. **Not reproduced on the desktop
build**, so nothing was changed. What was checked (with temporary logging in libntrsystem and BW2's `mcss.c`, since
removed):

- **The data and the engine agree.** BW2's Pokémon sprites are one multi-cell animation each (`MCSS`: NMCR + NMAR + NCER +
  NANR + a small `.bin`), the animation bank holds **one** sequence (Bronzong, Suicune, Houndoom: checked in the ROM), so
  BW2's "second animation" (`MCSS_SetAnimation(.., 1)` in `BtlvMcss_OnAnimationEnd`) is never used for them; the file
  has no such sequence. A Pokémon's loop is its sequence: Houndoom's is five frames of 50 and a last of 90 on the second
  multi-cell (the mouth-open cell animations), so the mouth opens every loop (about 340 video frames) in the data;
  Bronzong's is 315 + 105; Suicune's 35 frames (a 96-frame pose in the middle, and zero-length frames at 16 and 34).
- **Idle:** the `.bin` tail's idle node count is 0 for all three (`MCSS_GetIdleNodeCount`), read correctly (a 4-byte tail
  in the file). `BtlvMcss_OnAnimationEnd` therefore starts `BtlvMcss_IdleTask` about every second or third loop (1/3, 1/2,
  then always): it stops the multi-cell animation and every node (none is listed as an idle node) for 0x30 to 0x40
  frames, then sets animation 0 again. The sprite freezes briefly then; this is BW2's code as written, not a port issue.
- **Every node animates.** Per-tick dumps of the multi-cell instance (frames, active flags, sequences) for all the
  sprites on screen: no cell animation stays inactive or stuck while its multi-cell is running (about 1500 ticks each,
  Suicune 21 nodes, Houndoom 15, Bronzong 3); the multi-cell bank unpacked by `Gen5_UnpackMultiCellBank` matches a
  parse of the file; the cell animation sequences (rotation, translation) match the files; Bronzong's arms have mirrored
  rotation sequences (up to +-30 degrees) and both move on screen.
- **A real defect in libntrsystem, not the cause:** `SequenceEdgeHandleCommon_`'s `SDK_PORT` guard
  (`g2d_Animation.c`) tests `pCurrent->frames == 0`, with `pCurrent` already one past the sequence's end, so it reads the
  next sequence's first frame (or whatever follows the array) and stops the animation for good if that is 0. It never
  fired in these battles (counted), so it was left alone: testing "no frame of the sequence lasts any time" instead is the
  fix, to try if an animation ever stops for good.
- **Not checked:** the Android build (arm64) itself: the Thor wasn't reachable over adb when this was looked at, and
  every check above is the desktop build's. Differences between the two that this could be: arm64-only code in libntr's
  renderer (GLES) or an old APK. To check next: the same battle on the Thor with a build from this branch, and on the
  desktop in real time (these tests run the game faster than real time). Also: the battle runs at Platinum's 30 Hz
  logic rate (`FrameRate`, unless 60 FpsEverywhere), where BW2 runs its own at 60 Hz: every BW2 animation's timing is
  half-speed in game time if BW2's durations (7 frames per cell step...) assume 60 Hz; not measured against a real BW2.

## Milestone 7: double battles (2026-11-12, done on the desktop)

`Gen5Battle_Wants` takes Platinum's trainer battles of the four kinds `Encounter_NewVsTrainer` makes, and wild battles
beside a partner; `Gen5Battle` (`gen5/api/gen5.h`) carries `doubles`, a second trainer (`trainer2`), a partner
(`partner`) and a second wild Pokémon (`wild2`), and `runtime.c` sets BW2's setup as its own `BtlSetup_SetTrainer*`
functions do (`src/system/btl_setup.c`):

| Platinum | What it is | BW2 |
| --- | --- | --- |
| `BATTLE_TYPE_TRAINER_DOUBLES` | one trainer, two Pokémon at once (twins; a trainer whose data says double) | style double, client 1 |
| `BATTLE_TYPE_TAG_DOUBLES` | two trainers who saw the player at once | style double, multi mode 4: clients 1 and 3 |
| `BATTLE_TYPE_TRAINER_WITH_AI_PARTNER` | two trainers, a partner beside the player | multi mode 3: the partner client 2 |
| `BATTLE_TYPE_AI_PARTNER` (wild) | two wild Pokémon (Platinum's slots 1 and 3), a partner | multi mode 5, both wild Pokémon in client 1's party |

- A trainer who is in both of Platinum's enemy slots (the same ID twice) is one double battle's trainer, as Platinum
  treats it.
- **Each trainer's prize and defeat line:** both were one value for the battle. `Gen5_PortTrainerPrizeBase` and
  `Gen5_PortTrainerDefeatText` (`runtime.c`) look the trainer up by its ID (`btlv_core.c`'s patch calls the second), so
  two trainers each say their own line, and the prize is BW2's sum of both (Tristan 80 and Jacob 224: 304).
- **Art:** the opponents' Platinum art (`Gen5_SetTrainerArt`, `player_back.c`) has two slots, one per opposing trainer,
  keyed by BW2 class.
- **The partner** is drawn with BW2's back sprite of its nearest class (Barry as Hugh); Platinum's own back sprites of
  the partners, or the owner's Gen 5 style ones, are still to do. Its party isn't written back (Platinum doesn't either).
- **Double battle trainers' lines** (from the Thor: Liv & Liz said nothing): a double battle's trainer has no
  `TRMSG_DEFEAT` but one line for each of its two people (`TRMSG_DOUBLE_BATTLE_DEFEAT_1` and `_2`); both are shown,
  a page each, as BW2 shows one trainer's line.
- **Victory music** (from the Thor: none played): BW2's music calls are stubs (Platinum's music plays on, see
  "Sound"), so its win music (`SEQ_BGM_WIN1` to `6`) raises `GEN5_EVENT_VICTORY_MUSIC` (`gen5/src/sound.c`) and
  `gen5_battle.c` plays Platinum's own as its battle picks it (`PlayVictoryMusic`: by the first trainer's class, Gym
  Leaders, Galactic, Cyrus, the Elite Four, Cynthia; the wild one). For every Gen 5 battle, single ones too. Logged,
  not heard (the container has no audio).
- Test: `gen5_doubles.txt` (`turtwig`, with a level 50 Empoleon given) plays Twins Liv & Liz (trainer 15), Tristan and
  Jacob (1 and 16, the test command `trainers A B [PARTNER]`), then the same two beside Cheryl (608), to the end; each
  shot at the first command shows both sides right.
- **Not tested:** a wild battle beside a partner (it needs the partner's variable set and tall grass; catching there
  is for the second wild Pokémon too, `caughtWild2`); the defeat lines (logged with their length) and the second
  trainer's slide-in on screen;
  widescreen with four Pokémon; the scripted partner battles (Spear Pillar, Iron Island...) and their scripts after;
  the Thor.

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
- **The party screen** edge-extends its layers, and mirrors BG2 and BG3 (the panels and the lined background) across the
  sides through `SIM_SetWidescreenBgEdgeMirror` (a shader-side mirror for layers that aren't drawn through the wide path;
  `libntr_wide_mirror.patch`). The occupied slot's blue panel shows once more, mirrored, at the left.
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

The backdrop (BG3, `battle_main.c`) is drawn wide with its edge columns continued across the sides (not mirrored,
which repeated the bases the Pokemon stand on, 2026-11-11). The Pokemon are 3D quads: the scene is narrowed
(`SIM_SetWidescreenSceneOptions(scale3D)`) so they keep their 4:3 shape (they were stretched 1.33x wide, and
Bidoof's squash animation looked doubled). In a double battle the pairs stand 14 DS pixels further apart each
(`Widescreen_BattleSpreadX`, used where `battle_display.c` places a sent-out Pokemon and for the slide-in target;
move animations aimed at fixed coordinates may be a few pixels off); the Pokemon,
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
(wild, trainer...) are not widened. `tests/scripts/widescreen_leader.txt` (test command `leader N`).

## Widescreen: the Elite Four's and the Champion's encounter effect (2026-11-11)

`EncounterEffect_EliteFourChampion` (Aaron, Bertha, Flint, Lucian and Cynthia: one function, the parameters differ) is
a different build from the Gym Leader's: a captured frame (BG3 bitmap), 3D particles (BG0), and sprites: the two
portraits, the band's two halves (green, orange, purple, grey: its palette) and the "VS". The band is OBJs, so BG
layers don't help. While it plays (`EncounterEffect_PortEliteFourPlaying`) `src/port/widescreen.c` asks libntr's OBJ
edge extend (`SIM_SetWidescreenObjEdgeExtend`, the party balls' bar's mechanism) for **priority 2**, and the effect puts
the band's halves (they were priority 1, as the portraits) at priority 2: the band's edge columns, with the white borders,
continue across the sides, as the halves slide out too; the portraits stay at priority 1 so that their edge pixels
(hair, skin) never smear, and the name text (BG2, priority 0) stays above them. The 3D particles (priority 1 from the
clash on) now draw over the band instead of behind it, as they already did before the clash. Cynthia's portrait has a
black dotted blob over her mouth: it is in the art (`res/trainers/classes/champion_cynthia/mugshot.png`), not the port.
Checked frame by frame on the desktop with the new test command `stillshot` (`tests/scripts/widescreen_e4.txt`).
## Widescreen: the party balls' bar and the ability bar (2026-11-09)

BW2's bars reach the 4:3 picture's edges and stopped there in a wide scene. Two kinds:
- **The party balls' bar** (the foe's slides in from the left, the player's from the right) is an OBJ sprite of priority 0.
  libntr's `SIM_SetWidescreenObjEdgeExtend` (`libntr_wide_obj_edge.patch`) continues the pixels of OBJs at the picture's
  left and right columns across the sides (the whole bar, border and inside), for the priorities asked for:
  `src/port/widescreen.c` asks for priority 0 only during a Gen 5 battle. The other priorities hold the gauges, whose
  edge pixels (the player's EXP bar) would smear.
- **The ability pop-up's bar** ("Palkia's Pressure", from the left for the player's Pokémon, from the right for the
  foe's) is a BG layer (2 or 3): `SIM_SetWidescreenBgEdgeExtend` now takes layers 1 to 3 (it was the text box, BG1).
Checked on the desktop in widescreen: `gen5_roark_intro.txt` (the balls' bar, both sides),
`gen5_ability_bar.txt` (a wild Gyarados's Intimidate, the foe's bar) and `gen5_ability_bar_player.txt` (Palkia's Pressure,
the player's bar, from the owner's finished save). The Gym Leader's cut-in before the battle is Platinum's own
encounter effect, not BW2's, and is not part of this.

## Widescreen: the full-screen moves (2026-11-10)

Moves that fill the screen with a background of their own (Waterfall's water, Water Pulse's sea, Aerial Ace's chevrons,
Fire Blast's rays, Psychic's waves, Flamethrower's red) draw it on BG2 or BG3 and showed it in the 4:3 middle with the
edge columns smeared across the sides. Those layers are now edge-mirrored (`SIM_SetWidescreenBgEdgeMirror`, from the
widescreen branch's `libntr_wide_mirror.patch`; `src/port/widescreen.c`): the picture is reflected across each side, so
the patterns line up at the seam and fill the 16:9 window; the Pokémon, the particles and the text stay in the middle.
The ability pop-up's bar is on BG3 too, and its text would be mirrored, so the mirror is on only while a move's background
is up: `btlv_effvm.c`'s patch tells the port when `BtlvEffvm_CmdLoadBg` loads one and when the move's end restores BG3
(`Gen5_SetMoveBgShown`, `Gen5Battle_MoveBgShown`); otherwise the layers' edge columns are continued, as the text box's are. Checked on the desktop in widescreen with
`gen5_moves.txt` (the lead's first move set with the test command `learn MOVE`: 127 Waterfall, 352 Water Pulse,
332 Aerial Ace, 126 Fire Blast, 94 Psychic, 53 Flamethrower; the shots follow each move). Open: Flamethrower's fire
stream is a few faint red dots on its red background (Fire Blast's flames are solid): looked at, not understood.

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

## Merging the sessions' work (2026-11-11)

The parallel sessions' branches were merged into one (`gen5-battles-m5`), after each session confirmed its latest work.
- Base: `claude/determined-bun-fe75c5` (the bridge, milestone 4 and 5, trainers, Elite Four, sound, sprite animation,
  widescreen battles). `gen5-battles`' own milestone 4 (`8a63ef1b3`) was replaced by it.
- Merged: `final-apk` (the Gen 5 Summary, widescreen PC and party menus, libntr wide patches, signed-char Android
  fix), `claude/jolly-engelbart-5c2ba7` (widescreen Platinum double battles), the Hall of Fame crash fix and its
  `hall-of-fame` test command (`claude/epic-grothendieck-360b0b`), and the note that the animation fix is confirmed
  on the Thor.
- Dropped: `e57e0ed9b` (near/far clipping: `ea8e8a554`'s six-plane clipper replaces it), `cfcb1e5a2` (the particle
  patch is the same as `dfe62d2eb`), `369b291e2` (jingles through Platinum's sound: uncompiled, and the sound branch
  has BW2's own), `pc-black-screen-test` (test-only).
- Tested on the desktop: `first_battle` (from `before-starter`), `gen5_wild`, `gen5_trainer`, `gen5_roark`,
  `gen5_gym` (`full100`), `gen5_summary_menu`, `hall_of_fame`, `widescreen_gen4_double`, `widescreen_weather`.
- Open, from the sessions: BW2's victory music (`ProcWaitExit` waits for a button before the battle exits), Barry's
  first-battle crit suppression, the Flamethrower stream, the stat raise/lower particles on the Thor, the 30 Hz
  against 60 Hz timing assumption, the Summary's ribbons and markings, nothing from the Summary, widescreen or E4
  work run on the Thor yet.

Owner's decisions on the open items (2026-11-11):
- Twinleaf is 16:9 on the Thor now (no longer open).
- The Pokémon Center healing glitch can wait.
- Barry's first-battle crit suppression: not needed; he may crit.
- Flamethrower's stream is still bugged (open).
- The stat raise/lower particles look mostly fine; slightly off, probably alpha or transparency (low priority).
- Summary ribbons can be ignored; turn Gen 5 screens off to see them.

## Renegade Platinum (2026-11-11, milestone 8, first part)

A game whose Pokémon and move files aren't vanilla Platinum's (the hash of `pl_personal.narc` and `pl_waza_tbl.narc`:
Renegade Platinum, Complete or Classic, and Following Renegade Platinum) battles with its own numbers, and Fairy.

- **Data** (`src/port/gen5_data.c`, `Gen5Data_Prepare`, called before each Gen 5 battle): copies of BW2's
  `a/0/1/6` (Pokémon) and `a/0/2/1` (moves) go into the game's folder (`gen5_data/`, rewritten when the game's files
  change: Classic and Complete differ in `pl_personal.narc`) and BW2 reads them through `Gen5_FilesOverride`
  (`gen5/src/files.c`). The Pokémon's base stats, types (Platinum's "???" 9 becomes Fairy, 17; the types after it
  shift down by one, as BW2 has no "???") and abilities (two slots; BW2's hidden one stays) come from the game, forms
  of Deoxys, Wormadam, Giratina, Shaymin and Rotom too. The moves' type, category (Platinum's physical/special/status
  is 0/1/2, BW2's 1/2/0), power, accuracy, PP and priority likewise. Kept from BW2: the experience a Pokémon gives,
  its catch rate, EV yields, and every move's effect (BW2's code decides what a move does from its ID: Renegade's
  changes to what moves do are not carried over, nor are its learnsets and evolutions, which Platinum's side keeps).
  Abilities are BW2's code: Sturdy and the like work as in Gen 5, as decided.
- **Fairy** (`gen5/patches/src/ov167/btl_calc.c.patch`, `gen5/tools/gen_headers.py`): BW2's types become 18: Fairy is
  type 17 and the "no type" `TYPE_NULL` 18 (the header generator rewrites `constants/types.h`). `TYPE_CHART` is
  18 by 18, filled by `Gen5_PortTypeChart` from the ROM's own chart (`RomPatches_TypeChart`: Renegade's has Fairy
  and its changes; Following Renegade's is in the hack's code blob, which `rom_patches.c` now reads); BW2's own
  chart, with Fairy neutral, until it is called. Checked: Fairy on Dragon double, Dragon on Fairy none, Poison and
  Steel on Fairy double. `gen5/tools/amend_patch.py` adds changes to an existing patch.
- **Fairy's icon** (2026-11-11, `MakeFairyIcon` in `gen5_data.c`): BW2's type icons are 32 by 16 pixels in the menus'
  common archive (`a/0/8/2`, members 0x22 + type; the colors are member 0x21). "FAIRY" is made at the first battle from
  the letters of FIRE (F, I, R), GRASS (A) and FLYING (Y), pink with colors 8 and 9 of the third palette, which no icon
  uses, and put into that archive's copy (`gen5_data/a/0/8/2`, read through `Gen5_FilesOverride`) as member 0x33,
  Cool's, which BW2's battle never shows. `app_menu_common.c.patch` gives type 17 that file and palette. The move
  buttons' own pink is Psychic's palette. Seen in a battle with a Fairy move.
- **The replaced moves** (found while doing the icon): Renegade Platinum replaces eleven moves in their slots: Barrage
  (140) is Draining Kiss, Submission (66) Play Rough, Luster Purge (295) Dazzling Gleam, Mist Ball (296) Disarming
  Voice, Lunar Dance (461) Moonblast (all Fairy, none in BW2), and Brine, Constrict, Horn Drill, Sand Tomb, Twister and
  Volt Tackle become Scald, Icicle Crash, Drill Run, Bulldoze, Hurricane and Wild Charge (all in BW2); Charm (204),
  Moonlight (236) and Sweet Kiss (186) are only retyped Fairy. BW2 knows a move by its number, so `gen5_data.c`
  (`sReplacedMoves`) maps each slot, when the game's move there is the new one (its type and power), to a Gen 5
  number for the battle and back after it: the six BW2 has to their own, and each Fairy move to a Gen 5 move Renegade
  can't have (Night Daze 539, Snarl 555, Struggle Bug 522, Razor Shell 534, Horn Leech 532), whose data gets the Fairy
  move's numbers and effect (stat drop, drain, sound flag) and whose name, in the names (bank 403) and in the "X used
  move!" messages (bank 16, three lines per move, the name in the text), becomes the Fairy move's.
  `Gen5_PortNoHandlers` (`move_handlers.c.patch`) drops BW2's handlers for those five numbers, so they do only what
  their data says.
- **The Fairy moves' animations** (2026-11-11): from Moon Black 2 (below). A copy of its `a/0/6/5` put at
  `gen5_data/moon/a_0_6_5` in the game's folder (the owner's own ROM's file, never in the repository) makes
  `gen5_data/a/0/6/5`, BW2's scripts with Moon's Moonblast (its 461), Dazzling Gleam (552), Disarming Voice (545) and
  Play Rough (285) in the proxies' slots; without it the proxies' own animations play. Draining Kiss keeps Horn
  Leech's (Moon has none). Checked with screenshots every 20 frames (`fa_*.txt` scripts, Turtwig against a Magikarp):
  each plays on the right Pokémon and lines up (the moon over the user for Moonblast; pink rings from user to target
  for Dazzling Gleam; rings on the user and a yellow flash on the target for Disarming Voice; stars and dust on the
  target for Play Rough), and the text names the move. The stamp in `gen5_data/stamp.txt` has a version
  (`DATA_VERSION`) besides the game's hashes and Moon's, so the files are made again when the code changes.
- **Not done:** the Pokémon's types on BW2's own screens; Renegade's move
  effect changes beyond the replaced moves. Not needed (checked 2026-11-11): Hidden Power is never Fairy, and
  Renegade has no Pixie Plate and no Roseli Berry (its item names have neither), so Judgment and Natural Gift are
  never Fairy either. Also not done: the experience and catch rate from the game's data.
- **Tests:** `DATA=~/thor-dev/desktop-renegade tools/docker/desktop.sh test tests/scripts/gen5_battle.txt --from
  rn-turtwig` (fixtures made with `new_game.txt --keep rn-before-starter` and `turtwig.txt --from rn-before-starter
  --keep rn-turtwig`, in that data folder) passes; the log says "the game's own Pokemon and move data made" and the
  chart's Fairy values. A battle against a Clefairy (species 35, Fairy in Complete) runs.

### Moon Black 2 as a source of the Fairy moves (2026-11-11, findings; used for the animations)

`~/thor-dev/Pokemon Moon Black 2 (v4.2.3).nds` is Black 2 with 51 files changed (same file system, same move count).
Its Fairy moves replace Gen 5 slots: Play Rough 285 (Skill Swap's), Moonblast 461 (Lunar Dance's, with Lunar Dance's
animation unchanged), Disarming Voice 545 (Searing Shot's), Dazzling Gleam 552 (Fiery Dance's), Geomancy 554; its
type is 17. Their animation scripts (`a/0/6/5`, member = move number) reuse existing particle resources (e.g. 474, 317,
231, which BW2 has): copying members 285, 545 and 552 into the proxy slots' scripts should be enough, no new
resources. `pokebw2/tools/script/btlv_effvm_script.py disasm` (Python 3.11+, run it in Docker) shows them. Handlers
are code: the moves' slots in `move_handlers.c` keep the old moves' handlers (Lunar Dance's, Searing Shot's), so a
slot used for a Fairy move needs its handler dropped when the game is Renegade (a hook in the handler lookup).
Renegade's own slots 285 (Skill Swap) and 461 conflict: Play Rough needs a free slot (above 467) whose script is
Moon's 285. Moon's type icon (`a/0/8/2`) and names (`a/0/0/2`) could replace the generated ones. Done: the scripts go
into the proxies' slots (above), the handlers are dropped; the icon and names stay generated (no ROM needed for them).

**Experience crash (fixed 2026-11-11):** `AddExpAndEVs` (`btl_server_flow_sub.c`) divides the experience by the number
of standing Pokémon that faced the defeated one, which can be 0: the DS gives 0 for a division by zero, x86 traps
(SIGFPE, seen when `gen5_battle.txt` failed to run and won the next turn). The patch makes it 0.

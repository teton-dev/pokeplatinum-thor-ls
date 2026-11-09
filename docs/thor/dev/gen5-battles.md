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
- **Testing: a local Linux desktop build on the Mac** (Docker), not only cloud builds and the Thor.
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
   desktop, but for the arena's distant background and sound* (see "Milestone 3").
4. **Bridge:** real wild battles through the setting, with results written back: HP, experience, levels, catching
   (Pokédex entry, nickname, box), fainting and whiting out.
5. **Trainers:** single trainer battles: trainer sprites, AI, prize money, win and lose messages.
6. **The rest:** doubles, tag battles (Cheryl, Riley...), multi battles, the catching tutorial, the Safari Zone,
   Pal Park, legendaries and roamers, the Battle Frontier; items from the bag; weather and terrain arenas.
7. **Renegade Platinum:** the game's own Pokémon, move and item data in the Gen 5 engine, and Fairy.

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
  about 3 s on the Mac. Download Play's and Wi-Fi's files are left out. They go into `gen5.tmp/`, which becomes the
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
- **Not checked yet:** the Android build (no NDK on the Mac; `std::filesystem` and `std::thread` need nothing the
  NDK lacks) and the import on the Thor, where 270 MB more goes into the app's folder.

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
- **Sound is silent for now:** NitroSystem's sound library holds one sound archive, Platinum's. `gen5/src/sound.c`
  stands in for `snd_sys.c` with the 22 functions the battle calls, keeping only the music fades' timing (the battle
  waits for them); cries get no wave, which `PokeVoice` never plays. Playing BW2's music and cries needs its own
  archive loaded next to Platinum's, later.
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
    `meson subprojects update --reset libntr`). Not yet: the far plane's "draw or drop" bit of the polygon attributes (the DS drops a
    polygon that crosses the far plane unless bit 12 is set; libntr clips it).
  - Sound (above). A block of 0 bytes is left in one of BW2's heaps at the end (listed, harmless).
  - The Android build, as for milestones 1 and 2.

## Risks

- **64-bit layouts** in BW2 data loaded from files or shared with the SDK: found case by case at runtime.
- **Two games' systems in one process:** BW2's GFL and Platinum's systems both expect to own the DS's VRAM, OAM,
  3D engine, sound and interrupts. The battle must leave them as Platinum expects.
- **The DSi SDK:** BW2 is a DSi-enhanced game; anything it uses from the TWL SDK that libntr lacks needs adding.
- **Data mapping:** Platinum's trainer classes, terrains and battle types onto BW2's.

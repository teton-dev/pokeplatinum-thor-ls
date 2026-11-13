# HeartGold and SoulSilver (draft plan, 2026-11-12)

The goal: the owner's HeartGold (and SoulSilver) ROM played natively on the Thor the way Platinum is here, with full
widescreen, BW2's battle engine (`gen5-battles.md`), 60 FPS and HD texture packs.

Status: **milestone 1 under way** (2026-11-12). The fork is `teton-dev/pokeheartgold-thor-ls` (branch
`thor-android`), cloned at `~/pokeheartgold-thor-ls` with `upstream` = `SilasMayes/pokeheartgold`. Its desktop build
runs in Docker and boots the owner's HeartGold ROM (sha1 matches retail) through the intro and title into a new game.

### Running the desktop build

`tools/docker/desktop.sh` in the HGSS fork (branch `desktop-docker`; its `tools/docker/README.md`): `build`,
`start` (VNC on port 5901), `shot`, `key`, `hold`, `type`, `click`, `log`, `where`, `stop`, `GAME=ss`. Data folder
`~/thor-dev/desktop-hg`. Findings from setting it up:

- Its image is Ubuntu 24.04 with gcc 13, as upstream's CI, plus flex and bison (libntr's `makelcf`). A clean build
  takes about 15 minutes on 8 cores under Rosetta. Platinum's Debian image has gcc 14, which makes some of the
  decompilation's pointer mismatches errors (first: `src/application/main_menu/agb_cartridge.c`).
- On the first start the desktop build asks for the ROM in an ImGui file dialog and extracts it into the working
  directory; the game has to be restarted after (the script does both).
- **It needs `firmware.bin`** (the port's DS Wi-Fi settings block) in the data folder, or it segfaults at boot.
  The APK ships HGSS's `res/pc_port/dwc_nv.bin` as that file; the script copies it in for the desktop.
- `[assert] GF_ASSERT failed` from `GetOverlayRamBounds` (`src/poke_overlay.c`) at every overlay load is harmless
  and Platinum has it too (asserts only log on the port): on 64-bit, `FSOverlayInfoHeader` holds pointers, so the
  overlay table libntr reads from `header.bin` lands `file_id` on the ARM9 load address (0x02000000) and the
  lookup fails. Overlays are native code on the port anyway.
- Keys over xdotool: A = `z`, B = `x`, Start = `Return` (`tools/port/testrig/sim_config.ini`). Upstream's Linux test
  rig (`tools/port/testrig`) relies on gdb, which doesn't run under Rosetta here.

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

Scripted tests (2026-11-12): `lib/port/test_script.c` in the HGSS fork is Platinum's runner with the commands that
don't reach into the game (no item, give, wild, trainer, jump...), on HGSS's hooks; `desktop.sh test`, fixtures in
`~/thor-dev/fixtures/hg`. `new_game.txt` plays a new game (ETHAN) to the bedroom, map 64 at (6, 6), and keeps it as
`new-game`. The APK (`desktop.sh apk`, dev flavor `org.pokeheartgold.port.dev`) builds with the same volumes as
Platinum's and installs on the Thor; its first launch (the ROM picker) is untested.

Left in milestone 1: scripts on to Sprout Tower (upstream's own `tools/port/testrig/nav` scripts know the route,
through gdb). The APK runs on the Thor (first launch and the ROM import done by the owner).

### Platinum fork's fixes in HGSS (2026-11-12)

- **Launched from the Thor's lower screen** both DS screens were drawn there (the bottom screen's Presentation skips
  the game's own display and the main one). Fixed in both games' `PlatinumActivity` / `SecondaryScreen`: the
  activity moves its task to the main display, and the bottom screen is placed again on configuration changes.
- **libntr**: HGSS pins the same `teton-dev/libntr` commit (578c6a8, the left stick as a D-pad) with the same 18
  patches and `libntrsystem_anim_edge.patch`. The chain only applies with `patch` (meson's first choice); `git
  apply`, its fallback, refuses `libntr_wide_mirror.patch` and the ones after it on a clean checkout, here too.
- **Widescreen overworld** (`lib/port/widescreen.c` in the HGSS fork): the field camera widened, the weather's
  sprites and BG2 across the sides (`WeatherManager_PortLayers`), Flash's darkness 4:3. Battles and menus stay 4:3
  (Gen 5 battles will bring their own). `widescreen_newbark.txt` checks it.
- **Sprite depth bias** on HGSS's billboards (`BillboardLists_Draw`), as Platinum's `billboard.c`.
- **60 FPS** (`lib/port/frame_rate.c`): the overworld's logic at 60 Hz while the player is free, with the speed
  fixes (walk, run, bike, turn, ledge jumps, variable-speed walks, people's waits, sprite and effect animations,
  map textures and props, the field's 3D animations, the weather's tasks, the surf mount's bob) and the Platinum
  fork's settings (`sim_config_prj.ini`, *Settings > Gameplay*, `lib/port/settings_pages.cpp`). Not yet: the
  rest of Platinum's list where HGSS has its own effects (footprints, emotes, shadows' fades...), checked by eye.
- **Gen 5 battles** (2026-11-12): HGSS builds this repository's `gen5/` through `./gen5`, a symbolic link to
  `../pokeplatinum-thor-ls/gen5` (this fork has to be cloned next to the HGSS one, as pokebw2 is), with
  `-Dpokebw2`; the only game function the Gen 5 code calls (`BrightnessController_Update`) is HGSS's
  `DoAllScreenBrightnessTransitionStep` (`lib/port/gen5_game.c`). *Settings > Gen 5 battles* imports the BW2 ROM
  (`src/port/gen5_rom.cpp`, copied: keep the two the same) and has the setting and a debug wild battle.
  `lib/port/gen5_battle.c` is `src/port/gen5_battle.c` on HGSS's `BattleSetup`, started from `Battle_LaunchApp`:
  wild battles (party, bag, arena, results, catching into the party or the PC) and trainers (below). `gen5_wild.txt`
  plays one to the end. Next: evolution after a battle, the caught Pokémon's naming,
  Apricorn balls, the Safari Zone and Bug-Catching Contest. Later, `gen5/` and the game-neutral parts of the bridge
  (`gen5_rom.cpp`, the conversions) move to a repository of their own (milestone 3).
- **White HP boxes** (2026-11-12): HGSS's Gen 5 battles show White 2's light HP boxes. Both versions' palettes are in
  either ROM's archive 11 (0x1ae and 0x1b0 White 2's, 0x1af and 0x1b1 Black 2's), so a Black 2 ROM is enough;
  `gen5/`'s gauge patch picks them at run time (`Gen5_SetWhiteGauges`, default dark: Platinum unchanged).
- **Team Rocket's arena** (2026-11-12): `lib/port/gen5_rocket.c` in the HGSS fork, a copy of this fork's Galactic
  arena (`src/port/gen5_galactic.c` and `Gen5Data_PrepareGalacticArena`) with every streak Rocket red, the same
  darkened room and stars, a red rocket ship and a crescent moon in place of Jupiter and Saturn, and a dark crimson
  stage; background 16 terrain 1, for the grunts, the four Executives and Giovanni (`IsRocketClass`). Only seen
  through `POKEHEARTGOLD_GEN5_ARENA=16,1` so far: HGSS's trainer battles don't go to BW2's engine yet. Found: in
  the widescreen battle a texel of the sky is about 0.8 as wide as high on screen (not 1.8 times wider, as the
  Galactic notes say), the texture is seen mirrored, and the ribbons' dark edges cover its rows below about 15.
- **Trainer battles** (2026-11-13): single, double (one trainer's two), two trainers (TAG) and beside a partner
  (MULTI with AI), including the losable ones (`BATTLE_TYPE_11`) and Silver's first (`BATTLE_SPECIAL_FIRST_RIVAL`);
  Frontier, link, Safari, Bug Contest and the tutorial stay HGSS's. `gen5/` takes the game (`Gen5_SetGame`, Gen 4's
  version number): HGSS's class table in `gen5/src/trainers.c` and HeartGold or SoulSilver as its Pokémon's origin
  game. HGSS numbers its AI flags as Platinum does, so the table is shared. Sprites: BW2's where it has the same class
  or person (the generic classes it shares, Johto's and Kanto's Gym Leaders, Giovanni, Blue, Lance, Red), HGSS's own
  (`a/0/5/8`, the same format as Platinum's, converted as Platinum's are) for the rest: Team Rocket, Silver, the Elite
  Four, Kimono Girls, Sages... (`TrainerClassHasBw2Sprite`). Names, class names (`msg_0730`), defeat lines (`TRMSG_LOSE`,
  or both `TRMSG_DBL_LOSE_*` for a pair), prize by class (`sPrizeMoneyTbl`), victory music by class (`SEQ_GS_WIN1`,
  `WIN3` for Gym Leaders, the Elite Four and Lance, `WINBRAIN`). Arenas: Rocket's (16, 1); Gym Leaders on the dark
  stone hall (4, 19); Will, Koga, Bruno, Karen in BW2's Elite Four rooms (12 to 15), Lance in the Champion's (16, 0);
  Silver in a town (`MapHeader_GetMapType`, `MAP_TYPE_CITY_TOWN`) gets the grass field with a dirt path (0, 1), not
  its plain green default (terrain 0) — his first battle, outside the lab in New Bark Town, and any other he starts
  in a town; his battles elsewhere (routes, Burned Tower, Victory Road, Mt. Silver) keep whatever the field gives.
  Tests: `gen5_trainer.txt` (a Rocket Grunt), `gen5_rival.txt` (Silver in New Bark Town), and the runner's
  `trainer ID [ID2]`. With more than one Pokémon a
  mashed A loops in BW2's "switch?" prompt and party screen after a knockout: keep test parties to one. Not done:
  Mom's savings out of the prize, the trainers' mid-battle lines (`TRMSG_LAST_POKE`...), the player's back sprite
  (still the Platinum fork's Lucas and Dawn), the Frontier.
- Platinum's rain fix doesn't apply: HGSS's decompilation types the drops' work (`WeatherParticle`).
- **Battle particles** (2026-11-13): HGSS's own battles drew no move particles (Tackle's burst and dust). Its SPL
  (`lib/src/spl_draw_ptcl.c`, unlike Platinum's `lib/spl`) writes the geometry registers directly
  (`reg_G3_VTX_10 = ...`), and on the port those are plain variables the renderer never reads: only the `G3_`
  functions reach it. The file's writes are macros now, `G3_Direct0/1`, `G3_Translate` and `G3_Scale` on the port
  and the registers on the DS. `graphics_object_manager.c`'s scale (the dress-up) likewise. Still writing registers
  on the port, unchecked: `aprijuice_screen.c`, `dress_up.c`, `safari_decoration.c`, `plaza_game_swalot_plop.c`,
  `pokeathlon_lamp_jump.c`, `wifi_plaza_visitor_profiles.c`. `battle_particles.txt` checks Tackle.
- **Starter balls' spotlight** (2026-11-13): tiled across each ball, with an arc over it. It is a translucent A5I3
  texture with repeat off, and libntr's deferred translucent pass always set `GL_REPEAT`
  (`libntr_g3_tex_wrap.patch`: every pass takes the polygon's repeat and flip, and clamps to the edge texel as the
  DS does, where the opaque pass used `GL_CLAMP_TO_BORDER`). `starter_balls.txt` (new-game to Elm's machine) shows it.
  Platinum shares libntr: the patch belongs in its chain too. libntr also lacks the normal and vertex texture
  coordinate sources (TEXIMAGE_PARAM modes 2 and 3); neither scene uses them.
- Found on the way: on the port the overlay asserts (`GetOverlayLoadDestination`, `GetOverlayRamBounds`) always
  fail, and with communication set up an assert resets the game through the error screen; continuing a save hung.
  They are quiet on the port now.

## Decisions needed from the owner

- ~~Base the work on upstream's `thor-android` HGSS port~~: decided, forked 2026-11-12.
- Whether HGSS now moves ahead of Renegade Platinum (CLAUDE.md and `gen5-battles.md` put Renegade, milestone 8,
  first).
- Whether `hg-engine` matters at all for this (recommended: not as a base; maybe later as a hack target).
- "HD 3D models": the port has HD *texture* packs (Dolphin-style PNG replacement); replacing models themselves is
  new work, with no format or source for HD models yet. Texture packs for HGSS are milestone 6.
- Fork-only, as with Gen 5 in Platinum (GPL); ask upstream's developer before proposing any of it there.

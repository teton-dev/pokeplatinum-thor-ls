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
  plays one to the end. Next: the Bug-Catching Contest (the rest is below, "After the battle" and "The Safari Zone").
  Later, `gen5/` and the game-neutral parts of the bridge
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
  the Frontier (the rest is below, "After the battle").
  The player's back sprite is Ethan or Lyra (2026-11-13, below).
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
  Platinum's chain has it too (its `first_battle.txt` and `gen5_battle.txt` still pass). libntr also lacks the normal and vertex texture
  coordinate sources (TEXIMAGE_PARAM modes 2 and 3); neither scene uses them.
- **The wild battle's own transition, widened** (2026-11-13): the black or white tiles that flip the overworld away
  before any battle starts (`EncounterEffect_WildRows*`/`WildDiagonals*`/`WildWave*`, effects 0-5 of
  `gEncounterEffectFuncs`) draw on a 256-wide window (BG3, and BG1 for the wave), confined to the DS's width even
  with widescreen on, since the field isn't "shown" (`ShownFieldSystem`) while it plays: a narrow box over the
  (already wide) field for its whole length, then a cut to the wide battle. `lib/port/widescreen.c` now widens it
  (BG3's edge extended) while it plays. The flag (`EncounterEffect_PortWildPlaying`, `src/battle_transition_task.c`)
  is set around `EncounterEffect_Start`, not inside the six effect functions themselves: `Task_BattleTransition`'s
  own poll only changes twice (started, and `env->done`), but `Widescreen_Update()` (`src/main.c`'s loop) reads the
  static flag every real frame in between, so it stays wide for the whole transition regardless. Trainer battles'
  own transitions (spin, ball drop, the Leaders' and Rival's) aren't touched. Test:
  `widescreen_wild_transition.txt` (a stillshot mid-flip, `tests/README.md` says what to check for).
- **The Pokégear's own gray phone bezel** (2026-11-13): full-screen apps over the field (the Bag, the Pokédex...)
  already widen their own background to the window's edges with panels staying in the 4:3 middle (as the Platinum
  fork does); the Pokégear is the one of them that should look like a phone doing it, so `widescreen.c`'s new
  `WideApplication` (checking `fieldSystem->processManager->child->template.init` against `Pokegear_Init`) widens its
  background (`GF_BG_LYR_MAIN_0`) the same way, and `UpdatePokegearBezel` draws a gray bezel with a speaker grill
  over the gap on each side (two `SIM_WideStamp`s of one 16×64 image, the right one `flipX`, on the same layer,
  `SIM_SetWidescreenStampImage`/`Stamps`).
  The desktop build couldn't reach the Pokégear screen to check this directly: the start menu's Pokégear slot never
  rendered there even with `FLAG_GOT_POKEGEAR` set and read back true (a new `flag N` test command helped confirm
  that, `tests/README.md`), though `StartMenu_Init`, `StartMenu_BuildActionLists` and
  `FieldSystem_GetStartMenuButtonInhibitFlags_Normal` all read as correct on inspection. On the owner's Thor it's
  reachable fine (Elm's lab, the starter, Mom), so that's a desktop-only quirk, not a real bug — the actual bezel
  was checked on hardware instead, where it showed the background correctly widened but **no bezel graphic at
  all**, just the blue background's own sides. The cause: `SIM_SetWidescreenStamps`'s stamps only ever composite
  inside `G2SIM_DrawBGWide` (`subprojects/libntr/libraries/sim/src/sim_g2.cpp`), which only runs for a BG in
  `SIM_SetWidescreenSceneOptions`'s `wideBgLayers` mask; `WideApplication` had put the Pokégear's background in
  `edgeBgs` (`SIM_SetWidescreenBgEdgeExtend`) instead, which widens a BG's picture but never calls
  `G2SIM_DrawBGWide` at all, so the bezel's stamps never got a chance to draw. Switched to `wideBgs` and re-checked
  on hardware: **still just the blue background's sides, no bezel**, so that wasn't the whole story either. Left
  as a to-do; the generic mechanism was also checked against the Options app (reachable through the
  start menu on desktop, unlike the Pokégear) along the way: its own backdrop (also `GF_BG_LYR_MAIN_2`, per
  `options_app.c`) didn't widen at all even with `edgeBgs`, for a different reason — the Options screen swaps which
  hardware engine drives the top screen (`sWideSubTop` in `sim_main.cpp`), landing the edge-extend bit on the wrong
  engine's draw call. That Options branch was dropped rather than pursued further, since it was never asked for.
- **No music** (2026-11-13): no song started anywhere (the title, towns, routes, Pokémon Centers, battles):
  every `NNS_SndArcPlayerStartSeqEx` failed to load its sequence or wave archive, because the sound heap
  (`SND_HEAP_SIZE`, `src/sound.c`) was full: 6,560 bytes free before the title's 17,820-byte sequence. The port's
  data takes more room than the DS's; `heap_buf` is twice the DS's size on the port now, as Platinum's
  `SOUND_SYSTEM_HEAP_SIZE*2`, with 914,912 bytes free after the global data, and every song starts.
- **Trainers' arena on paths** (2026-11-13): any trainer battle on a town's or a route's ordinary ground
  (`TERRAIN_PLAIN`, a general or city background) is on BW2's grass field with a dirt path (terrain 1), as Silver's
  town battles were (`ArenaFromSetup`); tall grass, sand, water and the rest keep their own. `gen5_trainer_arena.txt`
  checks a town; routes aren't scripted (the game keeps the player in New Bark Town until Elm's scenes are done).
- **Cut-ins in widescreen** (2026-11-13), as the Platinum fork's: the Gym Leaders' (and Blue's and Silver's) banner,
  BG3 behind the zigzag wipe's window, is drawn wide and mirrored while `EncounterEffect_PortGymLeaderPlaying`; the
  Elite Four's and Lance's band's purple slashes (sprites 2 and 3, the "nameplates") go to sprite priority 2, which
  libntr's OBJ edge extend continues to the sides, the portraits keeping 1; Red's (effect 46), black blocks filling
  BG3 like the wild transitions, is widened with them (`EncounterEffect_PortWildPlaying`). Unlike Platinum's, HGSS's
  leader banner has no sliding bars. `widescreen_cutins.txt`.
- **X's menu in widescreen** (2026-11-13): its gray bar on the top screen (BG3, a vertical gradient loaded while the
  menu's cursor is active, `start_menu.c`) stopped at the 4:3 edges; while `StartMenu_PortBarShown` its rows are
  continued to the sides (BG edge extend). `widescreen_xmenu.txt`. Seen once on the way, unexplained: thin white
  diagonal lines across New Bark Town's 3D for a frame, with the menu open.
- **The Bug-Catching Contest's swap screen in widescreen** (2026-11-17): the screen after a contest catch ("Caught
  CATERPIE!", keep or swap, `BugContestSwapMon_Init`) is one of `WideApplication`'s apps now, its four layers' edge
  columns continued. Its top screen is the sub engine (the main engine, with the Pokémon and text, is on the
  bottom), and its picture is BG0, which libntr's shader never edge-extended (only BG1 to BG3): the sides showed the
  backdrop's lavender. `libntr_wide_edge_bg0.patch` gives BG0 drawn as 2D the same edge, mirror and stripe handling
  (only this screen asks for it so far). The sides continue the panel's dark-green border. Checked with
  `gen5_contest_catch.txt` run in widescreen (`SCREEN=1920x1080` so the X display holds both windows for its
  stillshots); `widescreen_xmenu.txt` unchanged. Not done: HGSS's Pokédex entry, naming screen and evolution after
  a Gen 5 battle are still 4:3 (Platinum's are wide, `gen5-battles.md`), as are its other apps (Bag, party...).
- Found on the way: on the port the overlay asserts (`GetOverlayLoadDestination`, `GetOverlayRamBounds`) always
  fail, and with communication set up an assert resets the game through the error screen; continuing a save hung.
  They are quiet on the port now.

### Gen 5 battles: after the battle, Apricorn Balls, trainers' lines (2026-11-15)

- **Naming and evolution** (`lib/port/gen5_battle.c`'s `Main`): the Gen 5 overlay replaces HGSS's whole battle
  overlay, whose end runs the catch's naming screen and the evolutions (`battle_022378C0.c`'s `BSTATE_EVOLUTION_*`), so
  it now does both after BW2's battle, as this fork's bridge does: a caught Pokémon gets HGSS's naming screen (its own
  heap, `HEAP_ID_BATTLE`; "sent to the PC" under the name when the party is full), then each Pokémon that gained a level
  and evolves (`BattleSystem_CheckEvolution`, `EvolutionScene_New`, as HGSS's flags). Tests `gen5_catch.txt`,
  `gen5_evolution.txt`.
- **Losing a debug or test wild battle**: HGSS's encounter task leaves a loss to its caller (a script whites out), and
  the debug window's and test runner's wild battles didn't, so a loss left the game with no overworld. Both now white
  out (`CallTask_Blackout`), as their trainer battles did. Test `gen5_wild_loss.txt`.
- **Apricorn Balls**, by HGSS's rules (`battle_command.c`): BW2's `CheckBallCapture` (patched) takes the catch rate from
  `Gen5_PortApricornCatchRate` (Fast, Level, Lure, Heavy, Love, Moon: HGSS's formulas, the Heavy Ball's catch-rate quirk
  kept) and the Sport Ball's 1.5x, only with `Gen5Battle.apricornBalls` (HGSS sets it; Platinum doesn't). BW2 has no
  battle data for them: `pml_item.c` (patched) gives them a Poke Ball's (their number, name, icon stay theirs), and the
  battle bag's own table (archive 228, `b_bag_item.c`, patched) puts them with the Poke Balls; BW2's throw sprites stop
  at the Dream Ball's (`btlv_effvm.c`, patched), so they fly as a Poke Ball looks (BW2's particles cover all 25 balls).
  The flag is set before the battle's bag is filled. A catch comes back as a ball number (Level Ball 18), which HGSS's
  catch data takes as an item (493): converted; a Friend Ball's catch gets friendship 200. `Gen5Battle.fishing` gives
  BW2 its fishing flag (the Lure Ball). Test `gen5_apricorn.txt`.
- **Mom's savings** needed nothing: HGSS moves a quarter of the money won to Mom when it copies the battle's results to
  the save (`BattleSetup_CopyResultsToSaveData`), whatever engine ran it. HGSS's line about it ("Sent some to Mom!"
  under the prize line) is shown since 2026-11-15 (below). Test `gen5_mom_savings.txt`.
- **Trainers' lines during the battle**: BW2 shows them itself (`btl_client.c`'s hints, at first hit, half HP, last
  Pokémon, last at half HP: its messages 0x11 to 0x14), read from BW2's trainer tables by number. A port trainer (from
  0x800) now has them in `Gen5Trainer.hintText`, which HGSS fills from `TRMSG_HIT_POKE_FIRST_TIME` to
  `TRMSG_LAST_POKE_HALF` (Silver, the Gym Leaders, the Elite Four and Lance have the last two); BW2's check and message
  (`btl_client.c`, `btlv_core.c`, patched) ask the port. Platinum fills them too (2026-11-15). Test `gen5_trainer_lines.txt`
  (Falkner: give the test's Pokémon Swift, his birds' Sand-Attack makes anything else miss).

### The Safari Zone in Gen 5 battles (2026-11-15)

- HGSS's Safari rules are Platinum's (`battle_controller_player.c`'s bait and mud, the opponent's flee check with
  `gSafariFleeRates`, the catch stages), so `gen5/src/safari.c` plays them unchanged. `Gen5Battle_Wants` lets
  `BATTLE_TYPE_SAFARI` through; the bridge passes `setup->safariBalls`, the species' `BASE_CATCH_RATE` and
  `BASE_GREAT_MARSH_RATE` (its flee rate), and gives back the balls left (HGSS's `Task_SafariEncounter` writes them to
  the save's counter and, at 0 with no catch, takes the player to the gate) and `BATTLE_OUTCOME_FOE_FLED` when it ran.
- The BALL, BAIT and ROCK labels are tile art the port adds to its copy of archive 11 (members 574 to 586, after the
  arena's two models): HGSS's Team Rocket arena maker (`lib/port/gen5_rocket.c`) predates them, so a Safari battle
  crashed loading the command screen. It now adds them as Platinum's does (`MakeSafariButtons`, copied unchanged;
  `ROCKET_VERSION` 5 makes the archive again).
- HGSS's flee rates aren't Platinum's: Chansey flees (90), Bulbasaur and most others never do (0), Kangaskhan 150. And no
  Safari Ball is a sure catch at full HP, even at catch rate 255 (about one in two).
- Tests (`TEST_ENV=POKEPLATINUM_GEN5_SAFARI_SEED=3`): `gen5_safari.txt`, `gen5_safari_catch.txt`,
  `gen5_safari_end.txt`, with the runner's `safari SPECIES LEVEL [BALLS]` (the Safari flag and ball counter set, the
  gate where the player stands) and `editor safari N`.
- Not done: arenas chosen per Safari area (each battle gets what the field gives, as any wild battle), the Nuzlocke
  catch rule there, and a check in the real Safari Zone (the tests start the battles where the player stands).

### The Bug-Catching Contest, legendaries, roamers and the rest (2026-11-15, second session)

- **The Bug-Catching Contest** goes to BW2's engine (`Gen5Battle_Wants` takes `BATTLE_TYPE_BUG_CONTEST`). The contest's
  Sport Balls (`setup->safariBalls`) are the battle bag's only item; HGSS's BAG throws one, BW2's opens its bag on
  them. A catch goes to the contest's slot (`setup->bugContestMon`, after the catch data, Pokédex and game stats; no
  naming screen, as HGSS's), for HGSS's own "Caught ...!" and swap screen. When the last ball misses the battle ends
  at once (`Gen5Battle.bugContest`; `btl_server_flow_sub.c`'s `UseBagItem` escapes silently when
  `Gen5_PortContestOutOfBalls`), and `Task_BugContestEncounter` takes the player to the judging. Test commands:
  `contest SPECIES LEVEL [BALLS]` (the contest started as the gate's `ScrCmd_BugContestAction` does, flag 0x996),
  `check contest-balls N`, `check contest-mon SPECIES`. Tests `gen5_contest_catch.txt`, `gen5_contest_out.txt`.
  Not checked: a whole contest from the gate in the real National Park, its arena (the field's), the timer.
- **Legendaries and roamers:** the flag the decompilation calls `canFlee` (`SetupAndStartWildBattle`, bit 8) is
  `BATTLE_SPECIAL_LEGENDARY`; it becomes BW2's legendary flag (its message has no "wild"). A roamer gets BW2's roaming
  flag (its AI flees) and keeps the HP and status the battle leaves (the runtime reads the wild Pokémon back;
  `Roamers_UpdateAfterBattle` stores them). The test runner's own wild battles are plain ones now. Test commands
  `legendary`, `roamer SPECIES LEVEL`; tests `gen5_legendary.txt` (Lugia), `gen5_roamer.txt` (Raikou flees).
- **Full boxes:** with the party and every box full no ball can be thrown (BW2's boxes-full flag), as in HGSS.
- **The nickname question:** "Give a nickname to the caught ...?" with BW2's Yes and No, in the battle after the catch
  messages (`btl_client.c`'s `ScBallThrow`, patched; BW2 asks in its field); the naming screen opens only on Yes. Not in
  the contest. Tests touch YES at (128, 55); `gen5_nickname.txt` answers No.
- **Mom's line:** with Mom's savings on, "Sent some to Mom!" under BW2's prize line (`Gen5Battle.momSavings`,
  `btlv_core.c`'s `BtlvCore_PortStartMsgPrize`; a page of its own if the prize line has two lines). Seen only in the log
  (`the prize line says some went to Mom`), not on a screenshot.
- **Obedience:** BW2 makes another trainer's Pokémon disobey above `(badges + 1) * 10`; it now uses HGSS's levels
  (`Gen5Battle.obeyLevel`: 10, 20 with a badge, 30 with two, 50 with the Fog Badge, 70 with the Storm Badge, all with
  eight). The runtime logs a party Pokémon BW2 doesn't count as the player's own (ID, gender and name).
- **Test bug found:** the runner's `give` used `OT_ID_PLAYER_ID`, which in HGSS's `CreateMon` means OT ID 0, so its
  Pokémon were another trainer's to BW2 and disobeyed: `gen5_trainer`, `gen5_trainer_lines` and `gen5_mom_savings` had
  been losing their battles while passing (they don't check the result). Fixed (`OT_ID_PRESET`); they win now.
- **Later the same day:** the Pokédex entry popup for a new species after the battle (`lib/port/gen5_dex_entry.c`:
  HGSS's `PokedexEntryPopup_Create` with the display the battle would give it; its own char transfer, so the sprite
  system's is popped meanwhile; font 4), also in the Bug-Catching Contest; the PC line for a catch with no nickname and a
  full party (msg_0197 1174 to 1177, shown by BW2's battle); Route 29's catching tutorial as BW2's capture demo (test
  command `tutorial`); the Apricorn Balls shaking on the ground in their own colour (palettes added to a copy of BW2's
  archive 94 by `gen5_rocket.c`, `ROCKET_VERSION` 7; BW2's own art throws and opens them). Tests `gen5_pc_message`,
  `gen5_tutorial`; `gen5_nickname` and `gen5_contest_catch` close the entry with A.
- Still to do in HGSS: the Battle Frontier, Pal Park, a whole Bug-Catching Contest from the gate on the Thor.

### Save editor and debug window (2026-11-15)

- **Settings > Save editor** (`lib/port/save_editor.c`, `save_editor_gui.cpp`): this fork's editor
  (`src/port/save_editor.c`, `gui_save_editor.cpp`) on HGSS's save APIs. Party and PC Pokémon, giving any Pokémon,
  .pk4 files, the bag, money, Battle Points, the 16 badges, play time, the National Pokédex. Left out: the trade
  box (HGSS has no save slots) and the legendary events (Platinum's Mystery Gift key items). PC edits mark their box
  modified (`PCStorage_SetBoxModified`): HGSS saves only the boxes that changed.
- **Developer windows > Application** (`lib/port/debug_window.cpp`, `debug_field.c`): warps to the 30 Fly points
  (named from the map each lands on: the decompilation's `SPAWN_` names don't follow `sSpawnPoints`' order, spawn 5
  is Cianwood) or any map and tile; the trainers on this map (object types 1-8 with a trainer script, as the sight
  checks read them), Go to (the tile it faces: it challenges the player as in the game), Battle, beaten on or off;
  any trainer by number or name; a wild battle. The developer windows draw while the game waits in its frame, so
  their buttons call the game directly (field tasks, as the test scripts' warp and battles).
- Tests: `save_editor.txt` and `debug_window.txt` (`editor` and `debug` commands call the same functions and check
  them). The windows were also clicked through on the desktop build.

## Decisions needed from the owner

- ~~Base the work on upstream's `thor-android` HGSS port~~: decided, forked 2026-11-12.
- Whether HGSS now moves ahead of Renegade Platinum (CLAUDE.md and `gen5-battles.md` put Renegade, milestone 8,
  first).
- Whether `hg-engine` matters at all for this (recommended: not as a base; maybe later as a hack target).
- "HD 3D models": the port has HD *texture* packs (Dolphin-style PNG replacement); replacing models themselves is
  new work, with no format or source for HD models yet. Texture packs for HGSS are milestone 6.
- Fork-only, as with Gen 5 in Platinum (GPL); ask upstream's developer before proposing any of it there.
- **Ethan and Lyra** (2026-11-13): the player's back sprite in HGSS's Gen 5 battles. `PlayerBack_ForTrainerClass`
  (`gen5/src/player_back.c`) gives Ethan (male) or Lyra (female) when `Gen5_GameVersion()` is HeartGold or SoulSilver,
  Lucas and Dawn otherwise. The owner's sheets (`gen5/art/sheets/trainer_ethan_fullspritesheet.png`,
  `trainer_lyra_fullspritesheet.png`) are converted by `gen5/tools/player_back.py convert` like the others. Lyra's is
  laid out as Dawn's (armless standing pose, arm pieces added). Ethan's poses have their near arms drawn in, so only
  the far arm is added to the throw, and BW2's "ball up" shows him standing, "ball higher" and "winding up" with his arm
  over his head. His sheet's backing is noisy (compression), snapped to one colour (`backing_tolerance`). Previewed with
  `player_back.py preview`, and now seen in an actual battle too (2026-11-14, `gen5_player_back.txt` and
  `gen5_player_back_girl.txt`, `tests/README.md`): Ethan's throw is clean. Lyra's has a to-do — her near arm (the
  one added to Dawn's armless pose) shows a gap of bare skin colour between her sleeve's cuff and the glove during
  the throw, where Ethan's drawn-in arm has none; `gen5/tools/player_back.py` or her sheet likely needs the glove
  moved in (or the cuff extended) to close it.
- **Catching up with Platinum's Nuzlocke mode and weather sync** (2026-11-14, scoping): since the last sync
  (606a8656e), the Platinum fork has gained Nuzlocke mode (per-save state and rules, the catch rule, dead Pokémon
  drawn gray in both engines, the alpha mark on the save screen and main menu, gift nicknames asked again, scripted
  tests) and, in `gen5/` (BW2's side of the bridge, shared with HGSS by the `./gen5` symlink): the weather staying
  on the arena and its ability bar at each turn's end, the weather's bar text centered, partner battles and sprite
  palettes, keeping the Pokémon's moves and the crit guard in BW2's engine, eggs staying eggs in battle. What's
  shared for free: everything under `gen5/` (the BW2-side patches and `gen5/src/runtime.c`'s hooks) needs nothing
  extra once HGSS's `./gen5` symlink is at this commit — HGSS's own `lib/port/gen5_battle.c` already builds against
  it. What needs a real port: Nuzlocke mode's engine and UI (`src/port/nuzlocke_mode.c` and `.h`,
  `nuzlocke_mode_game.h`: largely game-agnostic, behind a small per-game interface — a `nuzlocke_heartgold.c` to
  write, mirroring `src/port/nuzlocke_platinum.c`'s 150 lines; `nuzlocke_gray.c`'s gray icons, which lean on
  `pltt_transfer.c` and `pokemon_sprite.c`'s palette-slot patches; the mark on the save/main menu
  (`nuzlocke_mark.c`); and the UI touch points — party menu, PC boxes, the Summary screen, the battle party list —
  each ported file by file, not copied: **HGSS's decompilation names and organizes these very differently from
  Platinum's fork** (no `save_table.h` or top-level `savedata.h`; `include/save.h`, `include/save_pokegear.h` and
  the rest instead), even though the underlying constants match where checked (`MON_DATA_UNUSED_114`,
  `SAVE_PAGE_MAX`, `SAVE_SECTOR_SIZE`). `lib/port/gen5_battle.c` also needs the small changes Platinum's picked up
  (the dead flag into `Gen5Pokemon`, the partner's back-sprite art, egg handling). Not started; the per-game
  adapter (`nuzlocke_heartgold.c`) is the natural first milestone, since everything else layers on it.
- **Nuzlocke mode, wired up** (2026-11-14): the core and HGSS's adapter (above) now actually run.
  `NuzlockeMode_Update` is in `src/main.c`'s loop; fainting sets the dead flag and keeps a dead Pokémon's HP at 0
  (`src/pokemon.c`'s `SetMonData`/`AddMonData`); whiting out drops the run for good (`src/blackout.c`'s
  `Task_Blackout`). The catch rule hooks into both of HGSS's wild-battle tasks (`src/encounter.c`):
  `Task_WildEncounter` (tall grass, roamers) and `Task_StartEncounter`, which HGSS also reuses for trainer battles
  and scripted species-at-level encounters (legendaries; the test script's own `wild` command) — the hook there
  only tracks what the task's own Bill/Dex-call branch already treats as wild, and always clears a pending static
  mark so it can't leak onto a later random encounter; `SetupAndStartWildBattle`/`SetupAndStartFatefulWildBattle`
  mark themselves static, as Platinum's `Encounter_NewVsSpeciesAtLevel`/`Encounter_NewFatefulVsSpeciesAtLevel` do.
  For BW2's engine: `PokemonToGen5` now carries the dead flag, and a new `PortNuzlockeToGen5`
  (`lib/port/gen5_battle.c`) sets the level cap, crit guard and wild catch-refusal fields `gen5/src/runtime.c`
  already reads for `Gen5_PortCapExp`/`PortCritGuard`/`PortCatchRefused` — nothing to change there, since it's
  shared with Platinum through the `./gen5` symlink. Added a `nuzlocke` test command (mirroring Platinum's) to
  turn the mode on and log its state without a settings UI yet; a smoke test (mode on, a wild battle through
  `Task_StartEncounter`) ran clean. Not yet wired: HGSS's native (non-Gen5) battle engine's own crit-guard and
  catch-refusal checks, and item-use refusal on a dead Pokémon — their Platinum equivalents
  (`battle_script.c`/`battle_lib.c`/`item_use_pokemon.c`) don't exist under those names in HGSS's decompilation
  (still unk_-named overlay files); left as a to-do since Gen 5 battles are this fork's primary engine. No doubles
  wild support either, so only the first wild Pokémon is covered. Still no settings UI, gray icons, or the save/
  main-menu mark — Nuzlocke mode can only be turned on through the test script for now.
- **Settings > Challenges** (2026-11-17): Eterna's Nuzlocke mode section (pick a level, its rules, turn it on;
  level, crit guard, turn it off for good), in `lib/port/settings_pages.cpp`. Its rules text names only what
  Goldenrod enforces (dead Pokémon, whiting out, the catch rule, statics with Hard) and says the level cap, IVs of
  15, item and nickname rules aren't in yet. The settings library can't see the game's headers:
  `PORT_NUZLOCKE_MODE_GUI_ONLY` leaves `port_nuzlocke_mode.h`'s Pokémon functions out. Test command `settings PAGE`
  (as Platinum's); `desktop.sh` passes `SIM_NATIVE_SETTINGS(_EVENTS)`; `settings_challenges.txt` turns Normal on
  through the native recorder's events.
- **port-common in Goldenrod; Settings > Saves** (2026-11-17): upstream's shared layer (`subprojects/port-common.wrap`,
  the revision the Platinum fork pins, with its `port-common_nuzlocke_mode.patch`) and an HGSS adapter,
  `lib/port/game_hgss.c` (identity, the slot and autosave settings in `sim_config_prj.ini`'s new `[Saves]` section,
  the save summary, reload and image check, the overworld's "can save now", the game's own save). The Gear isn't
  built yet: `port-common_gear_option.patch` adds a `gear` option, off for Goldenrod, until it has the Gear's ~60
  `Gear_*` data adapters. Cheats, shiny hunt and the Nuzlocke helper are built but nothing calls them (their
  adapter functions are minimal). Hooks: `SaveSlots_Init`/`SaveSync_Init` before `SaveData_New` and
  `SaveSync_Update` in `src/main.c`; `SaveData_New` split so `SaveData_PortReload` loads another slot into the same
  SaveData, then `SaveSync_AtStartup` and `SaveSlots_OnLoaded`; `SaveSlots_OnSaved` after `SaveGameNormal` and
  the async write (`src/save.c`); `Autosave_FieldTick` in `FieldSystem_Control`; the main menu (L/R or the Saves
  page switch slots, fading out and back through the save check; "SLOT n/4" in the CONTINUE box; an empty slot
  shows the menu without CONTINUE). The Saves page and the "Saved" notice (`SIM::GUI::PrjOverlay`) are Eterna's,
  in `lib/port/settings_pages.cpp`. Gotcha: `PlayerAvatar_IsBetweenSteps` is TRUE when the player stands on a tile.
  Test `save_slots.txt` (the menu's slots, the page's cards, "Save now"); the APK builds. Not yet: the empty
  slot's menu doesn't say which slot it is (fixed: "SLOT n/4" on NEW GAME), and none of it has been tried on the Thor.
- **The Gear in Goldenrod, first part** (2026-11-17): port-common's Gear is built (its `gear` option on again; an
  existing build folder needs `meson configure ... -Dport-common:gear=true`, done for `build_linux` and
  `build_android/meson`), with `lib/port/gear_hgss.c`: the party (stats, moves, IVs/EVs, EXP), the trainer card,
  names (the save editor's caches), the species lookup (base stats, abilities, egg groups, evolutions both ways,
  level-up moves) and the pictures (party icons, front sprites decoded from the ROM's NARCs into malloc'd buffers,
  `UnscanPokepic` for the sprites). Empty for now: the battle tab and its menu (Goldenrod's battles are BW2's: Platinum's
  Gear reads a Gen 4 BattleSystem), encounters and where to find a species, bosses and the level cap, map names and
  the map, the PC tab, the Day Care, berries, renaming, TMs, egg and tutor moves. HGSS has no Pokétch: its tab is
  hidden. *Settings > Gear*, off by default while it's partial. Test `gear.txt` (the lower window shaped as the
  Thor's, `BottomWindowWidth=540 BottomWindowHeight=620`): the party tab with Cyndaquil and Pikachu.
- **The Gear in Goldenrod, second part** (2026-11-17): the Area tab from the map's own encounter table
  (`WildEncounters_ReadFromNarc`: grass for the time of day, morning/day/night, surfing, the three rods, a mass
  outbreak when the radio reports one there; rock smash, Hoenn/Sinnoh sounds and the night fishing slot aren't
  shown), location names (the map sections', msg 0279), and the bosses: Johto's eight leaders, Will, Koga, Bruno,
  Karen and Lance, Kanto's leaders in a usual order, then Red, their teams from the trainer data (custom moves, or the
  last four learned by level), beaten by their badge or the Hall of Fame. The level cap is the next boss's ace (only
  shown; Goldenrod's Nuzlocke level cap could now use it). Test command `gear` logs the Area and the next boss;
  `gear_area.txt` (Route 30 at night: Rattata, Spinarak, Hoothoot...; Falkner's Pidgey 9 and Pidgeotto 13, cap 13).
  Still empty: the battle tab, where to find a species, the map, the PC tab, the Day Care, berries, the rival.
- **The Battle Tower in Gen 5, ported from Eterna** (2026-11-17): `Gen5Battle_WantsFrontier` and the facility battle in
  `lib/port/gen5_battle.c` (no bag, money or Nuzlocke rules; the Frontier's lines from `msg_0197_00724`, entry id * 3
  + 2, or a player-made trainer's (0x2710) mail message; a forfeit is a loss; the World Tournament arena, light pools
  for the Brains, whose victory music HGSS's bridge already had), launched from `FrtCmd_TowerStartBattle`. **Found
  on the way, port bugs:** the Frontier's party selection (`FrontierFieldSystem_ReadPartySelection`) read the party
  menu's result at the DS's byte offsets (0x26, 0x27, 0x30) behind 64-bit pointers: every slot 255, "Your Pokemon
  must all be different". Fixed by reading `PartyMenuArgs`' fields (port only); the same padding trick in
  `PartyMenuArgsSlotView` (the Union Room's battle choice) fixed too. Then the Frontier's own scenes (after the save)
  crashed in `FrontierScriptContext_ReadHalfWord`: HGSS's Frontier script commands read their relative offsets as
  `u32` (`FrtCmd_GoTo`, `GoToIf`, `Call`, `LoadObjectGfx`, `CreateObjects`, `CreateSprites`, the script table in
  `FrontierScriptContext_GoToScript`), so a backward jump landed 4 GB past the script on 64-bit instead of wrapping
  as on the DS. They're `s32` now, as Platinum's (same ARM code). Goldenrod's Frontier runs on the port: `gen5_tower.txt`
  (from `new-game`) goes through the elevator to the battle room, the battle in BW2's engine ("a single facility
  battle against trainer 48"), RUN and YES (BW2's result 3) and back to the Battle Video question. Palmer too: test
  hook `TEST_ENV=POKEHEARTGOLD_TOWER_OPPONENT=305` (`src/frontier_opponents.c`, port only; the challenge's first
  opponent), `gen5_tower_palmer.txt`: Tower Tycoon Palmer in the light-pools arena.
  **2026-11-18:** no Battle Video question or RECORD entry after a Gen 5 Frontier battle (HGSS can't record BW2's
  engine): `Gen5Battle_SetFrontierUnrecorded` is set at the Tower's battle (`FrtCmd_TowerStartBattle`),
  `FrtCmd_CheckBattleRecordSlotValid` steps over the question and answers NO, `FrontierListMenu_AddItem` leaves out
  RECORD (`msg_0191_00263`), the message box close tolerates the unopened box; cleared in `FrtCmd_CreateBattleRecord`.
  New editor test commands `editor ability SLOT N` and `editor allmoves SLOT MOVE`; `gen5_tower_win.txt` wins with a
  Wonder Guard Spiritomb (a battle takes ~110000 frames: the room menu shows KEEP GOING, REST, RETIRE). The Tower's
  **doubles** too (`gen5_tower_doubles.txt`: four Pokemon, Double Battle at the attendant, RUN: a loss). Left: the
  Tower's multi battles.
  **The other facilities** (2026-11-18): the Factory, Hall, Castle and Arcade start their battles as the Tower does
  (`frontier_cmd_*.c`: `Gen5Battle_WantsFrontier` and `SetFrontierUnrecorded`). Tests, each a forfeit of the first battle
  (a loss, no video question): `gen5_factory.txt` (three rented Pokemon), `gen5_hall.txt` (one), `gen5_castle.txt` and
  `gen5_arcade.txt` (three). Goldenrod's party screen ignores its first A (a wake) and the dialogue's A presses are lost
  while scenes load, so the Factory and Castle count the yes/no prompts and lists with the `menu` event
  (`mash A until menu`). Seen once: the Arcade run crashed in `FrontierMap_Free` (a heap block of the palette buffers),
  and passed on the rerun; not investigated.

## Goldenrod, and the settings in Android's views (2026-11-16)

- **The app is Goldenrod** (`app.goldenrod`, dev flavor `app.goldenrod.dev`, "Goldenrod dev"), with a goldenrod-spray
  icon; it installs beside the old `org.pokeheartgold.port.dev`. On the Thor its files were copied over with adb
  (`cp -r` from the old `Android/data/.../files` after launching the new app once; `save.bin` compared identical).
- **Settings in Android's own views**, as in the Platinum fork (`NativeSettings.java`, `libntr_native_menus.patch`,
  see that fork's CLAUDE.md): every page including the save editor; *Advanced > Android menus* switches back.
- **Pages regrouped** like the Platinum fork's: Gameplay (the Japanese Game Corner), Battles (Gen 5 battles and the
  BW2 ROM, was "Gen 5 battles"), the frame rate on Display (was Gameplay), Link play after Controls, Save editor at
  the end with Advanced.
- Tried on the Thor: D-pad, A, B work; the first build left the system bars showing after the settings (the game
  lost its bottom strip) and a green focus outline round the game: fixed (immersive flags and WindowInsetsController,
  no default focus highlight on SDL's surface).

## Sacred Gold and Storm Silver (2026-11-16)

- **Adding them**: *Settings > Games > Add a game* (the new Games page: switch games, add one, choose at launch),
  from Drayano's 1.05 download. Its patches are **xdelta 1.1** (`%XDZ004%`), which libntr didn't read:
  `libntr_carts_known.patch` (both forks) decodes them (gzip'd data segment and libedsio-serialized control; the
  output's MD5 is the check). The HeartGold (USA) No-Intro dump (MD5 258cea3a..., CRC C180A0E9) isn't the dump the
  patch was made from (ae2a483d...), but they only differ where the patch doesn't copy from: the output checks out.
  Complete and Classic are both full rebuilds, so they're two games to choose from (no per-save versions).
- **What the hack changes**: data files only (Pokemon, learnsets, trainers, encounters, text, sound...), plus a new
  `msgdata/msg.narc`, and 243 bytes of arm9: its anti-piracy fix and the special marts' lists (`_0210FA3C`,
  `src/scrcmd_mart.c`), changed in place. No overlay changes. `lib/port/rom_hacks.c` reads those lists from the
  ROM's arm9 the first time a special mart opens (BLZ-decompressed; the pointer table is found by the lists'
  lengths, so HeartGold's and SoulSilver's layouts both work): 13 of 30 differ in Sacred Gold Complete.
- **Names**: Sacred Gold / Storm Silver by `msgdata/msg.narc` and the game code; Classic unless Venomoth is
  Bug/Psychic (personal data, `a/0/0/2` member 49).
- **Warnings**: the clean HeartGold and SoulSilver dumps (No-Intro's and Drayano's sources) and the four 1.05 patches
  are known by MD5 (`SIM_Prj_KnownRom`, `SIM_Prj_KnownPatch`); anything else is added with a warning.
- **Tests**: `rom_hack_sg.txt` with `ROM=~/thor-dev/sg_complete.nds DATA=~/thor-dev/desktop-sg` (the hack imported
  as the main ROM: name, marts, boots to the new game); on vanilla HeartGold it reads 0 changed lists. Storm Silver
  is untested (no SoulSilver ROM here). Not done: the Gen 5 battles' and Japanese Game Corner's imported files live
  in each game's folder, so an added game imports them again.

## Bugs from the owner (2026-11-18)

- **Dark caves were black** (Dark Cave, and every cave that needs Flash; reported with widescreen, but black at 4:3
  on the desktop too). HGSS's darkness isn't Platinum's circle of windows: Flash's weather fogs the whole 3D scene
  black (fog table all 127, `WeatherTask_Flash`), and a flat model (`field_flash_darkness.c`, 160x112 with a lit hole,
  a third of the way to the camera) is drawn translucent without fog, which on the DS clears the fog flag of the
  pixels under it (a translucent polygon ANDs it). libntr fogs each polygon in its fragment shader as it draws it, so
  nothing could un-fog. The port draws it without the fog (`FLASH_FOG`, `weather_manager.c`): the model moved toward
  the camera (1.25x the near plane, scaled down as much, so the same screen area with nothing of the map in front),
  and an opaque black frame around it in its plane, a tenth into the model's box at the sides and the top (its dark
  edges don't fill the box there) and far past the wide screen's edges. The scene is wide in these caves now
  (`WeatherLayers`, `lib/port/widescreen.c`). Test `widescreen_dark_cave.txt`. Not tried: after using Flash (the
  model at 4x), on the Thor. A general fix would be fog as a final pass in libntr with the DS's fog-flag rule
  (the edge-marking pass's attribute output could carry the flag).
- **Nuzlocke's "You've already used your catch for this area!"** ran off BW2's message window: HGSS's copy lost
  Eterna's line break (and its 0xFFFE conversion). Now on two lines as in Eterna (`PortNuzlockeToGen5`). Test
  `gen5_nuzlocke_refused.txt`.
- **The catching tutorial showed Bianca**: BW2's capture demo's effect script (0x23c) sets its own demonstrator (class
  38, Bianca) on the player's side with its SetTrainer command. `gen5/patches/src/ov168/btlv_effvm.c.patch` puts the
  battle's player there in the capture demo (Lyra, or Ethan when the player is Lyra). Test `gen5_tutorial_back.txt`.
  Shared `gen5/`: Eterna's tutorial plays BW2's demo too (`BATTLE_TYPE_CATCH_TUTORIAL`), so it gets the same
  change (Dawn or Lucas instead of Bianca); not yet run there.

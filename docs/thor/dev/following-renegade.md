# Following Renegade Platinum (notes)

The hack: Renegade Platinum v1.3.0 with the Following Platinum 2.0 integration (`FollowingRenegadePlatinum3541.xdelta`,
on a 3541 ROM; the ROM is `~/thor-dev/following_renegade_plat.nds`, 128 MB). Its followers are built into the game's
scripts and cut scenes, unlike the port's own follower (`src/port/follower.c`), which handles a few species and is poor.
Code: `src/port/following_hack.c`, `include/port/following_hack.h`.

## Decisions (with the owner, 2026-11-11)

- The port's own follower is only for vanilla Platinum and base Renegade Platinum; with the Following hack it is off.
- Gen 5 battles must support Fairy and all of Renegade's move, Pokémon stat and type changes. Gen 5 ability
  behaviour stays as BW2 has it (Sturdy and the like), with Gen 5 battles on.

## How the hack works (reverse engineered from the ROM)

The hack adds **ARM (Thumb) code that the port can't run**, so the port has to do what it does, in C.

- **The code blob:** `data/weather_sys.narc` member 65 (vanilla has members 0 to 64), 96 KB, loaded at **0x023C8000** at
  startup by a function that replaces the first entries of the script command table in arm9 (0x020EAC58) and that is
  called as an init function; the init (0x023CDBB8) then checks the CRC of some members. About 196 functions
  (29 KB of code, from 0x023C8000 to 0x023CF300), then the hack's tables.
- **Hooks:** about 120 `BL` sites in arm9 and overlays 5, 6, 8, 9, 11, 16, 21, 73 point at the blob: usually a
  vanilla function replaced by a copy with the follower's special cases. Examples: `MapObjectMan_PauseAllMovement`
  (0x02062C48) skips the follower (local ID 0xFD); `0x02060B64` (set the player's movement action, 20 call sites) is
  wrapped; the tile checks (0x02063964 etc.) learn the follower's states; ov5 0x021D1930 (the map objects' update)
  calls the follower's own update (spawn when needed, 0x023C88E0).
- **Relocated tables:** the hack moved these into the blob, with new entries: the script command table (arm9
  0x020EAC58 -> 0x023D2194: commands 840 to 860 are new; 841 to 843 are empty), the movement type table (0x020EE3A8 ->
  0x023D1C10: types 0x44 and 0x45 are new, for the follower), object graphics: renderers (0x021FB97C -> 0x023D2F08),
  textures (0x021FC9B4 -> 0x023CFB40), frame sequences (0x021FB5BC -> 0x023D1B48), animations (0x021FD77C -> 0x023D62A0),
  render details (0x021FC194 -> 0x023D48D0) and a table of effects (0x021FF85C -> 0x023D1D28); also two movement tables
  in arm9 (0x020EE31C, 0x020EE4B8) and one in overlay 16.
- **Graphics:** 566 new object graphics, IDs 276 to 841 (0x114 to 0x349): one billboard per Pokémon (forms, females
  and shinies included), whose pictures are in `data/mmodel/mmodel.narc` (1.4 MB -> 5.9 MB) and which share one
  renderer (the blob's, built on the vanilla billboard renderer `ov5_021ECA70` with two functions of its own,
  0x023CA2BC and 0x023CA384: they choose the picture each frame). **The crash on choosing the starter** was
  `ScrCmd_AddObject` adding an object with graphics ID 0x2BE (the follower's placeholder in the map's object events)
  with no renderer.
  The species to graphics table is at 0x023CE560 (493 entries of 6 bytes: species, base graphics ID (10 bits), flags for
  forms and females); `hk_23C830C` computes the ID (base, + 1 for a female when the species has one, + form).
  IDs 0x114 to 0x349 are "Pokémon" (`hk_23C8218`).
- **The follower:** a map object with local ID 0xFD, movement type 0x44 (0x45 on a bike or surfing...), kept in the
  field system's extra fields (+0xCC the object, +0xE0 on/off, +0xE4 spawn request, +0xE8 the ball animation).
  Spawned from the lead Pokémon (0x23C8000), hidden or shown by script commands and cut scenes. **Coming out of the
  ball** is a field effect with 3D models from the field effect NARC (members 0xC9, 0xCA, 0xCB, which the hack
  adds to `fldeff.narc`), sound effect 0x5E8, and the Pokémon's cry when it ends (0x23C9248 and the functions
  around it).
- **New script commands** (the scenes' scripts in `scr_seq.narc` use them): 840 sets a byte; 844 puts the follower
  out with its ball animation; 845 to 860 hide, show, move, face, look at the player and read the follower's
  state (some set a variable to the follower's graphics ID, or whether the party has a Pokémon that can follow).
- **ROM patches:** on this ROM `RomPatches_Apply` finds none of Renegade's markers (it reports shiny 1/8192 and no
  Fairy): the checks need updating for it.

## Tooling

- `DATA=~/thor-dev/desktop-following tools/docker/desktop.sh test SCRIPT` runs a test on this ROM's extracted files
  (`desktop.sh` honours `DATA`; scripts go in `~/thor-dev/`, which the container mounts); fixture
  `fr-before-starter` is the save before the starter. `~/thor-dev/desktop-renegade` is the base Renegade ROM
  extracted (compare the two to see the hack's changes only).
- To read the hack's code: extract member 65 of `weather_sys.narc` and disassemble it as Thumb at 0x023C8000
  (capstone does; call targets in arm9 and the overlays are the vanilla game's functions at the same addresses).
  The decomp's functions have been renamed since they were addresses (`sub_0206298C`): `git log -S sub_0206298C`
  finds the commit that renamed one, and its hunk the new name.

## Where it stands (2026-11-11)

Following Renegade Platinum starts, plays its intro, picks the starter and wins the first battle on the desktop build
(`new_game.txt`, `first_battle.txt` and `turtwig.txt` pass on it with `DATA=~/thor-dev/desktop-following`), and the
Pokémon follows the player in the hack's own graphics. Done in `src/port/following_hack.c` and the hooks:

- the graphics tables (276 to 841), read from the blob;
- the script commands 840, 844 to 860 (the table is a pointer in the port now, `scrcmd.c`), and the local ID 0xFD
  for the follower in scripts (`GetLocalMapObjByIndex`);
- the follower itself: spawn (map load, `field_map_change.c`; after the map's objects, `fieldmap.c`), its data
  flags, where it may follow (maps, big Pokémon in buildings), `PokemonGraphicsID` (species, form, gender);
- the movement actions 154 to 171 (the follower's hops; the emotes, 158 to 171, end at once: `unk_020655F4.c`),
  without which the post-battle scene crashed;
- script IDs from 10500 (the follower's talk script, `scr_seq.narc` member 1124 and text bank 724);
- the port's own follower is off in this game (`follower.c`), and its setting says so.

Done since (2026-11-11, second pass):

- **The follower's movement** (`following_hack.c`, `FollowingHack_MovementType`, from `map_object.c`): movement 0x44 is
  the hack's own (hk_23CA46C): the follower moves only when the player starts a step (`FollowingHack_PlayerStep`,
  called from `PlayerAvatar_SetMovement` in `player_move.c`, as the hack's copies of the player's step functions call
  hk_23C884C), one frame later, onto the player's last tile at the player's speed (running is a fast walk); a ledge
  jump is a slow walk to the ledge, then a hop over it at the next step (actions 154 to 157); a hidden follower comes
  back at the next step, out of its ball when its data says so (bit 0x40), turned away from doors and warps. Movement
  0x45 (bike, water) keeps it out of sight at the origin. The follower's data bits (index 2): 0x2 hidden, 0x8 hop
  next (direction in bits 4-5), 0x40 out of its ball next, 0x80 shiny, 0x100 hopping.
- **The ball effect** was drawn all along: the closed ball shows for two frames only, so `shot` missed it;
  `stillshot` (one frame at a time) shows the ball, the flash, then the Pokémon. **The recall** (type 1) is the hack's
  now (hk_23C9490): the Pokémon shrinks and its palette (the last 64 bytes of its `mmodel.narc` member, sent to its
  billboard's palette slot) fades to white over 6 frames, then the closed ball shows on its tile and fades out (model
  alpha) over 6 more.
- **Hooks:** `MapObjectMan_PauseAllMovement` leaves the follower moving (hk_23CBF3C); `ov5_021EB1A0` (a map object's
  billboard, for the effects) gives the billboard renderer's for the hack's Pokémon (hk_23CBD0C); `sub_02063F00` (an
  object at a tile) is the hack's (hk_23CBA98): the player walks through the follower, the others don't, and a big
  Pokémon also takes the tile behind it.
- **The picture each frame** (hk_23CA2BC, `FollowerRenderer_Update`): the animation is the facing direction (the
  hack's four), advanced once a frame, with a bob (2 units for 5 frames of 10), the species' offsets toward where it
  faces, and the second palette for a shiny follower. Platinum's walker update (used before) advanced twice a frame
  and asked for walking animations the Pokémon don't have: on the Thor Piplup faced up and stepped fast.
- **Bikes and water** (2026-11-12, from the Thor: the follower walked on the water a tile or two behind): the hack
  switches the follower to 0x45 from its hooks in the bike's and Surf's code; the port does it from the player's state
  in the follower's update (`FollowerStartRide`): the recall effect, then 0x45 until the player is on foot again, then
  0x44 with the hidden and ball bits, so it comes out of its ball at the first step. `sub_02063478` (may an object move)
  lets the follower move out of sight, as the hack's hk_23CCF64 does for 0x44 (the port also for 0x45). Tested on
  Twinleaf's pond (`script 10004` facing it).
- **Map borders** (from the Thor: the follower vanished entering Route 201): walking into a connected map, Platinum
  deletes the last map's objects that aren't persistent (`sub_0206184C`), the follower among them. The follower is
  skipped there and moved to the new map (`FollowingHack_MapConnected`, from `fieldmap.c`), deleted where it may not
  follow, and spawned again (out of its ball at the next step) where it may. Tested Twinleaf to Route 201 and back.
- Tested: `following_ball.txt`, `first_battle.txt`, `turtwig.txt` and `gen5_battle.txt` with this ROM; walking with
  turns back over the follower (the follower stays behind, the player passes through it).

Not done (simplified or missing):

- **Bikes** are not tested on screen. Ledge hops and Surf (from the A button, with *Field moves without badges*) work
  on the Thor (2026-11-12).
- After a hop the hack shakes the screen for some heavy Pokémon (hk_23C8BD0): not ported. When it comes back out of
  its ball the hack also plays the tile's grass effect (hk_23CB998): not ported.
- The emotes (renderer 0x23, hk_23C9720, 14 effects), and hk_23CA384 (the renderer's resume function: it also changes
  the follower's picture when the lead Pokémon changed).
- **Hooks still not ported** (`hooks.py` in the analysis lists them; grouped by the function they replace): the tile
  effects of map objects (`sub_0206397C`, `sub_02063964`: grass; `sub_02063D30`, `sub_02063DA8`: reflections), `Player_SetYPos`, `ov5_021ECCC8`, the shadows and effects in `ov5_021F17E4` to
  `ov5_021F1A8C` and `ov5_021F4AB4`, the map change (`FieldSystem_MapChange_LoadObjects`, `sub_0205281C`), the
  overlay at 0x02247000 (HM cut-ins, surfing), `ov5_021D4858`/`ov5_021D4A24`, `ov5_021E1134`, `Player_SetDir`.
  Expect odd behaviour in cut scenes, on bikes and on water.

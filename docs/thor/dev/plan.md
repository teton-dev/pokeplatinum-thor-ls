# The plan: where everything stands (2026-11-17)

The roadmap across the fork's projects, with the owner's decisions. Each project has its own detailed plan:
Gen 5 battles `gen5-battles.md`, HGSS `hgss.md`, Following Renegade `following-renegade.md`, Nuzlocke `nuzlocke.md`,
enhanced graphics `enhanced-graphics.md`. The order is: Gen 5 battles (Platinum), HGSS, Renegade, enhanced graphics.

## Where it stands

- **Gen 5 battles in Platinum (Eterna): ~90%.** Wild, catching, trainers (Platinum's AI levels), doubles and partners,
  legendaries, roamers, Giratina, the Safari Zone, the catching tutorial, weather, the screens after a catch,
  obedience, widescreen. Left: Pal Park, the Battle Frontier, widescreen for the post-battle screens (Pokedex entry,
  naming), tests that fail on a lost battle, and trying all of it on the Thor.
- **HGSS (Goldenrod): ~75%.** The same Gen 5 battles plus the Bug-Catching Contest, Apricorn Balls, Safari Zone, Mom's
  line; save editor, debug window, Nuzlocke rules, the Japanese Game Corner, Sacred Gold / Storm Silver (2026-11-16).
  Left: a whole-game playthrough, the Frontier and Pal Park, a whole contest from the gate, Storm Silver, the Saves
  and Nuzlocke pages, the Gear.
- **Renegade Platinum: ~70%.** Added, named (Following Renegade too), played; Fairy in Gen 5; the followers in C.
  Left: the remaining Following pieces (`following-renegade.md`) and checking Renegade's Pokemon, move and item data in
  the Gen 5 engine.
- **Enhanced graphics: not started** (last, by design).
- **Done 2026-11-16, not yet tried on the Thor** (except Goldenrod's settings): Eterna/Goldenrod names, IDs and icons;
  native Android settings with regrouped pages; the Gear's Nuzlocke tracker; unknown-ROM/patch warnings. Open there:
  the left stick and button remapping in the native settings, the dark band at the bottom with the settings open
  (check with a real R3), MD5s of Renegade's Classic and extra patches (warned about until added).

## Next, in order

1. **A Thor session**: install both apps (adb address from the owner), play the new pieces, fix what turns up.
   2026-11-17: Eterna 9100035 (Actions run 35, with the wide Pokedex entry) and Goldenrod 26111609 installed over USB; SoulSilver and Sacred Gold
   (`sg_complete.nds`) copied to the SD card's `ROMs/nds`. Playing them is the owner's.
2. **Widescreen for the post-battle screens**: Pokedex entry and naming screen done on the desktop (2026-11-17,
   `gen5-battles.md`); the evolution scene after a battle and HGSS's contest swap screen too (2026-11-17). Left: the Thor, and HGSS's
   own post-battle screens and apps (its widescreen covers only the Pokégear, the swap screen and the field so far).
3. **Goldenrod's missing pages and the Gear** (below). Challenges (Nuzlocke mode) and Saves (port-common's slots,
   sync, autosave) done 2026-11-17 on the desktop; the Gear's party, trainer, species, sprites, area
   encounters and bosses too; left: its battle, map, PC, Day Care and berry data for HGSS (`hgss.md`).
4. **The Battle Frontier** in the Gen 5 engine (below), then Pal Park. 2026-11-17: the Battle Tower's single battles run
   in BW2's engine (its facility battle; `gen5-battles.md`); Palmer in the light-pools arena too; Goldenrod's Tower singles too (its Frontier
   crashed on 64-bit: `u32` script jump offsets, fixed). 2026-11-18: the Battle Video is settled (skipped after a
   Gen 5 battle, no RECORD entry) and the Tower's doubles run in BW2's engine, in both games; the Factory, Hall, Castle and Arcade too (singles); next: the
   Tower's multi battles and the other facilities' doubles and multis, an APK of each for the Thor, then Pal Park.
5. **Renegade checks** (below).

## Later

- **BW2 Summary shadows** (2026-11-23): the Pokémon's shadow in the Gen 5 Summary is off for now (the owner's choice: no
  shadow looks better than a broken one). Without libntr's polygon ID rule (`SIM_SetTranslucentPolygonIdRule`) the
  shadow's many overlapping cell quads (`MCSS_DrawQuad`, polygon ID 1, a low alpha) stack into an opaque black blob beside
  the Pokémon; with the rule on (it is on all the time now, `src/main.c`, for the overworld's overlapping shadows) nothing is drawn (checked by pixels on the desktop,
  `gen5_summary.txt`; the battle's shadows are fine with the same rule). Not found why: log `shadowAlpha`, the polygon IDs
  and the stencil in `G3SIM_DrawItems` for the Summary. The goal is BW2's faint shadow under the Pokémon.
- **Eterna/Twinleaf signs** (2026-11-23): on the Duo the sign's lower plank covered the cap's top outline; the sprite depth
  nudge went from 8 to 16 (`billboard.c`, Eterna 9100047): check it on the Duo.

## The owner's decisions (2026-11-17)

Platinum:
- **Pal Park** should be reachable: the port can load a GBA game's save (*Settings > GBA slot*), which Pal Park
  migrates from. Make it work in the Gen 5 engine (Park Balls always catch, no bait or rock, like the Safari Zone's
  view-only battle in `gen5/src/safari.c`).
- **The Battle Frontier must run in the Gen 5 engine**, with its own original music and sprites working. The
  **Frontier Brains** get the 3-lights arena (BW2's World Tournament arena's light-pools variant, 19 alt 1: confirmed) and their own music.
  (BW2's `BTL_TYPE_FACILITY`, its Subway: level scaling, no items or EXP. Large: start with the Battle Tower singles.)
- **The order of the post-catch screens stays as it is** (the nickname question in BW2's battle first, then the
  Pokedex entry): swapped for Gen 5 is fine.

HGSS:
- **No Pokewalker support** for now.
- **Frontier and Pal Park**: port them from Platinum once Platinum's work is done; it should carry over well.
- **SoulSilver is now in `~/thor-dev`** (`Pokemon - SoulSilver Version (USA).nds`): test SoulSilver, and Storm Silver
  from Drayano's patches.
- **Import both pages**, Saves and Nuzlocke mode, from Eterna (Eterna's `src/port/gui_settings.cpp` DrawSaves and
  DrawNuzlockeMode; HGSS has the cores: save slots via libntr, `lib/port/nuzlocke_mode.c`).
- **The Gear companion can likely be ported with minimal work** (Eterna's is port-common's `gear_ui.cpp` with
  `src/port/gear_*.c` data adapters).

Renegade:
- **Complete the checks**: the remaining Following Renegade pieces, and Renegade's own Pokemon, move and item data
  (and Fairy) in the Gen 5 engine.

# HGSS: the Japanese Game Corner (slot machines) as an option

Goal: *Settings > Gameplay* toggle that swaps the English Game Corner's Voltorb Flip for the Japanese version's
slot machines, with the text machine-translated to English, in widescreen. Needs the player's own Japanese HGSS ROM
if the English one doesn't carry the files.

## Findings (2026-11-14)

- Japan kept the Game Corner's slot machines; Western and Korean HGSS replaced them with Voltorb Flip
  (Bulbapedia, Serebii). Both Game Corners (Goldenrod, Celadon) are affected.
- **The English ROM has nothing to reuse.** In the HGSS decomp (`~/pokeheartgold-thor-ls`) the only Game Corner
  app is `src/voltorb_flip/` (overlay `voltorb_flip`, assets `files/application/voltorb_flip/`); there is no slot
  code, graphics or message data. `Coins_*` (`include/coins.h`) and the Coin Case item exist, and the scripts
  that give coins/prizes are shared. So the JP ROM is required, not optional.
- **The JP slot machine is ARM overlay code that nobody has decompiled** (pokeheartgold covers USA only), and the
  port cannot run ARM code. It has to be rewritten in C, like `src/port/following_hack.c` was for Renegade.
- The Japanese ROM is `~/thor-dev/HEARTGOLD_JP.nds` (game code IPKJ, HeartGold Japan).

## What the JP ROM contains (2026-11-14, from unpacking with `ndspy` and diffing against USA)

- 128 overlays in JP, 129 in USA; the table is shifted near the end (USA's `voltorb_flip` is one of the extra ones).
  Most overlays differ by small amounts (translation/fixes), so addresses can't be assumed equal.
- **Overlay 77 is the slot machine's code.** JP: 22272 bytes (RAM 0x021E5A40-ish, load address 35540544); USA: a
  32-byte stub (the decomp's `OVY_77`, empty, "After main"). Neighbours 74-76 and 122-124 also differ in size
  (74: 75552 vs 90144, 75: 21760 vs 12928), likely Wii message/Battle Frontier shuffling, not yet checked.
- **`a/1/5/5` is the slot machine's graphics NARC.** JP: 88072 bytes, 47 files (NCGR/NCLR/NSCR/NANR/NCER: one
  24640-byte background NCGR, a 2084-byte NSCR, and ten 4656-byte sprite sheets each with palette, animation and cell
  bank). USA: 64 bytes (emptied). So USA kept the slot of the file and removed the data.
- The text is in the JP message NARC `a/0/2/7` (814 banks vs USA's 829: bank numbers are shifted, so the Game Corner
  banks have to be found by content, not index). Scripts (`a/0/1/2`) and the Goldenrod/Celadon event data also differ.
- `data/namein.narc` exists only in JP; `data/str2uni.bin` and `a/2/6/4` (Voltorb Flip) only in USA.

## Overlay 77, first look (2026-11-14)

Tools (no ROM data in them): `tools/jp-gamecorner/` (`compare_roms.py`, `extract_ov77.py`, `disasm_ov77.py`; run with
a venv holding `ndspy` and `capstone`, paths point at `~/thor-dev`). The disassembly itself stays outside the repo.

- Load address `0x021E4E40`, 22272 bytes (to `0x021EA540`), all Thumb, 180 function entries.
- Entry points are the usual OverlayManager trio: `0x021E4E40` is Init (a two-state switch: creates heap `0x4d` of
  0x20000 bytes, allocates a 0x304-byte work struct, zero-fills it, calls setup `0x021E4F34`), `0x021E4EA8` is
  Main (a 4-state switch through a jump table: 0, 1 -> `0x021E5018`/`0x021E50A0`, 2 -> `0x021E5C48`), then Exit.
- Calls into the arm9 go to ~200 distinct addresses (`0x0200E040/E0B0` ×18 each, `0x020F21B8` ×26, `0x0201A7C0` ×23,
  `0x020D420C` is MI_CpuFill-like); there is **no symbol map for the JP arm9 here**, so each has to be identified.
  Plan: match JP arm9 functions against USA's by their bytes (ignoring `bl` operands) and name the USA ones from the
  decomp (`Voltorb Flip`'s overlay calls the same BG/OAM/message/heap helpers: the sequence of calls tells which).
- **The game rules are plain data at `0x021EA000-0x021EA52C`:** a table of six function pointers (the per-state
  handlers), the bet/payout ladders (`1,3,6,10`, `1,10,100,1000,10000,100000,1000000...`), per-symbol probability tables
  (`0x1e 0x2d 0x0f ...` sets summing to 100), the "pattern" permutations of reel symbols 0-5 (six symbols), the
  reel/lamp animation coordinate tables (`0x12c,0x1db ...`) and the sprite-slot index tables (`0x06 0x19 0x1c 0x1f ...`).
  These can be lifted into C tables once their meaning is pinned down.

## Matching the overlay's arm9 calls to USA (2026-11-14)

`tools/jp-gamecorner/match_arm9_calls.py` decompresses both arm9s (`ndspy.codeCompression`) and finds each called
JP function's first 8-24 bytes in USA's arm9. Result: **109 of the 140 distinct arm9 callees match uniquely**
(31 are too short/ambiguous, e.g. leaf stubs), with a shift of ~0x2E0-0x7B0 between the JP and USA addresses (e.g.
JP `0x0200E040` = USA `0x0200E320`, JP `0x020D420C` = USA `0x020D4994`). The static arm9 code is the same compiler
output, so semantics carry over.

What it does not give is **names**: there is no symbol map (a matching build needs the proprietary mwccarm, which
this machine does not have; `docs/ds-build.md`), and the decomp source carries no addresses. Names so far come
from call context, using the decomp's own APIs:
- `0x2007198` OverlayManager_CreateAndGetData(man, size, heapId); `0x20071A8` GetData; `0x20071BC` GetArgs;
  `0x201A5C4` CreateHeapAtEnd(parent, id, size).
- `0x20D420C` MI_CpuFill8-style fill (`(dst, value, size)`); `0x20F21B8` signed 32-bit divide/modulo helper;
  `0x20F1998` a fixed-point divide (arg `r0<<12`).
- `0x2005F64` Sound effect play (arg `0x5DC`, the confirm SE), `0x200DAA8`/`0x201A7C0`/`0x201A740`/`0x201EC70`
  sprite/OAM-list calls (OAM context + index), all unconfirmed.

To finish naming: align the decomp's compiled USA overlay for Voltorb Flip (same BG/OAM/message/heap helpers) against
its C source call by call, which gives name <-> USA address for the common helpers, then carry over by the JP->USA
table. Alternatively port the logic in terms of the decomp's existing API directly (the port already has the C
versions of all of these), reading each JP function for what it does rather than what it is called.

## Scale

The code is ~22 KB of ARM/Thumb overlay: thousands of instructions to read and re-express in C, plus the
script commands that start it. It is a multi-session job: expect the reverse-engineering (plan step 2) to be the
bulk of it.


## Plan

1. ~~Get the JP ROM, unpack and diff it~~ done (above); the Game Corner scripts/maps and text banks are still to find.
2. **Reverse the slot overlay** (disassemble with the symbols we can map from USA; the Nitro SDK and shared
   systems are the same code). Pin down: reel strips, symbol payouts, the coin bet flow, the RNG/"hot" logic,
   the bonus (Pokémon-themed) mode, its BG/OAM setup and input handling.
3. **Rewrite it in C** as a port-only app beside `voltorb_flip` (`lib/port/slots/`), same OverlayManager-style
   Init/Main/Exit so scripts can launch it, using the shared coin case.
4. **Assets**: import from the JP ROM at runtime like the Gen 5 BW2 import (*Settings > Games*): a JP ROM import
   step, no JP data in the repo.
5. **Scripts and maps**: with the option on, swap the Game Corner's Voltorb Flip clerk/machine events for slot
   machine events (the JP scripts and event data, converted), and the prize counter text where it differs.
6. **Text**: machine-translate the JP messages to English into a port-side message bank (kept separate from the
   USA banks; mark as machine translation), shown when the option is on.
7. **Widescreen**: the slot machine's top/bottom screens through the same SIM widescreen mechanism as the
   Pokégear/Voltorb Flip (`lib/port/widescreen.c`): the backdrop on `wideBgs`, not `edgeBgs`.
8. **Settings toggle** and tests (a `slots` script test that plays a spin).

Open decisions: which JP version to support first (HG or SS; the Game Corner is identical in both),
and whether to include the bonus mode in the first pass.

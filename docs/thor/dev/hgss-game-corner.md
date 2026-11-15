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

## Reading the logic (2026-11-14)

Read so far: the state-handler table (`0x021EA000`, 7 handlers `0x021E66F8 ... 0x021E69FC`, each a `switch (work[8])`
substate machine that returns 1 when the whole screen state is done) and the pure-logic functions `0x021E6A58-0x021E6EE0`.
The work struct (0x304 bytes, `r0`/`r4`/`r5` everywhere) so far:

| offset | meaning (inferred) |
|---|---|
| +0x08 | substate of the current handler; +0x10 next screen state |
| +0x1c / +0x34 | a handle / a countdown timer (handlers wait for it to reach 0) |
| +0x38, +0x39 | machine type (0-?), bet/"line" index (1-3) |
| +0x3b | the drawn reel result symbol 0-6; +0x3c outcome class 0-2 |
| +0x3e / +0x3f | spin counter / spin limit (8, or 15 when `+0xA0 == 6`) |
| +0x40, +0x42, +0x4c | round counters; +0x4c is a streak that saturates at 10 |
| +0x46, +0x48 (u16) | coin totals, clamped to 0x383 (899) |
| +0x4d (s8), +0x4e/0x4f | reel stop offsets; per-reel speed/stop tiers (8, 12, 24 and 0, 1, 2) |
| +0x50..52, +0x53..55 | per-reel flags (3 reels): "forced hit" and "forced miss" |
| +0x6c | flag word: bit0 first-round, bit4 reach, bit6, bit7 bonus |
| +0xA0 | the machine's Pokemon/symbol id (6 is the special machine) |

Logic confirmed from the code:
- **Symbol draw** (`0x021E6B3C`): rolls 0-999 (helper `0x021E7114`, from `0x0201F9F0`, the LCRNG) and walks the cumulative
  u16 thresholds of `0x021EA284 + 0x2A*type + 0x0E*bet` (7 entries) to pick the result symbol; a second roll against
  `0x021EA204 + 0x12*symbol + 6*bet` (3 thresholds) gives the outcome class (symbols 4 and up use a single threshold).
- **Reel assist** (`0x021E6CD8`): picks the table row `0x021EA1A8 + 15*min(streak,5) + 5*tier` (tier from the spin
  counter: 0-3/4-5/6+, or 0-5/6-10/11+ in the 15-spin mode) and rolls 0-99 for: hit chance per reel (`row[4]`), the
  stop-speed tier (`row[2]`, `row[3]`), and whether to arrange a "reach" (`row[0]`, `row[1]`) on a random reel.
- **Bonus roll** (`0x021E6E48`): rolls 0-99 against cumulative per-type thresholds `0x021EA154 + 3*type` (values like
  0x0F 0x1E 0x1E, or 0x64 0 0) to pick a 0-3 multiplier; adds `multiplier * 0x021EA11C[2*type + i]` into the two coin
  totals (clamped to 899).
- RNG helpers: `0x021E7128(n)` is a random number below `n`; `0x021E7114` is the 0-999 roll; `0x021E7188(symbol)` tests if a
  symbol is a "special" one; `0x021E7198(work, symbol)` gives its animation/payout class.

All of this is table-driven, so the C port is the tables (extracted at import time from the player's JP overlay, not
copied into the repo) plus a few hundred lines of state machine. What is still unread: the other ~150 functions (screen
setup/teardown, the reel animation and sprites, the payout/coin counting, text boxes, touch and button input, the
sound calls). The coin/payout rules (`0x021E8B9C`, `0x021E8C24` and the `0x021EA20x` tables) are the next to read.

## Coins and the hidden payout meter (2026-11-14)

Read: `0x021E6A10`, `0x021E6A58`, `0x021E6EE0`, `0x021E8B9C`, `0x021E8C64`. Corrections/additions to the struct above:
`+0x28` is the **coin count** (capped at 50000 = `MAX_COINS`; also the Coin Case's cap, so it is the shared save value),
`+0x30` the coins bet this spin (1 or 3), `+0x26` the reason a bet was refused (1 = full case, 2 = fewer than 3 coins,
3 = no coins), `+0x4D` (signed byte, -99..99) a **hidden payout balance**, `+0x6C` bit 2 = free play (e.g. in a bonus
round, bets are not charged).

- **CanBet** (`0x021E6A10`, `CanBet(work, oneCoin)`): coins >= 50000 -> refuse (reason 1); free play -> ok; one-coin
  needs >= 1 coin (reason 3); otherwise needs >= 3 (reason 2).
- **Round start** (`0x021E6A58`): sets `+0x4D = -(table_118[streak % 3 (or the 2nd entry if streak>5)] + +0x40)`, i.e. the
  balance starts negative (the machine must "pay back" before it favours the player), bumps the streak (max 10), clears
  per-reel flags, bonus flag and the 3-reel results. In the 15-spin special (`+0xA0 == 6`) the limit is 15, else 8.
- **Payout decision** (`0x021E6EE0`, run after each spin): `+0x3A` is the outcome (3 = none, 2 = hit pending, 0-2 = payout
  tier). If `+0x4D <= 0` the balance only creeps up by `table_114[bet]` and the result is "none". Otherwise roll 0-99
  against `table_130[(type_flags)*3...]` to pick a tier, add `table_108[bet] * tier` to the balance (cap 99), and if the
  balance is still positive roll again against `(balance * table_16c[...]) / 15`: on success pick the final payout tier
  (second 0-99 roll against `table_184`), set the balance to `-(table_124[bet][tier] * balance) / 10`, reset the
  streak and clear flag bit 6. This is a classic "hot/cold" slot: the player's expected return is shaped by the
  balance rather than by pure reel odds.
- **Reel spin parameters** (`0x021E8C64`): each of 3 reels gets a start position `rand % 21` (or a fixed value from
  `table_110` while the first-round flag is set) and a distance `0x1F8 - 0x18 * pos`.
- **Win animation** (`0x021E8B9C`): `+0x4D` is turned into a 1-5 step lamp display (`(|balance|-1)/3+1`, capped at 5)
  using the lamp table `0x021EA020` and `0x021EA054`, with SE `0x939`.

**Payout amounts and adding coins (read 2026-11-14):**
- `GetPayout(work, symbol)` (`0x021E7198`), symbols 0-6: normal play pays `table_098[symbol] = {0, 0, 15, 10, 2, 100, 100}`
  coins per bet unit (symbols 5 and 6 are the two jackpot symbols, tested by `0x021E7188`: `symbol - 5 <= 1`), and for
  those two the running jackpot counter `+0x46`/`+0x48` (u16, grows in the bonus roll, max 899) is added on top. In
  bonus play (`+0x6C` bit 0) it is `table_084[symbol] = {20, 20, 20, 0, 1, 0, 1}` plus `(+0x40 / 2) * ((+0x4C >> 2) + 1)`.
- `AddCoins(work, amount, ...)` (`0x021E7B04`): `+0x28 += amount` at once, clamped to 50000 (the extra is dropped
  from the amount shown), then starts a counting task (`0x021E7B74`, each frame moves one coin from "amount pending"
  to the displayed total with SE `0x62C`; in bonus play it also adds to the bonus total `+0x4A`, max 999).
- RNG: `0x021E7114` = `LCRNG() % 1000`; `0x021E7128(n)` = `LCRNG() % n` (for `n == 2` it takes `(LCRNG() % 1000) % 500`,
  an odd shape that makes the draw 0..499).

**Reels (read 2026-11-14):** each reel is a loop of 21 symbols, 24 px per symbol (a full turn is `21*24 = 504 = 0x1F8`
pixels). `SymbolAt(reel, offset)` (`0x021E9290`): `idx = (pos[reel] + offset) mod 21` (`pos` = s16 at `+0x66`), then the strip
value `strip[reel][idx]`. Normal strips (`0x021EA32C`, 21 u32 each) use symbols 0-5:
- reel 0: `2 5 4 1 0 2 3 4 0 2 3 5 0 2 1 4 2 1 0 5 0`
- reel 1: `4 5 2 0 1 3 0 3 2 4 0 2 4 1 2 0 5 3 1 0 2`
- reel 2: `3 5 2 4 3 0 1 2 5 2 0 1 4 3 0 1 2 0 1 2 0`
Bonus strips (`0x021EA428`, same shape) hold sprite/tile ids instead (3 reels of "7 10 13 / 16 19 22 / 25 28 31 ..." with 6 as
the blank filler), i.e. the bonus round shows big picture tiles rather than the six symbols. The lamp/coin-digit scale
`0x021EA034` is `10^0 .. 10^7` (digit extraction for the coin counter, `0x021E8C24`) and `0x021EA020` is `{0,1,3,6,10}`
(cumulative lamp counts). Symbol 6 is not on any strip: it is a result of the draw only.

**How the game launches it (read 2026-11-14):**
- JP arm9 `0x0203F798` = `SlotMachine_LaunchApp(FieldSystem *, u32 type)`, the twin of the decomp's `VoltorbFlip_LaunchApp`: it
  allocates a 0x1C-byte args struct (heap 0xB), zero-fills it, sets `args+0x14 = fieldSystem->saveData` (`+0xC`),
  `args+0x18 = &fieldSystem->menuInputState` (`+0x10C`), `args+0x08 = <coins pointer from the save>`, `args+0x04 = type`,
  `args+0x0C = 0`, and calls `FieldSystem_LaunchApplication(fs, &template, args)` with the template at JP `0x020F99E4`
  `{Init 0x021E4E41, Main 0x021E4EA9, Exit 0x021E4F05, overlay id 0x4D (77)}`.
- The script command is in overlay 1 at `0x022017F4`: it reads **two u8 script arguments** (machine index, mode), calls
  `0x0220173C(saveData, index, mode)` for the machine's *type*, then launches the app and a wait-for-app task.
- **Machine types** (`0x0220173C`): there are 21 machines (mode 0) or 14 (mode != 0; index >= count returns type 3).
  Which machine has which type is derived from a per-save random seed (the LCRNG is re-seeded from the save, 7 machines
  become type 1 and 3 type 2 in the 21-machine corner, 5 and 2 in the 14-machine one, the rest type 0, then the
  generator state is restored). That is the "lucky machine" system: the type selects one of four odds sets
  (`0x021EA284 + 0x2A*type`).
- App setup (`0x021E4F34`): `work+0x1C = coins ptr`, `work+0x28 = coins` (read from the save), `work+0x38 = args->type`,
  and it bumps a game stat (id 5, the slot plays counter). On exit (`0x021E4FA0`) it clamps the coins to 50000 and writes
  them back. The slot plays the BGM `0x45`.

**Scripts: the USA ROM already has them (2026-11-14).** The decomp keeps the machines' scripts and the `CasinoGame <machine>, <city>`
command (`ScrCmd_CasinoGame`, `src/field/scrcmd_pokemon_misc.c`) *with the same arguments*, `SlotLuckiness()` included
("Return value is unused outside of Japanese copies"): USA just launches Voltorb Flip where Japan launches the slot app. The
scripts are: Goldenrod `scr_seq_0906_T25R1101` (22 `CasinoGame n, 0`: machines 0-20, plus 21 -> type 3), Celadon
`scr_seq_0803_T07R0401` (14 machines, city 1), and `scr_seq_0910_T25SP0101` / `scr_seq_0806_T07SP0101` (one machine each),
which already check for the Coin Case and for 3 coins (`msg_0599_T25R1101`) before `CasinoGame`. So **the switch is one
hook in `ScrCmd_CasinoGame`**: with the option on and the JP data imported, launch the port's slot app with the luck value
instead of `VoltorbFlip_LaunchApp`. No script or map changes are needed for the machines themselves; the Game Corner's
other scripts (clerk, prize counter, coin exchange, `ScriptOverlayCmd 3` coin display) are shared. (What the US maps show
as the machine objects, and any Voltorb Flip text on the counter, has not been checked.)

**Text: one small bank (2026-11-14).** The slot app opens one message bank with `MessageData_New(0, NARC 0x1B msgdata,
bank 0x1AD = 429, heap)` (`0x021E5838`). It is **JP `a/0/2/7` file 429**, 16 strings (decoder: `tools/jp-gamecorner/decode_msg.py`
plus the decomp's `charmap.txt`). Machine translation (English text is ours, not Nintendo's):

| id | Japanese | English |
|---|---|---|
| 0 | ゲームをしゅうりょうしますが よろしいですか？ | Quit the game? Is that OK? |
| 1 | コインが50000まいになりました これいじょうコインはふえません | You have reached 50,000 coins. You can't get any more coins. |
| 2 | コインが３まいありません ゲームをしゅうりょうします | You don't have 3 coins. The game will end. |
| 3 | コインがありません ゲームをしゅうりょうします | You have no coins. The game will end. |
| 4, 5, 6 | ベット / スタート / とめる | BET / START / STOP |
| 7 | タッチでコインを３まいいれます | Touch to insert 3 coins |
| 8 | タッチでドラムをまわします | Touch to spin the reels |
| 9 | タッチでドラムをひとつとめます | Touch to stop a reel |
| 10 | タッチでコインを１まいいれます | Touch to insert 1 coin |
| 11-15 | (result / play-stress / "n:" table headings with `{fffe}` format codes) | likely debug-only; check where the app prints them |

The overlay has no other strings (the machine's face, reels and lamps are graphics, `a/1/5/5`), so translation is a
dozen lines, and, as the graphics carry no Japanese words either apart from the machine art, no image editing is needed.
(Unchecked: whether the 47 files of `a/1/5/5` contain Japanese lettering in the BG tiles.)

**Graphics archives: two, both needed from the JP ROM (2026-11-14).** The slot app opens NARC `0x7F` = **`a/1/2/7`**
(`0x021E554C`; 25 files: 5 palettes, tile sets (NCGR 12912/14256/24640/8256/8256 bytes), 3-4 screens, cell/animation
banks) and **`a/1/5/5`** (47 files: one 24640-byte tile set and 2084-byte screen like `a/1/2/7`'s, then ten sprite sets of
palette + 4656-byte tiles + animation + cell bank, one per symbol/lamp). The USA ROM *has* `a/1/2/7` (same 25 files, same
sizes) but its **tile data is blanked** (99.8% zero bytes, only palettes, screens and animation banks survive), and `a/1/5/5`
is a 64-byte stub; so the import must read both from the Japanese ROM. The graphics code uses the usual field-less
loaders (palette `0x02003138`, tiles+screen `0x02007A5C`/`0x02007A80`, sprite resource `0x02007B60`) with file indices into
those NARCs (palettes 0, 4, 8, 20; tiles/screens 14+15, 9+10, 21+22 ...), plus a text window set-up for the three
string boxes (BET, START, STOP, ids 4-6 of the message bank).

For the port, the display can be reimplemented in C over the port's existing BG/sprite loaders (the same ones
Voltorb Flip uses), reading these archives from the imported ROM. It's the single biggest remaining piece after the
logic (about 120 of the 180 functions are display, animation and input).

Still to read for payouts: the per-symbol payout values (probably `0x021EA20x`-`0x021EA32x`, the coin amounts shown
on the machine), what `+0x3A` and the 3 reel results (`+0x50..55`) turn into on the reel strips, and where coins are
actually added (`+0x28 += +0x30 * payout`, near `0x021E5Cxx`/`0x021E7Bxx` which reference `0xC350` and `999`).

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

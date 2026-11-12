# Hall of Fame / credits crash (fixed)

**Symptom** (desktop and the Thor): after the Hall of Fame shows the six Pokémon, the sequence crashes when it
should go on to the "saving" message and the credits.

**Cause.** `ClearGame` (`src/clear_game.c`) runs the Hall of Fame as a child application. Starting one ends the field
map, which frees `fieldSystem->bgConfig` and destroys the `HEAP_ID_FIELD1` heap (`src/overlay005/fieldmap.c`), but
leaves `fieldSystem->bgConfig` pointing at the freed block. When the Hall of Fame ends, `ClearGame` recreates the
heap and `sub_02052F28` sets up its background through that dangling `bgConfig` (`Bg_InitFromTemplate` reads its heap
ID and allocates the tilemap from it). The DS never overwrote the freed memory, so it worked by luck; on the desktop
and Android builds the freed block is reused, `Heap_Alloc` returns NULL, and `MI_CpuClear16(NULL, ...)` crashes. It
has nothing to do with the Gen 5 setting or widescreen.

**Fix.** `sub_02052F28` makes its own `BgConfig_New(HEAP_ID_FIELD1)` and the cleanup frees it.

**Found with** `tests/scripts/hall_of_fame.txt` (`desktop.sh test tests/scripts/hall_of_fame.txt --from turtwig`),
which uses the new `hall-of-fame` script command to start the same sequence the Champion's room does. The crash
report named `sub_02052F28` at `clear_game.c:260`. After the fix it passes through the credits to the soft reset.

**Other use-after-free of the same kind:** any code that touches `fieldSystem->bgConfig` while a child application
runs. None other was seen here.

# Pokémon Center healing: the trainer sprite glitch (not found yet)

Tried on the desktop build with `tests/scripts/heal_shots.txt` (new `jump MAP X Z` script command: warps to Jubilife's
Pokémon Center, talks to the nurse, then takes a screenshot every few frames): about 130 frames of the exchange, from
the nurse's question to "Thank you for waiting", showed no wrong palette, sprite or OAM frame. The healing machine
(`src/overlay006/healing_machine_animation/pokecenter.c`) is only 3D map props (mini Poké Balls, the screen); it has
no trainer sprite of its own, so a glitch there would be the player's or the nurse's OBJ sprite, or the field's
transition. Screenshots taken by the harness pause the game for about 2 s each and skip frames, so a glitch of a
frame or two may just not be caught. What would help: whether the glitch is the player or the nurse, whether it
happens on the desktop build as well as the Thor, and at which line of the text; then a recording or a
`SIM` frame dump around it.

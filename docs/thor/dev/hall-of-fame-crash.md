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

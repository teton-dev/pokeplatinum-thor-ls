# The desktop build in Docker

The Linux desktop build, built and played in Docker with `tools/docker/desktop.sh` (written for macOS). The Linux
target is x86-64 (`SDK_X86`), so the container is x86-64; on an Apple Silicon Mac it runs under Rosetta.

## Setup

1. Docker Desktop; on Apple Silicon, with *Settings > General > Use Rosetta for x86_64/amd64 emulation on Apple
   Silicon* on.
2. A folder for the ROMs and the game's data, `~/thor-dev` by default (`THOR_DEV=...` for another). The game keeps
   its extracted ROM, saves and settings in `~/thor-dev/desktop`.
3. `tools/docker/desktop.sh build`. The first build makes the images (`pokeplatinum-build` from
   `tools/docker/Dockerfile` without devkitPro, and `pokeplatinum-desktop` from `Dockerfile.desktop`: a virtual
   display, VNC, screenshots and scripted input, and an arm64 compiler) and takes about 15 minutes; later ones only
   rebuild what changed. With a [pokebw2](https://github.com/fuddlesworth/pokebw2) clone next to this repository
   (`../pokebw2`, or `POKEBW2=...`), it builds the Gen 5 battle code too.

## Playing

`tools/docker/desktop.sh start` runs the game on a virtual display (1280x800, `SCREEN=WxH` for another). Watch and
play it in macOS's Screen Sharing: *Finder > Go > Connect to Server*, `vnc://localhost:5900`, no password (the port
only listens on this computer).

The first start asks for the Platinum ROM in a small file picker that opens in `~/thor-dev/desktop`. A link to the
ROM there puts it first in the list: `ln -s ../PKMNPLAT.nds ~/thor-dev/desktop/`. Double-click it; the files are
extracted once and kept.

Keys: Z = A, X = B, C = X, V = Y, Enter = Start, Right Shift = Select, arrows = D-pad, Q/E = L/R, \` = the settings,
Tab = fast-forward. The mouse is the touch screen.

## Scripted checks

For checks without a person at the screen (and for Claude):

| Command | Does |
|---|---|
| `desktop.sh shot [file.png]` | screenshot, by default `~/thor-dev/shots/shot.png` |
| `desktop.sh key Return z z` | presses keys (xdotool's names) |
| `desktop.sh hold 600 Up` | holds a key for 600 ms |
| `desktop.sh click X Y` | clicks or touches at screen coordinates |
| `desktop.sh type TEXT` | types text |
| `desktop.sh log [lines]` | the game's output (`[assert]` lines show here) |
| `desktop.sh stop` | stops the game |
| `desktop.sh shell [CMD]` | a shell, or one command, in the build container |
| `desktop.sh gen5-check` | the Gen 5 battle code's link check and arm64 compile check ([plan](gen5-battles.md)) |
| `desktop.sh test SCRIPT` | a scripted test: a new game, the first battle... ([tests/README.md](../../../tests/README.md)) |

## Known problems

- **The AppImage step** (`copy_pc_files.sh` runs linuxdeploy) is last; the game and its files are in
  `build_linux/pokeplatinum` before it. `desktop.sh build` says so and carries on when only that step fails, which it
  does when GitHub's release downloads fail. Rosetta also can't start an AppImage as it is (its magic number is in
  the ELF header's padding), so the script clears those three bytes of the downloaded linuxdeploy.
- **WrapDB outages:** when `wrapdb.mesonbuild.com` can't serve a wrap's patch, meson setup fails. Unpacking the
  source into `subprojects/<directory>/` with the wrap's build files from the
  [wrapdb repository](https://github.com/mesonbuild/wrapdb) (`subprojects/packagefiles/<name>` at the
  `<name>_<version>` tag) and applying the wrap's `diff_files` gets past it; meson uses an existing directory as is.
- **No sound** in the container (`SDL_AUDIODRIVER=dummy`).
- **Slow 3D:** the 3D is rendered in software, at the internal resolution the settings ask for (10 times the DS's
  by default). Under Rosetta that runs the overworld at a few frames a second; *Internal Resolution* at 1x or 2x
  in the libntr panel's *Config* (or `InternalResolutionScale` in `~/thor-dev/desktop/sim_config.ini`) makes it
  play smoothly. Test runs use 1.

#!/bin/bash
# The Linux desktop build in Docker, built and played on a Mac (teton-dev fork). See docs/thor/dev/desktop-docker.md.
#
#   tools/docker/desktop.sh build            make linux (and the Gen 5 code, when ../pokebw2 is there)
#   tools/docker/desktop.sh gen5-check       the Gen 5 code's link check and arm64 (Android) compile check
#   tools/docker/desktop.sh start            run the game on a virtual display; watch and play it over VNC
#   tools/docker/desktop.sh shot [file.png]  screenshot of the display
#   tools/docker/desktop.sh key KEY...       press keys (xdotool names: Return, x, z, Up, ...)
#   tools/docker/desktop.sh hold MS KEY      hold a key for MS milliseconds (walking)
#   tools/docker/desktop.sh type TEXT        type text (the ROM picker's path field)
#   tools/docker/desktop.sh click X Y        left click at display coordinates
#   tools/docker/desktop.sh log              the game's output
#   tools/docker/desktop.sh stop             stop the game and the display
#   tools/docker/desktop.sh shell [CMD...]   a shell (or a command) in the build container
#
# The container is x86-64 (the Linux target is SDK_X86) and runs under Rosetta on Apple Silicon.
# THOR_DEV (default ~/thor-dev) holds the ROMs and, in desktop/, the game's data folder: the extracted ROM, saves
# and settings, kept between runs. The game sees THOR_DEV at the same path as on the Mac.

set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
THOR_DEV=${THOR_DEV:-$HOME/thor-dev}
POKEBW2=${POKEBW2:-$(cd "$ROOT/.." && pwd)/pokebw2}
DATA=$THOR_DEV/desktop
NAME=pokeplatinum-desktop
IMAGE=pokeplatinum-desktop
SCREEN=${SCREEN:-1280x800}
VNC_PORT=${VNC_PORT:-5900}

export DOCKER_DEFAULT_PLATFORM=linux/amd64
command -v docker >/dev/null || export PATH="$HOME/.docker/bin:$PATH"

image() {
    if [ -z "$(docker images -q pokeplatinum-build)" ]; then
        docker build -t pokeplatinum-build -f "$ROOT/tools/docker/Dockerfile" --build-arg USE_DEVKITPRO=0 "$ROOT"
    fi
    if [ -z "$(docker images -q $IMAGE)" ]; then
        docker build -t $IMAGE -f "$ROOT/tools/docker/Dockerfile.desktop" "$ROOT/tools/docker"
    fi
}

mounts() {
    echo "--mount type=bind,src=$ROOT,target=$ROOT"
    [ -d "$POKEBW2" ] && echo "--mount type=bind,src=$POKEBW2,target=$POKEBW2"
    mkdir -p "$DATA"
    echo "--mount type=bind,src=$THOR_DEV,target=$THOR_DEV"
}

# In the running game's container, on its display
on_display() {
    docker exec -e DISPLAY=:1 $NAME "$@"
}

case "$1" in
build)
    image
    shift
    in_container() {
        docker run --rm -i --user "$(id -u):$(id -g)" -e HOME=/home -e APPIMAGE_EXTRACT_AND_RUN=1 $(mounts) -w "$ROOT" \
            $IMAGE sh -c "$*"
    }
    run_make() {
        in_container make linux "$@"
    }
    # The Gen 5 battle code (gen5/meson.build) with a pokebw2 clone next to this repository
    if [ -d "$POKEBW2" ]; then
        in_container "make build_linux/build.ninja && subprojects/meson-1.12.0/meson.py configure build_linux \
            -Dpokebw2=$POKEBW2"
    fi
    # Rosetta won't start an AppImage: its magic number sits in the ELF header's padding. Without it, linuxdeploy
    # runs (extracting itself, as APPIMAGE_EXTRACT_AND_RUN asks)
    LINUXDEPLOY=$ROOT/tools/linuxdeploy/linuxdeploy-x86_64.AppImage
    unmark() {
        [ -s "$LINUXDEPLOY" ] && [ "$(xxd -s 8 -l 3 -p "$LINUXDEPLOY")" = "414902" ] &&
            printf '\0\0\0' | dd of="$LINUXDEPLOY" bs=1 seek=8 count=3 conv=notrunc 2>/dev/null
    }
    # A failed download leaves an empty file, which the copy script takes for the tool
    [ -s "$LINUXDEPLOY" ] || rm -f "$LINUXDEPLOY"
    unmark || true
    run_make "$@" || { unmark && run_make "$@"; } || {
        # The AppImage is the last step: the game and its files are in place before it (copy_pc_files.sh)
        if cmp -s "$ROOT/build_linux/main" "$ROOT/build_linux/pokeplatinum/main"; then
            echo "The game is built in build_linux/pokeplatinum; only its AppImage failed (not needed to play here)."
        else
            exit 1
        fi
    }
    ;;
gen5-check)
    image
    docker run --rm -i --user "$(id -u):$(id -g)" -e HOME=/home $(mounts) -w "$ROOT" $IMAGE sh -c "
        ninja -C build_linux gen5/libgen5_battle_bw2.a >/dev/null &&
        echo 'Link check (symbols nothing defines):' && gen5/tools/link_check.sh build_linux &&
        python3 gen5/tools/arm64_check.py build_linux"
    ;;
start)
    image
    docker rm -f $NAME >/dev/null 2>&1 || true
    BIN=$ROOT/build_linux/pokeplatinum
    [ -x "$BIN/main" ] || { echo "No build: run $0 build first"; exit 1; }
    # The game reads firmware.bin and header.bin and keeps its data in its working directory
    docker run -d --name $NAME --user "$(id -u):$(id -g)" -e HOME=/home -p 127.0.0.1:$VNC_PORT:5900 \
        -e DISPLAY=:1 -e SDL_AUDIODRIVER=dummy -e LIBGL_ALWAYS_SOFTWARE=1 $(mounts) -w "$DATA" $IMAGE \
        sh -c "Xvfb :1 -screen 0 ${SCREEN}x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
               sleep 1
               x11vnc -display :1 -forever -shared -nopw -rfbport 5900 -quiet >/tmp/x11vnc.log 2>&1 &
               cp -f '$BIN/firmware.bin' '$BIN/header.bin' .
               exec '$BIN/main' $(printf "'%s' " "${@:2}")" >/dev/null
    echo "Running. Watch and play: open vnc://localhost:$VNC_PORT (Finder > Go > Connect to Server, no password)."
    ;;
shot)
    OUT=${2:-$THOR_DEV/shots/shot.png}
    mkdir -p "$(dirname "$OUT")"
    on_display import -window root "/tmp/shot.png"
    docker cp $NAME:/tmp/shot.png "$OUT" >/dev/null
    echo "$OUT"
    ;;
key)
    shift
    on_display xdotool key --delay 120 "$@"
    ;;
hold)
    on_display sh -c "xdotool keydown '$3'; sleep $(awk "BEGIN{print $2/1000}"); xdotool keyup '$3'"
    ;;
type)
    shift
    on_display xdotool type --delay 40 "$*"
    ;;
click)
    on_display xdotool mousemove "$2" "$3" click 1
    ;;
log)
    docker logs $NAME 2>&1 | tail -n "${2:-80}"
    ;;
stop)
    docker rm -f $NAME >/dev/null 2>&1 || true
    ;;
shell)
    image
    shift
    if [ $# -eq 0 ]; then
        docker run --rm -it --user "$(id -u):$(id -g)" -e HOME=/home -e APPIMAGE_EXTRACT_AND_RUN=1 $(mounts) -w "$ROOT" $IMAGE bash
    else
        docker run --rm -i --user "$(id -u):$(id -g)" -e HOME=/home -e APPIMAGE_EXTRACT_AND_RUN=1 $(mounts) -w "$ROOT" $IMAGE sh -c "$*"
    fi
    ;;
*)
    sed -n 2,18p "$0" | sed 's/^# \{0,1\}//'
    exit 1
    ;;
esac

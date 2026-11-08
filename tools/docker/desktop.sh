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
#   tools/docker/desktop.sh test SCRIPT [--from FIXTURE] [--keep FIXTURE]
#                                            run a test script (tests/README.md) in a fresh data folder, from a
#                                            fixture's save, and keep the save it ends with as a fixture
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
    LOG=$ROOT/build_linux/desktop-build.log
    run_make() {
        mkdir -p "$ROOT/build_linux"
        in_container make linux "$@" 2>&1 | tee "$LOG"
        return "${PIPESTATUS[0]}"
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
        # The AppImage is the last step: the game and its files are in place before it (copy_pc_files.sh). Only
        # when it is the one failure
        if [ "$(grep -c '^FAILED: ' "$LOG")" = 1 ] && grep -q '^FAILED: pokeplatinum.us' "$LOG" &&
            cmp -s "$ROOT/build_linux/main" "$ROOT/build_linux/pokeplatinum/main"; then
            echo "The game is built in build_linux/pokeplatinum; only its AppImage failed (not needed to play here)."
        else
            exit 1
        fi
    }
    ;;
test)
    image
    shift
    SCRIPT=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
    shift
    [ -f "$SCRIPT" ] || { echo "No script $SCRIPT"; exit 1; }
    FROM= KEEP=
    while [ $# -gt 0 ]; do
        case "$1" in
        --from) FROM=$2; shift 2 ;;
        --keep) KEEP=$2; shift 2 ;;
        *) echo "Unknown option $1"; exit 1 ;;
        esac
    done
    BIN=$ROOT/build_linux/pokeplatinum
    [ -x "$BIN/main" ] || { echo "No build: run $0 build first"; exit 1; }
    [ -f "$DATA/arm9.bin" ] || { echo "No extracted ROM in $DATA: start the game once ($0 start)"; exit 1; }
    TEST_NAME=$(basename "$SCRIPT" .txt)
    RUN=$THOR_DEV/test-run
    SHOTS=$THOR_DEV/test-shots/$TEST_NAME
    FIXTURES=$THOR_DEV/fixtures
    # A fresh data folder: the extracted ROM linked, no saves but the fixture's, and the default settings, so
    # that the player's (text speed...) don't change a test's timing
    rm -rf "$RUN" "$SHOTS"
    mkdir -p "$RUN" "$SHOTS"
    for entry in "$DATA"/*; do
        name=$(basename "$entry")
        case "$name" in
        save.bin | save_slot* | *.sav | DbgLog.txt | *.nds | sim_config.ini | sim_config_prj.ini | imgui.ini) ;;
        gen5 | gen5.tmp) ;;
        carts.ini | cart.name) cp "$entry" "$RUN/" ;;
        *) ln -s "$entry" "$RUN/$name" ;;
        esac
    done
    # The 3D at the DS's own size: the default (10 times as large) is far too slow in software, at a few frames a
    # second on the overworld
    printf '[General]\nInternalResolutionScale=1\n' > "$RUN/sim_config.ini"
    if [ -n "$FROM" ]; then
        [ -f "$FIXTURES/$FROM/save.bin" ] || { echo "No fixture $FIXTURES/$FROM"; exit 1; }
        cp "$FIXTURES/$FROM/save.bin" "$RUN/save.bin"
    fi
    TEST=pokeplatinum-test
    docker rm -f $TEST >/dev/null 2>&1 || true
    # The Black 2 or White 2 ROM a script's gen5-import imports
    GEN5_ROM=${GEN5_ROM:-$(ls "$THOR_DEV"/PKMNB2.nds "$THOR_DEV"/PKMNW2.nds 2>/dev/null | head -n 1)}
    docker run -d --name $TEST --user "$(id -u):$(id -g)" -e HOME=/home -e DISPLAY=:1 -e SDL_AUDIODRIVER=dummy \
        -e LIBGL_ALWAYS_SOFTWARE=1 -e LIBNTR_TEST_MODE=1 -e POKEPLATINUM_TEST_SCRIPT="$SCRIPT" \
        -e POKEPLATINUM_GEN5_ROM="$GEN5_ROM" $(mounts) -w "$RUN" \
        $IMAGE sh -c "Xvfb :1 -screen 0 ${SCREEN}x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
                      sleep 1
                      cp -f '$BIN/firmware.bin' '$BIN/header.bin' .
                      exec '$BIN/main'" >/dev/null
    echo "Running $TEST_NAME${FROM:+ from $FROM}"
    # The script's log, and its screenshots as it asks for them, while the container runs
    docker logs -f $TEST 2>&1 | while IFS= read -r line; do
        case "$line" in
        "[test] shot "*)
            shot=${line#\[test\] shot }
            # The game's window (both screens), found by its title
            docker exec -e DISPLAY=:1 $TEST sh -c \
                'import -window "$(xdotool search --name POKEMON | head -n 1)" /tmp/shot.png' 2>/dev/null &&
                docker cp $TEST:/tmp/shot.png "$SHOTS/$shot.png" >/dev/null 2>&1 && echo "$line -> $SHOTS/$shot.png" ;;
        "[test] "* | *"[assert]"* | *"Assert"*) echo "$line" ;;
        esac
    done &
    READER=$!
    # Whatever the script's own timeouts, a run ends after TEST_LIMIT seconds. Polled here, so that nothing left
    # running holds the output open after the run
    START=$(date +%s)
    TIMED_OUT=
    while [ "$(docker inspect -f '{{.State.Running}}' $TEST 2>/dev/null)" = true ]; do
        if [ $(($(date +%s) - START)) -ge "${TEST_LIMIT:-600}" ]; then
            echo "[test] FAIL: still running after ${TEST_LIMIT:-600} s"
            docker exec -e DISPLAY=:1 $TEST sh -c \
                'import -window "$(xdotool search --name POKEMON | head -n 1)" /tmp/shot.png' 2>/dev/null &&
                docker cp $TEST:/tmp/shot.png "$SHOTS/timeout.png" >/dev/null 2>&1
            docker stop -t 1 $TEST >/dev/null 2>&1
            TIMED_OUT=1
            break
        fi
        sleep 1
    done
    wait $READER 2>/dev/null || true
    CODE=$(docker inspect -f '{{.State.ExitCode}}' $TEST)
    [ "$CODE" = 0 ] || [ "$CODE" = 1 ] || CODE=1
    [ -z "$TIMED_OUT" ] || CODE=1
    docker logs $TEST > "$SHOTS/log.txt" 2>&1
    docker rm -f $TEST >/dev/null 2>&1 || true
    if [ "$CODE" = 0 ] && [ -n "$KEEP" ]; then
        [ -f "$RUN/save.bin" ] || { echo "The run ended without a save to keep"; exit 1; }
        mkdir -p "$FIXTURES/$KEEP"
        cp "$RUN/save.bin" "$FIXTURES/$KEEP/save.bin"
        echo "Kept the save as the fixture $KEEP ($FIXTURES/$KEEP)"
    fi
    echo "$TEST_NAME: $([ "$CODE" = 0 ] && echo passed || echo "FAILED (exit $CODE)"); log and screenshots in $SHOTS"
    exit "$CODE"
    ;;
gen5-check)
    image
    docker run --rm -i --user "$(id -u):$(id -g)" -e HOME=/home $(mounts) -w "$ROOT" $IMAGE sh -c "
        ninja -C build_linux gen5/gen5_battle_bw2.o >/dev/null &&
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
    sed -n 2,21p "$0" | sed 's/^# \{0,1\}//'
    exit 1
    ;;
esac

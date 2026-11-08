#!/bin/bash
# Milestone 1's link check (Gen 5 battles): lists the symbols libgen5_battle.a needs that neither it, libntr's
# libraries (the SDK, NitroSystem) nor the C library define. Prints nothing when the battle code links.
#
#   gen5/tools/link_check.sh BUILD_DIR      (build_linux)
set -e
BUILD=$1
LIB=$BUILD/gen5/libgen5_battle_bw2.a
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
nm -g --defined-only "$LIB" 2>/dev/null | awk 'NF==3{print $3}' | sort -u > "$TMP/defined"
nm -g --undefined-only "$LIB" 2>/dev/null | awk '{print $NF}' | grep -v ':$' | sort -u > "$TMP/undefined"
find "$BUILD/subprojects" -name '*.a' | xargs nm -g --defined-only 2>/dev/null | awk 'NF==3{print $3}' | sort -u \
    > "$TMP/sdk"
# _GLOBAL_OFFSET_TABLE_ is the linker's own
comm -23 "$TMP/undefined" "$TMP/defined" | comm -23 - "$TMP/sdk" | grep -v '^_GLOBAL_OFFSET_TABLE_$\|^$' > "$TMP/rest" || true
# What's left must link against the C library alone. Each is referenced by its symbol name only (an assembler
# label), so that no C declaration of memcpy, exit and the like gets in the way
{
    echo 'int main(void) { return 0; }'
    n=0
    while read -r symbol; do
        n=$((n + 1))
        echo "extern char ref$n[] __asm__(\"$symbol\"); char *use$n = ref$n;"
    done < "$TMP/rest"
} > "$TMP/check.c"
gcc -w -c "$TMP/check.c" -o "$TMP/check.o" # A failure here is the script's own
gcc "$TMP/check.o" -lm -o "$TMP/check" 2>&1 |
    grep -o "undefined reference to \`[^']*'" | sed "s/undefined reference to \`//; s/'$//" | sort -u || true

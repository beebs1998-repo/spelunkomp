#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# Release build for Linux, for the processor it runs on: x86_64 (the Steam Deck and Steam Machine included) or ARM64
# (aarch64): build/release/Spelunkomp-linux-x86_64.tar.gz or Spelunkomp-linux-arm64.tar.gz, a folder Spelunkomp/
# holding the collection's launcher (spelunkomp-launcher), every game's program beside it, librashader and the shader
# presets, and the licences; and update-linux-<processor>.txt, its manifest for the launcher's updates (lib.sh). Run
# on Linux with the packages of tools/release/Dockerfile (Rust included, for librashader), from the repository's top,
# with each game's translated code (lib.sh); on a Mac, in Docker (tools/release/all.sh does this; docs/BUILDING.md):
#   docker build --platform linux/amd64 -t spelunkomp-build tools/release
#   docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src spelunkomp-build tools/release/linux.sh
# On Apple silicon the amd64 image runs emulated, several times slower: tools/release/x86_64/Dockerfile is an arm64
# image with Ubuntu's x86_64 cross compilers that runs this unchanged at native speed.
# SDL 3 is built in statically from its checked source, and the C++ runtime is linked in, so the programs need only
# the system's usual X11 or Wayland, sound and zlib libraries (and OpenGL, for the shaders).
#   GAMES="a b"  those games only (a test; a release has them all)    NO_GAME_CODE=1  CI's build (lib.sh)
set -eu
. "$(dirname "$0")/lib.sh"
case "$(uname -m)" in
    x86_64) SYSTEM=linux-x86_64 ;;
    aarch64|arm64) SYSTEM=linux-arm64 ;;
    *) echo "error: no Linux release for $(uname -m)" >&2; exit 1 ;;
esac
BUILD="$ROOT/build/release-$SYSTEM"
sdl_static "$SYSTEM"
build_programs "$BUILD" -DSDL3_DIR="$DEPS/sdl-$SYSTEM/lib/cmake/SDL3" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"

STAGE="$BUILD/stage/$ARCHIVE"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
copy_programs "$BUILD" "$STAGE" ""
strip "$STAGE/$EXE-launcher" $(for game in $GAMES; do echo "$STAGE/$game"; done)
# (nothing but the system's own libraries: no C++ runtime, no SDL)
for program in "$STAGE/$EXE-launcher" $(for game in $GAMES; do echo "$STAGE/$game"; done); do
    if readelf -d "$program" | grep NEEDED | grep -q 'libstdc++\|libgcc_s\|libSDL3'; then
        echo "error: $program links a library the system may not have:" >&2
        readelf -d "$program" | grep NEEDED >&2
        exit 1
    fi
done
add_notices "$STAGE" "$BUILD/launcher"
add_librashader "$STAGE" librashader.so "$STAGE" "$STAGE/licenses"
rm -f "$OUT/$ARCHIVE-$SYSTEM.tar.gz"
tar -czf "$OUT/$ARCHIVE-$SYSTEM.tar.gz" -C "$BUILD/stage" "$ARCHIVE"
check_archive "$OUT/$ARCHIVE-$SYSTEM.tar.gz"
update_files "$SYSTEM" "$STAGE" "$OUT/$ARCHIVE-$SYSTEM.tar.gz"
echo "wrote $OUT/$ARCHIVE-$SYSTEM.tar.gz"

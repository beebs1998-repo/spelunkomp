#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# Release build for macOS (Apple silicon and Intel in one program, macOS 13 and later): build/release/
# Spelunkomp-macOS.zip holding Spelunkomp.app (the launcher, with every game's program beside it), and
# update-macos.txt, its manifest for the launcher's updates (lib.sh). Needs the translated code of each game, made
# from the builder's own ROM sets (lib.sh), and librashader (tools/librashader.sh, Rust: the Display page's shaders). SDL 3 is built in. The app is signed ad hoc and not notarized, so Gatekeeper
# warns the first time it is opened. Every game's program is started with its ROM set before the zip is made
# (lib.sh: ROMS=folder, ../cv1k-roms by default).
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/release-macos"
# (MAC_ARCHS=arm64: Apple silicon alone, for CI's pull requests, which are not releases)
ARCH="-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES=${MAC_ARCHS:-arm64;x86_64}"
sdl_static "macos-$(echo "${MAC_ARCHS:-arm64;x86_64}" | tr ';' '-')" $ARCH
# (SDL3_DIR: never Homebrew's SDL, which is one processor's and a library the app would need beside it)
build_programs "$BUILD" $ARCH -DSDL3_DIR="$DEPS/sdl-macos-$(echo "${MAC_ARCHS:-arm64;x86_64}" | tr ';' '-')/lib/cmake/SDL3"

APP="$OUT/$NAME.app"
rm -rf "$APP" && mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"
copy_programs "$BUILD" "$APP/Contents/MacOS" ""
strip -x "$APP/Contents/MacOS/"*
sed -e "s/@VERSION@/$VERSION/g" -e "s/@NAME@/$NAME/g" -e "s/@APP_ID@/$APP_ID/g" -e "s/@EXE@/$EXE-launcher/g" \
    "$ROOT/tools/release/Info.plist" > "$APP/Contents/Info.plist"
add_notices "$APP/Contents/Resources" "$BUILD/launcher"
add_librashader "$APP/Contents/Frameworks" librashader.dylib "$APP/Contents/Resources" "$APP/Contents/Resources/licenses"
codesign --force --deep -s - "$APP"

for binary in "$APP/Contents/MacOS/"* "$APP/Contents/Frameworks/"*; do
    if otool -L "$binary" | grep -q '/opt/homebrew\|/usr/local\|libSDL3'; then
        echo "error: $binary links a library outside macOS:" >&2
        otool -L "$binary" >&2
        exit 1
    fi
    for arch in $(echo "${MAC_ARCHS:-arm64;x86_64}" | tr ';' ' '); do
        lipo "$binary" -verify_arch "$arch" || { echo "error: $binary has no $arch code" >&2; exit 1; }
    done
done
check_programs_start "$APP/Contents/MacOS"
rm -f "$OUT/$ARCHIVE-macOS.zip"
(cd "$OUT" && ditto -c -k --norsrc --noextattr --keepParent "$NAME.app" "$ARCHIVE-macOS.zip")
check_archive "$OUT/$ARCHIVE-macOS.zip"
update_files macos "$APP" "$OUT/$ARCHIVE-macOS.zip"
echo "wrote $OUT/$ARCHIVE-macOS.zip"

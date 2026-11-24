#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# Release build for Windows x64, cross-compiled with MinGW-w64 on Linux: build/release/Spelunkomp-windows-x64.zip
# (the launcher, every game's program, SDL3.dll, librashader.dll and the shader presets, licences) and
# update-windows-x64.txt, its manifest for the launcher's updates (lib.sh). Needs the packages of
# tools/release/Dockerfile, each game's translated code (lib.sh), and Rust with its x86_64-pc-windows-gnu target for
# librashader (tools/librashader.sh --windows). zlib and the C++ and MinGW-w64 runtimes are linked in.
#   docker build --platform linux/arm64 -t spelunkomp-build-arm64 tools/release
#   docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src spelunkomp-build-arm64 tools/release/windows.sh
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/release-windows"
SDL="$DEPS/SDL3-mingw-$SDL_VERSION"
if [ ! -d "$SDL/x86_64-w64-mingw32" ]; then
    curl -fsSL -o "$DEPS/sdl3-mingw.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-devel-$SDL_VERSION-mingw.tar.gz"
    echo "$SDL_MINGW_SHA256  $DEPS/sdl3-mingw.tar.gz" | sha256 -c - >/dev/null
    mkdir -p "$SDL" && tar -xzf "$DEPS/sdl3-mingw.tar.gz" -C "$SDL" --strip-components=1
    rm -f "$DEPS/sdl3-mingw.tar.gz"
fi
sdl_source # (for its licence text)
build_programs "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/release/mingw-w64.cmake" \
    -DCMAKE_PREFIX_PATH="$SDL/x86_64-w64-mingw32" -DZLIB_USE_STATIC_LIBS=ON -DCMAKE_EXE_LINKER_FLAGS="-static"

STAGE="$BUILD/stage/$ARCHIVE"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
copy_programs "$BUILD" "$STAGE" .exe
cp "$SDL/x86_64-w64-mingw32/bin/SDL3.dll" "$STAGE/"
# (the programs unstripped, kept beside the archive, not in it: a crash log's program+offset into a function name,
# tools/release/crash_where.sh)
rm -rf "$BUILD/symbols" && mkdir -p "$BUILD/symbols" && cp "$STAGE"/*.exe "$BUILD/symbols/"
x86_64-w64-mingw32-strip "$STAGE"/*.exe
# (linked statically but for SDL3.dll and Windows' own (WinHTTP: the launcher's requests, recomp-common); anything
# else here is a mistake)
if x86_64-w64-mingw32-objdump -p "$STAGE"/*.exe | grep -i 'DLL Name' | grep -viE 'SDL3|KERNEL32|msvcrt|USER32|SHELL32|ADVAPI32|ole32|api-ms-|GDI32|IMM32|WINMM|VERSION|SETUPAPI|OLEAUT32|COMDLG32|WS2_32|IPHLPAPI|WINHTTP'; then
    echo "error: a program needs a DLL that is not in the archive" >&2
    exit 1
fi
add_notices "$STAGE" "$BUILD/launcher"
add_librashader "$STAGE" librashader.dll "$STAGE" "$STAGE/licenses" --windows
if x86_64-w64-mingw32-objdump -p "$STAGE/librashader.dll" | grep -i 'DLL Name' | grep -viE 'KERNEL32|ntdll|msvcrt|USER32|ADVAPI32|api-ms-|bcrypt|WS2_32|userenv|OPENGL32|GDI32|ole32|OLEAUT32|SHELL32'; then
    echo "error: librashader.dll needs a DLL that is not in the archive" >&2
    exit 1
fi
# (zlib and the MinGW-w64 and GCC runtimes are linked into the programs)
cp "$(ls /usr/share/doc/libz-mingw-w64*/copyright | head -1)" "$STAGE/licenses/zlib.txt"
cp /usr/share/doc/mingw-w64-common/copyright "$STAGE/licenses/MinGW-w64-runtime.txt"
cp /usr/share/doc/gcc-mingw-w64-base/copyright "$STAGE/licenses/GCC-runtime.txt"
rm -f "$OUT/$ARCHIVE-windows-x64.zip"
(cd "$BUILD/stage" && zip -qr "$OUT/$ARCHIVE-windows-x64.zip" "$ARCHIVE")
check_archive "$OUT/$ARCHIVE-windows-x64.zip"
update_files windows-x64 "$STAGE" "$OUT/$ARCHIVE-windows-x64.zip"
echo "wrote $OUT/$ARCHIVE-windows-x64.zip"

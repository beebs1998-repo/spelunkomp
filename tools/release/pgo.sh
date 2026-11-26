#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#
# Profile-guided optimisation for the macOS release (clang's): build/pgo/spelunkomp.profdata, which macos.sh builds
# with (PGO=1, or PGO_PROFILE=file). Each game's program is built instrumented (Apple silicon only: the profile is the
# source's, not the processor's, and the release's Intel half is built with it too), then played with no window and
# no sound, as fast as it goes, the ways a player's time goes:
#   - begun at stage 1 and played there, the slowdown on, with the GPU renderer and with the software one;
#   - the same with run-ahead (1 frame);
#   - taken straight to stage 1's boss and played there (where the game has that), the slowdown on and off.
# Then the profiles are merged. A game not played has its own code built without a profile; the code the games share
# (the boards, the renderers, the program) is profiled by every game played. Needs each game's translated code and its
# ROM set (lib.sh; ROMS=folder, ../cv1k-roms by default).
#   tools/release/pgo.sh               # every game (GAMES="a b": those), about a minute a game
#   env: PGO_FRAMES=3600 (the frames of each run after the game begins)
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/pgo-train"
PROF="$ROOT/build/pgo"
ROMS="${ROMS:-$ROOT/../cv1k-roms}"
FRAMES="${PGO_FRAMES:-3600}"
ARCH="-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES=arm64"
sdl_static macos-arm64 $ARCH
INSTR=-fprofile-instr-generate
build_programs "$BUILD" $ARCH -DSDL3_DIR="$DEPS/sdl-macos-arm64/lib/cmake/SDL3" -DCMAKE_C_FLAGS="$INSTR" \
    -DCMAKE_CXX_FLAGS="$INSTR" -DCMAKE_EXE_LINKER_FLAGS="$INSTR"

rm -rf "$PROF/raw"
mkdir -p "$PROF/raw"
home=$(mktemp -d)
for game in $GAMES; do
    set=$(sed -n 's/^rom_set = //p' "$ROOT/games/$game/game.cfg" | head -1)
    [ -e "$ROMS/$set.zip" ] || { echo "error: no $ROMS/$set.zip to play $game with (ROMS=folder)" >&2; exit 1; }
    title=$(sed -n 's/^title_frame = //p' "$ROOT/games/$game/game.cfg" | head -1)
    frames=$(( ${title:-3000} + FRAMES ))
    program="$BUILD/game-$game/$game"
    features=$("$program" --list-features 2>/dev/null || true)
    play() { # name, then the program's arguments
        name=$1; shift
        mkdir -p "$home/$game-$name"
        if ! HOME="$home" SPELUNK_HIDDEN_WINDOW=1 LLVM_PROFILE_FILE="$PROF/raw/$game-$name-%p.profraw" "$program" \
            --roms "$ROMS" --saves "$home/$game-$name" --settings "$home/$game-$name/settings.ini" --mute \
            --frames "$frames" --begin-now 5 --press 0:1000000:0x10 "$@" > "$home/$game-$name.log" 2>&1; then
            echo "error: $game's $name run ended badly ($home/$game-$name.log)" >&2
            exit 1
        fi
    }
    play gpu --features out_of_harm=1
    play software --software --features out_of_harm=1
    play ahead --features out_of_harm=1,run_ahead=1
    if echo "$features" | grep -q '^to_boss'; then
        play boss --features to_boss=1,out_of_harm=1
        echo "$features" | grep -q '^slowdown' && play boss-fast --features to_boss=1,out_of_harm=1,slowdown=1
    fi
    echo "$game: played"
done
rm -rf "$home"
xcrun llvm-profdata merge -o "$PROF/spelunkomp.profdata" "$PROF/raw/"*.profraw
# (what the profile was made from: macos.sh says so when it builds, and warns when the source is not that any more)
{
    echo "commit $(git -C "$ROOT" rev-parse HEAD)$(git -C "$ROOT" diff --quiet HEAD -- . ':!work' ':!build' || echo ' (with changes not committed)')"
    echo "games $GAMES"
    echo "made $(date -u +%Y-%m-%dT%H:%MZ) by $(xcrun clang --version | head -1)"
} > "$PROF/spelunkomp.profdata.txt"
echo "wrote $PROF/spelunkomp.profdata ($(ls "$PROF/raw" | wc -l | tr -d ' ') runs, $(echo $GAMES | wc -w | tr -d ' ') games)"

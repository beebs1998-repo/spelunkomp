#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# A PGM game's translated Z80 sound driver against the interpreter: the same run (the game's recipe, or the options
# given) with the Z80 interpreted and on its translated code must give the same hash of the board after every frame
# (the Z80's RAM and registers in it) and the same sound, sample for sample; and two translated runs must repeat
# each other. Then how much of the Z80's time ran translated.
#   tools/checkz80.sh ROMS [FRAMES [RUN OPTIONS...]]   (GAME=ddpdoj, B=build/$GAME; FRAMES 7000)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/checkz80.sh ROMS [FRAMES [RUN OPTIONS...]]}
FRAMES=${2:-7000}
shift; [ $# -gt 0 ] && shift
GAME=${GAME:-ddpdoj}
B=${B:-build/$GAME}
if [ $# -eq 0 ] && [ -f games/$GAME/recipe.txt ]; then
    set -- $(grep -v '^#' games/$GAME/recipe.txt)
fi
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cmake -S . -B $B -G Ninja -DGAME=$GAME >/dev/null
ninja -C $B spelunk-headless >/dev/null
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --z80 interpreter --hash-log "$T/interpreter" --wav "$T/interpreter.wav" "$@" 2>/dev/null &
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --hash-log "$T/native" --wav "$T/native.wav" "$@" 2>"$T/log" &
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --hash-log "$T/native2" --wav "$T/native2.wav" "$@" 2>/dev/null &
wait
fail=0
cmp -s "$T/interpreter" "$T/native" || { echo "FAIL: translated Z80 code differs from the interpreter at frame $(cmp "$T/interpreter" "$T/native" | sed 's/.*line //')"; fail=1; }
cmp -s "$T/interpreter.wav" "$T/native.wav" || { echo "FAIL: the sound differs"; fail=1; }
cmp -s "$T/native" "$T/native2" && cmp -s "$T/native.wav" "$T/native2.wav" || { echo "FAIL: two translated runs differ"; fail=1; }
grep '^z80 code' "$T/log"
[ $fail = 0 ] && echo "checkz80: $GAME, $FRAMES frames: translated Z80 == interpreted (every frame's hash, the sound sample for sample), and repeatable"
exit $fail

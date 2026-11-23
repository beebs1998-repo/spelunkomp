#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The ARM beside the 68000 (DoDonPachi II's IGS027A: docs/PGM.md), checked over the game's recipe:
#   the processor itself: tests/arm7_test.cpp (each kind of instruction against what ARM's description says it gives)
#   waits     a run with the ARM's waiting loops counted (as the game runs) and one with every round of them run
#             give the same hash of the whole board, the ARM in it, after every frame
#   steps     runs with the ARM brought up to the 68000's time 16, 64 (as the game runs) and 1024 times a frame give
#             the same work RAM after every frame and the same frames with a new picture: the game does not depend
#             on how finely the two are run side by side
#   late      no write of the 68000's came while the ARM was ahead of it
#   tools/check_arm.sh ROMS [FRAMES]   (GAME=ddp2, B=build/$GAME; FRAMES 7000)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check_arm.sh ROMS [FRAMES]}
FRAMES=${2:-7000}
GAME=${GAME:-ddp2}
B=${B:-build/$GAME}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cmake -S . -B $B -G Ninja -DGAME=$GAME >/dev/null
ninja -C $B spelunk-headless arm7-test >/dev/null
./$B/arm7-test
set -- $(grep -v '^#' games/$GAME/recipe.txt)
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --hash-log "$T/as" --ram-hash-log "$T/ram64" --frame-log "$T/fl64" "$@" 2>"$T/log" &
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --arm-plain --hash-log "$T/plain" "$@" 2>/dev/null &
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --arm-steps 16 --ram-hash-log "$T/ram16" --frame-log "$T/fl16" "$@" 2>/dev/null &
./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --arm-steps 1024 --ram-hash-log "$T/ram1024" --frame-log "$T/fl1024" "$@" 2>/dev/null &
wait
fail=0
cmp -s "$T/as" "$T/plain" || { echo "FAIL: waits: counting the ARM's waiting loops differs from running them at frame $(cmp "$T/as" "$T/plain" | sed 's/.*line //')"; fail=1; }
for n in 16 1024; do
    cmp -s "$T/ram64" "$T/ram$n" && cmp -s "$T/fl64" "$T/fl$n" || { echo "FAIL: steps: $n steps a frame and 64 differ ($(cmp "$T/ram64" "$T/ram$n" | sed 's/.*line /frame /'))"; fail=1; }
done
grep -q "^arm: 0 writes" "$T/log" || { echo "FAIL: late: $(grep '^arm: .*writes' "$T/log")"; fail=1; }
grep '^arm:' "$T/log"
[ $fail = 0 ] && echo "check_arm: $GAME, $FRAMES frames: waits counted == run; 16, 64 and 1024 steps a frame give the same game; no write behind the ARM"
exit $fail

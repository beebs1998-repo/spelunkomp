#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The translation's check: the same run on the block interpreter and on the translated code must give the same hash
# of what the game can see after every frame, and two translated runs must repeat each other.
#   tools/check.sh ROMS [FRAMES]   (GAME=mushisam, B=build/$GAME; TIMING=1: with the board's slowdown worked out,
#   docs/SLOWDOWN.md, which must leave the two equal too)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check.sh ROMS [FRAMES]}
FRAMES=${2:-6000}
GAME=${GAME:-mushisam}
B=${B:-build/$GAME}
OUT=work/$GAME/check
mkdir -p $OUT
# Each game's way into play (a coin, Start, button 1 through its selects, then held fire and movement)
case $GAME in
mushisam) PLAY="--system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4700:1200:0x11" ;;
*) PLAY="--system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10 --press 4900:5:0x10 --press 5000:5:0x10 --press 5200:800:0x11" ;;
esac
PLAY="$PLAY ${TIMING:+--timing}"
run() {
    ./$B/spelunk-headless --game games/$GAME/game.cfg --roms "$ROMS" --frames $FRAMES --cpu $1 --hash-log $OUT/$2.txt $PLAY 2>&1 | tail -1
}
run blocks blocks &
run native native &
run native native2 &
wait
fail=0
cmp -s $OUT/blocks.txt $OUT/native.txt || { echo "FAIL: translated code differs from the interpreter at frame $(cmp $OUT/blocks.txt $OUT/native.txt | sed 's/.*line //')"; fail=1; }
cmp -s $OUT/native.txt $OUT/native2.txt || { echo "FAIL: two translated runs differ"; fail=1; }
[ $fail = 0 ] && echo "ok: $GAME, $FRAMES frames${TIMING:+, with the slowdown worked out}: translated == interpreted, and repeatable"
exit $fail

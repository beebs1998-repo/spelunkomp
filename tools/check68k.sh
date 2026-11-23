#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# A Cave 68000 game's translated code against the interpreter, block by block (registers, flags, cycles, every
# write), over a run of play; then that two translated runs and a run of the interpreter stopping where blocks do
# go the same way frame by frame.
#   tools/check68k.sh ROMS [FRAMES [RUN OPTIONS...]]   (GAME=ddonpach; the game's own recipe when no options are given)
# A block that differs because the play reached code by a computed jump the translator had not seen is not a fault:
# the place is added to work/<game>/entries.txt, the code translated again and the check run again (up to 8 times).
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check68k.sh ROMS [FRAMES]}
FRAMES=${2:-7000}
shift; [ $# -gt 0 ] && shift
GAME=${GAME:-ddonpach}
W=work/$GAME
if [ $# -eq 0 ] && [ -f games/$GAME/recipe.txt ]; then
    set -- $(grep -v '^#' games/$GAME/recipe.txt)
fi
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
n=0
while :; do
    cmake -S . -B build/$GAME -G Ninja -DGAME=$GAME >/dev/null
    ninja -C build/$GAME spelunk-headless >/dev/null
    # (a run on the translated code, noting where it came out of it at a place no block starts)
    cp $W/entries.txt "$T/before"
    ./build/$GAME/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --record-entries $W/entries.txt --hash-log "$T/native" "$@" 2>"$T/log"
    if ! cmp -s "$T/before" $W/entries.txt; then
        n=$((n + 1))
        [ $n -le 8 ] || { echo "check68k: still finding new places after 8 rounds"; exit 1; }
        echo "check68k: $(($(grep -vc '^#' $W/entries.txt) - $(grep -vc '^#' "$T/before"))) new places reached: translating again"
        python3 tools/m68k/recomp.py --program $W/program.bin --entries $W/entries.txt --out $W/gen --guess $W/guesses.txt \
            --hook-sources games/$GAME/src --list $W/program.lst
        continue
    fi
    break
done
cmake -S . -B build/$GAME-check -G Ninja -DGAME=$GAME -DCHECK68K=ON >/dev/null
ninja -C build/$GAME-check spelunk-headless >/dev/null
./build/$GAME-check/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --cpu check "$@" 2>&1 | tail -12
./build/$GAME/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --cpu blocks --hash-log "$T/blocks" "$@" 2>/dev/null
./build/$GAME/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --hash-log "$T/native2" "$@" 2>/dev/null
cmp "$T/native" "$T/native2" && cmp "$T/native" "$T/blocks" && echo "check68k: $GAME: translated == interpreted over $FRAMES frames, and two runs alike" && tail -1 "$T/log"

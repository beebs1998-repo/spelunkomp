#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Translates a Cave 68000 game's code: everything fixed branches and calls reach from the reset vector, the
# interrupt vectors and the places kept in work/<game>/entries.txt (reached by computed jumps: found by running the
# game) becomes C++ (tools/m68k/recomp.py) in work/<game>/gen, and the game is built with it.
#   tools/translate68k.sh ROMS [RUNS [FRAMES [RUN OPTIONS...]]]   (GAME=ddonpach and B=build/$GAME by default)
# RUNS rounds of made-up play (6 runs side by side each, FRAMES long: 20000) find more entries first, each round
# built with what the one before found; 0: only translate from the entries there.
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/translate68k.sh ROMS [RUNS [FRAMES]]}
RUNS=${2:-0}
FRAMES=${3:-20000}
shift; [ $# -gt 0 ] && shift; [ $# -gt 0 ] && shift
GAME=${GAME:-ddonpach}
B=${B:-build/$GAME}
W=work/$GAME
mkdir -p $W
build() {
    cmake -S . -B $B -G Ninja -DGAME=$GAME >/dev/null
    ninja -C $B spelunk-headless >/dev/null
}
translate() {
    python3 tools/m68k/recomp.py --program $W/program.bin --entries $W/entries.txt --out $W/gen --guess $W/guesses.txt \
        --hook-sources games/$GAME/src --list $W/program.lst
}
build
./$B/spelunk-headless --roms "$ROMS" --dump-program $W/program.bin
# (the interrupt vectors are places to start from too)
[ -f $W/entries.txt ] || python3 - $W/program.bin > $W/entries.txt <<'PY'
import sys
mem = open(sys.argv[1], "rb").read()
print("# Places the game's code was seen to reach in ways the translator cannot follow")
for v in sorted({int.from_bytes(mem[a:a + 4], "big") for a in range(0x60, 0x80, 4)}):
    if v < len(mem) and not v & 1:
        print("%06X" % v)
PY
round=0
while [ $round -lt "$RUNS" ]; do
    translate
    build
    for seed in 1 2 3 4 5 6; do
        s=$((round * 6 + seed))
        ./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --auto $s --record-entries $W/entries-$s.txt "$@" 2>/dev/null &
    done
    wait
    before=$(grep -vc '^#' $W/entries.txt)
    { echo "# Places the game's code was seen to reach in ways the translator cannot follow"; cat $W/entries.txt $W/entries-*.txt | grep -v '^#' | sort -u; } > $W/entries.new
    mv $W/entries.new $W/entries.txt
    rm -f $W/entries-*.txt
    echo "round $((round + 1)): $before -> $(grep -vc '^#' $W/entries.txt) entries"
    round=$((round + 1))
done
translate
cmake -S . -B $B -G Ninja -DGAME=$GAME >/dev/null
ninja -C $B

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Translates a game's Z80 sound driver (a PGM game's; Mazinger Z's and Air Gallet's: see the end of this note). The driver is not in a ROM: the 68000 copies it into the Z80's RAM at
# boot, so the game is run (its recipe, then made-up play) and the Z80's RAM taken after FRAMES (work/<game>/z80.bin);
# tools/z80/recompz80.py makes C++ of the code there (z80_*.cpp in work/<game>/gen, beside the 68000's), starting from
# the reset, the interrupts and the places in work/<game>/entries_z80.txt (computed jumps and the mode 2 interrupt's
# vectors, noted by the runs), and the game is built with it. The translation keeps the bytes it was made from and
# runs only while the RAM holds them (runtime/native_z80.h): a driver that differs is interpreted.
# On Cave's 68000 board (Mazinger Z, Air Gallet) the driver is in a ROM: the 16 KB of it that are always there are
# translated (the rest is seen a 16 KB bank at a time: left to the interpreter).
#   tools/translatez80.sh ROMS [RUNS [FRAMES]]   (GAME=ddpdoj, B=build/$GAME; RUNS rounds of 4 runs side by side
#   noting entries, each built with what the one before found: 2 by default; FRAMES 20000)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/translatez80.sh ROMS [RUNS [FRAMES]]}
RUNS=${2:-2}
FRAMES=${3:-20000}
GAME=${GAME:-ddpdoj}
B=${B:-build/$GAME}
W=work/$GAME
mkdir -p $W
RECIPE=$(grep -v '^#' games/$GAME/recipe.txt 2>/dev/null || true)
# (a game that starts under the PGM's BIOS has the BIOS's own driver in the Z80's RAM first: the places that one
# reaches are not noted. game.cfg's z80_driver_frame: the game's driver is there by then)
FROM=$(sed -n 's/^z80_driver_frame = //p' games/$GAME/game.cfg)
FROM=${FROM:+--z80-entries-from $FROM}
build() {
    cmake -S . -B $B -G Ninja -DGAME=$GAME -DCMAKE_BUILD_TYPE=Release >/dev/null
    ninja -C $B spelunk-headless >/dev/null
}
LIMIT=
grep -q '^hardware = cave68k' games/$GAME/game.cfg && LIMIT="--limit 4000"
translate() {
    python3 tools/z80/recompz80.py --program $W/z80.bin --entries $W/entries_z80.txt --out $W/gen --list $W/z80.lst $LIMIT
}
build
# (the driver as the RAM holds it once the game is going; then the same a long way on, or the code is not fixed)
./$B/spelunk-headless --roms "$ROMS" --frames 3000 --z80 interpreter --dump-z80 3000:$W/z80.bin $RECIPE 2>/dev/null
[ -f $W/entries_z80.txt ] || echo "# Places the Z80's code was seen to reach in ways the translator cannot follow" > $W/entries_z80.txt
round=0
while [ $round -lt "$RUNS" ]; do
    translate
    build
    ./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --record-z80-entries $W/entries_z80-r.txt $FROM $RECIPE 2>/dev/null &
    for seed in 1 2 3; do
        s=$((round * 3 + seed))
        ./$B/spelunk-headless --roms "$ROMS" --frames "$FRAMES" --auto $s --record-z80-entries $W/entries_z80-$s.txt $FROM 2>/dev/null &
    done
    wait
    before=$(grep -vc '^#' $W/entries_z80.txt || true)
    { echo "# Places the Z80's code was seen to reach in ways the translator cannot follow"; cat $W/entries_z80.txt $W/entries_z80-*.txt | grep -v '^#' | sort -u; } > $W/entries_z80.new
    mv $W/entries_z80.new $W/entries_z80.txt
    rm -f $W/entries_z80-*.txt
    echo "round $((round + 1)): $before -> $(grep -vc '^#' $W/entries_z80.txt) entries"
    round=$((round + 1))
done
translate
build
ninja -C $B >/dev/null 2>&1 || true

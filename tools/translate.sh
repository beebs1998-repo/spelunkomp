#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Translates the game's code: runs it a block at a time (the interpreter) to record the blocks it runs, makes C++ of
# them (tools/translate.py) into work/<game>/gen and builds with them.
#   tools/translate.sh ROMS [FRAMES [RUN OPTIONS...]]  (GAME=mushisam and B=build/$GAME by default; recordings are kept
#   and added to; RUN OPTIONS go to the run, for example --system and --press to reach play; FRAMES 0: no run, the
#   recordings there are translated, as after tools/explore.sh; FOUND=1 the blocks found by following the code too)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/translate.sh ROMS [FRAMES]}
FRAMES=${2:-3000}
shift; [ $# -gt 0 ] && shift
GAME=${GAME:-mushisam}
B=${B:-build/$GAME}
mkdir -p work/$GAME
cmake -S . -B $B -G Ninja -DGAME=$GAME >/dev/null
ninja -C $B spelunk-headless >/dev/null
if [ "$FRAMES" != 0 ]; then
    ./$B/spelunk-headless --game games/$GAME/game.cfg --roms "$ROMS" --frames "$FRAMES" --cpu blocks \
        --record work/$GAME/blocks-$FRAMES.txt ${STATIC:+--record-static $STATIC} "$@" 2>&1 | tail -1
fi
python3 tools/translate.py ${FOUND:+--found} work/$GAME/gen work/$GAME/blocks-*.txt
cmake -S . -B $B -G Ninja -DGAME=$GAME >/dev/null
ninja -C $B

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Input lag, measured (docs/LAG.md): a button is pressed at a known frame of play and the first frame whose picture
# differs from the same run without the press is found, for each Run-ahead setting. Also checks that run-ahead leaves
# the game itself exactly as it was (the same state after the same frames).
#   tools/lag.sh ROMS   (GAME=mushisam, B=build/$GAME)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/lag.sh ROMS}
GAME=${GAME:-mushisam}
B=${B:-build/$GAME}
OUT=work/$GAME/lag
mkdir -p $OUT
# Into play with free play (Start, button 1 through the selects), then the stick pushed right at frame PRESS
INTO="--system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10"
PRESS=${PRESS:-5000}
END=$((PRESS + 40))
run() { # name run-ahead [press]
    SPELUNK_HIDDEN_WINDOW=1 SPELUNK_REPORT_STATE=1 ./$B/$GAME --roms "$ROMS" --mute --frames $END --features run_ahead=$2 $INTO $3 \
        --picture-log $OUT/$1.txt 2> $OUT/$1.err
}
fail=0
# (two copies at a time: each opens a window, hidden, and the machine's rule is at most two of those at once)
for n in 0 1 2 3; do
    run base$n $n "" &
    run press$n $n "--press $PRESS:30:0x08" &
    wait
done
for n in 0 1 2 3; do
    first=$(paste -d' ' $OUT/base$n.txt $OUT/press$n.txt | awk '$2 != $4 { print $1; exit }')
    [ -n "$first" ] && echo "$GAME run-ahead $n: a press read in frame $((PRESS + 1)) first shows in the picture of frame $first: $((first - PRESS - 1)) frames after" \
        || echo "$GAME run-ahead $n: the press never showed"
    # (the game itself: the same state whatever the run-ahead)
    a=$(grep '^state:' $OUT/press0.err); b=$(grep '^state:' $OUT/press$n.err)
    [ "$a" = "$b" ] || { echo "FAIL: run-ahead $n changed the game ($a / $b)"; fail=1; }
done
[ $fail = 0 ] && echo "ok: run-ahead leaves the game's state the same"
exit $fail

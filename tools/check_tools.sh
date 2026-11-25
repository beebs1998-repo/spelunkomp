#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The training tools' check (runtime/timeline.h): the practice point and rewind put the game back exactly, and neither
# changes it while unused. Each game is played to a place, then:
#   idle      both tools on and never used: the same state and VRAM as a run without them
#   point     the point set at frame SET, gone back to 300 frames later, played on: the same as a run that simply
#             played that many frames (the game's state, all of VRAM, the picture)
#   rewind    rewind held for HOLD frames (300: 5 s) from frame SET + 400 + 3 HOLD, played on: the same as a run that
#             simply played to the frame it ends on, and as far back as the hold goes (a moment of 6 frames every other
#             frame held: 15 s of play; still after SET, the controls the same in both)
#   both      with run-ahead on, the point set, rewind held, back to the point, played on
# With the GPU and (SOFT=1 too) the software blitter.
#   tools/check_tools.sh ROMS [GAME...]   (BUILDS=build: the folder of each game's build, BUILDS/<id>/<id>)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check_tools.sh ROMS [GAME...]}
shift
GAMES=${*:-$(for cfg in games/*/game.cfg; do basename "$(dirname "$cfg")"; done)}
SET=${SET:-5000}
HOLD=${HOLD:-300}
REWIND=$((SET + 400 + 3 * HOLD)) # (where rewind is held)
BUILDS=${BUILDS:-./build} # (where each game's program is: BUILDS/<id>/<id>; tools/shadow/ gives its shadow builds)
# Into play with free play, then fire held for good (the controls must not depend on when the tools were used)
PLAY="--system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10 --press 4900:99999:0x11"
fail=0
for g in $GAMES; do
    [ -x $BUILDS/$g/$g ] || { echo "$g: not built"; continue; }
    out=work/$g/tools
    mkdir -p $out
    run() { # name frames [more...]
        name=$1; frames=$2; shift 2
        SPELUNK_HIDDEN_WINDOW=1 SPELUNK_REPORT_STATE=1 $BUILDS/$g/$g --roms "$ROMS" --mute $MODE --frames $frames --shot $frames:$out/$name.png $PLAY "$@" \
            2>&1 | grep '^state:' > $out/$name.txt || true
    }
    same() { # what a b
        if [ -s $out/$2.txt ] && cmp -s $out/$2.txt $out/$3.txt && cmp -s $out/$2.png $out/$3.png; then
            echo "$g$MODE: $1: ok ($(cut -d' ' -f1-3 $out/$2.txt))"
        else
            echo "$g$MODE: $1: FAIL: $(cat $out/$2.txt) / $(cat $out/$3.txt)"; fail=1
        fi
    }
    for MODE in "" ${SOFT:+" --software"}; do
        # (two at a time: each opens a window, hidden)
        run idle $((SET + 600)) --features practice_point,rewind &
        run plain $((SET + 600)) --features -practice_point,-rewind &
        wait
        same "unused" idle plain
        run point $((SET + 600)) --features practice_point,rewind --press $SET:2:0x1000 --press $((SET + 300)):2:0x800 &
        run rewind $((REWIND + 140 + HOLD)) --features practice_point,rewind --press $REWIND:$HOLD:0x400 &
        wait
        run both $((SET + 900)) --features practice_point,rewind,run_ahead=2 --press $SET:2:0x1000 --press $((SET + 200)):40:0x400 --press $((SET + 500)):2:0x800 &
        rewound=$(sed -n 's/^state: frame \([0-9]*\).*/\1/p' $out/rewind.txt)
        run after_rewind ${rewound:-1} &
        wait
        both=$(sed -n 's/^state: frame \([0-9]*\).*/\1/p' $out/both.txt)
        run after_point $((SET + 300)) &
        run after_both ${both:-1} --features run_ahead=2 &
        wait
        same "practice point" point after_point
        same "rewind (to frame $rewound)" rewind after_rewind
        # (the board stood still while it was held: it ran REWIND + 140 frames of its own, less those gone back)
        back=$((REWIND + 140 - ${rewound:-0})) want=$(((HOLD / 2 - 2) * 6))
        if [ $back -ge $want ]; then
            echo "$g$MODE: rewind held $HOLD frames: ok (back $back frames, at least $want)"
        else
            echo "$g$MODE: rewind held $HOLD frames: FAIL: back $back frames, not $want"; fail=1
        fi
        same "both, with run-ahead (to frame $both)" both after_both
    done
done
exit $fail

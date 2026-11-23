#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Sound lag, measured (docs/LAG.md's Sound): each game is run for real, paced as it is played (--real-time), with its
# sound handed to the sound device as in play but not heard (SPELUNK_AUDIO_LOG sets the stream's volume to 0), and the
# sound already waiting when each frame's is handed over is logged once a second: that, and the device's own buffer, is
# how long after the game makes a sound it reaches the system's sound output. Prints a line a game: the device's
# buffer, what waits (on average, the least and the most), the frames whose sound was left out (the game catching up)
# and the times the sound ran dry (a gap heard).
#   tools/audio_lag.sh ROMS [GAME...]   (one game a board unless named: mushisam ddonpach ket; FRAMES=3600, B=build)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/audio_lag.sh ROMS [GAME...]}
shift
GAMES=${*:-mushisam ddonpach ket}
FRAMES=${FRAMES:-3600}
B=${B:-build}
OUT=work/audio_lag
mkdir -p $OUT
run() { # game
    SPELUNK_AUDIO_LOG=1 SPELUNK_HIDDEN_WINDOW=1 ./$B/$1/$1 --roms "$ROMS" --real-time --frames $FRAMES --features boot=1 2> $OUT/$1.err || true
}
# (two at a time: each opens a window, hidden, and the machine's rule is at most two of those at once)
set -- $GAMES
while [ $# -gt 0 ]; do
    run $1 &
    [ $# -gt 1 ] && run $2
    wait
    shift; [ $# -gt 0 ] && shift
done
for g in $GAMES; do
    # (the first ten seconds left out: the device starting, and the board's own start)
    awk -v game=$g '
        /^audio: device/ { buffer = $9; sub(/\(/, "", buffer) }
        /^audio: frame/ { gsub(/,/, ""); f = $3; if (f <= 600) next
            n++; sum += $5; least = (n == 1 || $8 < least) ? $8 : least; m = $10; sub(/\)/, "", m); most = (m + 0 > most) ? m + 0 : most
            out += $11; dry += $14 }
        END { if (!n) { print game ": no sound measured (" ENVIRON["PWD"] "/'$OUT'/" game ".err)"; exit 1 }
              printf "%-9s device buffer %s ms; waiting %.1f ms on average (least %.1f, most %.1f): %.1f ms from the game to the system, %d frames left out, ran dry %d times\n",
                     game, buffer, sum / n, least, most, sum / n + buffer, out, dry }' $OUT/$g.err
done

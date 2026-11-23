#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Every check, for every game with a build (or those named): the translation against the interpreter, with the
# board's slowdown off and on (tools/check.sh); the GPU's picture against the software blitter's
# (tools/check_video.sh); the training tools' exactness (tools/check_tools.sh). Run before a commit that touches the
# translator, the machine or the port's frame loop. About 15 minutes a game; the last two open a hidden window each,
# so nothing else that draws should be running.
#   tools/gate.sh ROMS [GAME...]
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/gate.sh ROMS [GAME...]}
shift
GAMES=${*:-$(for cfg in games/*/game.cfg; do basename "$(dirname "$cfg")"; done)}
fail=0
CV1000=""
for g in $GAMES; do
    [ -x build/$g/$g ] || { echo "$g: not built"; continue; }
    if grep -q '^hardware = cave68k' games/$g/game.cfg; then
        GAME=$g tools/check68k.sh "$ROMS" 7000 | tail -2 | head -1 || fail=1
        continue
    fi
    CV1000="$CV1000 $g"
    GAME=$g tools/check.sh "$ROMS" 6500 | tail -1 || fail=1
    GAME=$g TIMING=1 tools/check.sh "$ROMS" 6500 | tail -1 || fail=1
done
# (the GPU against the software blitter: the CV1000 games)
tools/check_video.sh "$ROMS" 7000 $CV1000 || fail=1
tools/check_tools.sh "$ROMS" $GAMES || fail=1
[ $fail = 0 ] && echo "gate: ok" || echo "gate: FAILED"
exit $fail

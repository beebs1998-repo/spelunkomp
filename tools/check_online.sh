#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Online play's check (port/netplay.h, docs/ONLINE.md): two copies of a game on this machine, one hosting and one
# joining it by address, play a co-op game with made-up controls (each presses Start now and then, then moves about at
# random with the shot held), and the two must stay the same: each writes the hash of the game at every second's
# confirmed frame, and the two lists must be alike, line for line. With FAKE, every packet is sent late (LAG ms, give or
# take JITTER, and LOSS in a hundred lost), so the guesses are often wrong and rollback is tried.
#   tools/check_online.sh ROMS [GAME...]
#   FRAMES=6000  the match's frames      FAKE=LAG:JITTER:LOSS (none unless said)
#   START=1500   the frame Start is first pressed (the board has started by then)
#   DELAY=2      the host's input delay   SEED=1  the made-up play's
#   EXTRA='...'  more options for both copies (--shader FILE, say)
#   HOSTMENU=1   the host hosts as the in-game menu's Host a match does (at frame 60, playing alone until the other
#                copy joins; the game then made anew for the match) instead of --host
#   LOG=1        every confirmed frame's controls and hash, each side's, into work/GAME/online/*-controls.txt (where two
#                games part: the first line that differs)
# Two game windows open (hidden); each pair is run under the GPU lock other checks take.
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check_online.sh ROMS [GAME...]}
shift
GAMES=${*:-mushisam ddonpach ddpdoj}
FRAMES=${FRAMES:-6000}
START=${START:-1500}
DELAY=${DELAY:-2}
SEED=${SEED:-1}
PORT=${PORT:-7911}
# The made-up play of one side: SEED START UNTIL
play() {
    awk -v seed="$1" -v start="$2" -v until="$3" 'BEGIN {
        srand(seed)
        for (f = start; f < until; f += 200) printf "--system %d:5:0x10 ", f
        split("0 1 2 4 8 5 6 9 10", dirs, " ")
        for (f = start + 400; f < until; f += n) {
            n = 3 + int(rand() * 23)
            bits = dirs[1 + int(rand() * 9)] + (rand() < 0.8 ? 16 : 0) + (rand() < 0.05 ? 32 : 0)
            printf "--press %d:%d:0x%x ", f, n, bits
        }
    }'
}
fail=0
for g in $GAMES; do
    [ -x build/$g/$g ] || { echo "$g: not built"; fail=1; continue; }
    set_=$ROMS
    [ -f "$ROMS/$g.zip" ] && set_=$ROMS/$g.zip
    out=work/$g/online
    mkdir -p $out
    rm -f $out/host.txt $out/join.txt
    fake=${FAKE:+--net-fake $FAKE}
    a=$(play $SEED $START $FRAMES)
    b=$(play $((SEED + 1000)) $((START + 300)) $FRAMES)
    log1=${LOG:+--net-log $out/host-controls.txt}; log2=${LOG:+--net-log $out/join-controls.txt}
    hosting="--host $PORT"; wait=1
    [ -n "$HOSTMENU" ] && hosting="--net-menu-host 60 --net-port $PORT" && wait=3
    lockf -k /tmp/spelunkomp-gpu.lock sh -c "
        SPELUNK_HIDDEN_WINDOW=1 ./build/$g/$g --roms '$set_' --mute --frames $FRAMES $hosting --net-service off --net-delay $DELAY \
            --net-name HOST $fake $log1 $EXTRA --net-hashes $out/host.txt $a > $out/host.log 2>&1 &
        sleep $wait
        SPELUNK_HIDDEN_WINDOW=1 ./build/$g/$g --roms '$set_' --mute --frames $FRAMES --join 127.0.0.1:$PORT --net-service off \
            --net-name GUEST $fake $log2 $EXTRA --net-hashes $out/join.txt $b > $out/join.log 2>&1
        wait"
    lines=$(wc -l < $out/host.txt | tr -d ' ')
    stats=$(grep '^online: frame' $out/join.log | tail -1 | sed 's/^online: //')
    if [ "$lines" -ge $((FRAMES / 60 - 5)) ] && cmp -s $out/host.txt $out/join.txt; then
        echo "$g: ok: $lines hashes alike ($stats)"
    else
        echo "$g: FAIL: $lines hashes; $(cmp $out/host.txt $out/join.txt 2>&1 | head -1); see $out/*.log"
        fail=1
    fi
done
exit $fail

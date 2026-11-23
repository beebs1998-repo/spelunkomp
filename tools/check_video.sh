#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The native renderer's check (docs/NATIVE.md): each game run with the GPU and the software model side by side, the
# screen compared pixel for pixel every frame (and, on a CV1000, all of VRAM at the end). One game at a time (each opens
# a window, hidden).
#   tools/check_video.sh ROMS [FRAMES] [GAME...]   (every game with a build in build/<id> by default)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check_video.sh ROMS [FRAMES] [GAME...]}
FRAMES=${2:-7000}
shift; [ $# -gt 0 ] && shift
GAMES=${*:-$(for cfg in games/*/game.cfg; do basename "$(dirname "$cfg")"; done)}
# Into play with free play: Start, button 1 through the selects, then fire held and the stick moved
PLAY="--system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10 --press 4900:2100:0x11 --press 5300:200:0x04 --press 5800:200:0x08"
fail=0
for g in $GAMES; do
    [ -x build/$g/$g ] || { echo "$g: not built"; continue; }
    out=$(SPELUNK_HIDDEN_WINDOW=1 ./build/$g/$g --roms "$ROMS" --mute --check-video --frames $FRAMES $PLAY 2>&1 | grep '^check-video' | tail -1)
    echo "$g: $out"
    case "$out" in "check-video: 0 of "*"VRAM at the end: 0 pixels"* | "check-video: 0 of "*"most sprites"*) ;; *) fail=1 ;; esac
done
exit $fail

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's runner on macOS (tools/release/memcheck.sh starts it): each game's program in the app, started
# from this session (it needs the logged-in user's desktop), its RSS read every 2 s with ps (no VRAM: the GPU's memory
# is the Mac's own).
#   mac.sh APP ROMS WORK      (APP: Spelunkomp.app as unpacked from Spelunkomp-macOS.zip; ROMS: the folder of the
#                              ROM sets, <id>.zip; WORK: a folder for each run's settings, saves, log and pictures)
set -u
. "$(dirname "$0")/play.sh"
[ $# -eq 3 ] || { echo "usage: mac.sh APP ROMS WORK" >&2; exit 2; }
DIR="$1/Contents/MacOS"
[ -d "$DIR" ] || { echo "error: $DIR is not there: APP is Spelunkomp.app" >&2; exit 2; }
mkdir -p "$3"

launch() {
    local name=$1
    shift
    "$@" > "$WORK/$name/out.txt" 2>&1 &
    PID=$!
}
# (bash reaps its own background job as it ends: kill -0 fails then)
alive() { kill -0 "$PID" 2>/dev/null; }
mem_sample() { ps -o rss= -p "$PID" 2>/dev/null | tr -d ' '; } # (no VRAM of its own: the Mac's memory is one)
ask_stop() { kill -TERM "$PID" 2>/dev/null; }
force_stop() { kill -KILL "$PID" 2>/dev/null; }
finish() { wait "$PID" 2>/dev/null; CODE=$?; }

WORK="$3"
echo "memory RSS (ps)"
memcheck_all "$DIR" "$2" "$WORK"

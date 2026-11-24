# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's play and its sampling, for the runners of macOS and Linux (mac.sh, linux.sh source this; win.ps1
# does the same in PowerShell, and must be kept alike). A runner defines:
#   launch NAME PROGRAM ARGS...   starts the program, sets PID (and may set UNIT)
#   alive                         true while the program runs
#   mem_kb                        its memory in KB now (RSS)
#   finish                        waits for it and sets CODE (its exit status), cleans up
# then calls memcheck_all DIR ROMS WORK, which prints, for tools/release/memcheck.sh to judge:
#   memory <what is measured>
#   case NAME                     sample NAME SECONDS MB (every 2 s)          end NAME exit:N | killed:MB | timeout
# (bash 3.2: the Mac's own)

MEMCHECK_GAMES="${MEMCHECK_GAMES:-mmpork:1260 ddpsdoj:1920 ddonpach:2940 ket:1320}" # (ID:TITLE, below)
MEMCHECK_SECONDS="${MEMCHECK_SECONDS:-120}"
MEMCHECK_REWIND="${MEMCHECK_REWIND:-ddpsdoj}" # (the game also run with rewind on; none: no such run)
MEMCHECK_KILL_MB="${MEMCHECK_KILL_MB:-2048}"  # (a run is stopped here: it has failed, and the machine is spared)

# The play, as the cabinet's controls (machine/inputs.h, every board alike). A program goes straight to the title, its
# board's frames to game.cfg's title_frame (TITLE) run unseen as fast as the machine goes; the time is the board's from
# there (--real-time: a check's --frames otherwise runs as fast as the machine goes), so the run is TITLE and SECONDS
# of frames. Free play on, and also a coin (0x04, system) and Start (0x10, system) three times over at 1, 15 and 30 s
# after the title, then a new credit every 50 s; from 1 s on the shot (0x10) held 85 frames of every 90 and the stick
# left (0x04) and right (0x08) by turns, 90 frames each.
play_args() { # $1 the seconds, $2 the title's frame; sets PLAY
    local t=$2 frames=$(($2 + $1 * 60)) c i
    PLAY=(--frames "$frames" --real-time --mute --window-shot "$((t + $1 * 30)):$T/middle.png" --window-shot "$((frames - 120)):$T/late.png")
    credit() { PLAY+=(--system "$1:5:0x04" --system "$(($1 + 100)):5:0x10" --system "$(($1 + 200)):5:0x10" --system "$(($1 + 300)):5:0x10"); }
    credit $((t + 60))
    credit $((t + 900))
    for ((c = t + 1800; c < frames; c += 3000)); do credit "$c"; done
    for ((c = t + 60, i = 0; c < frames; c += 90, i++)); do
        PLAY+=(--press "$c:85:0x10" --press "$c:90:$((i % 2 ? 0x08 : 0x04))")
    done
}

# One run: NAME ID TITLE FEATURES DIR ROMS WORK (DIR: the folder holding the game programs)
run_case() {
    local name=$1 id=$2 title=$3 features=$4 dir=$5 roms=$6 work=$7 t0 s kb mb status=""
    T="$work/$name"
    rm -rf "$T" && mkdir -p "$T/saves"
    play_args "$MEMCHECK_SECONDS" "$title"
    echo "case $name"
    launch "$name" "$dir/$id" --roms "$roms/$id.zip" --settings "$T/settings.ini" --saves "$T/saves" \
        --features "$features" "${PLAY[@]}"
    t0=$(date +%s)
    while alive; do
        kb=$(mem_kb)
        s=$(($(date +%s) - t0))
        if [ "${kb:-0}" -gt 0 ]; then # (none, or 0: it is ending)
            mb=$((kb / 1024))
            echo "sample $name $s $mb"
            if [ "$mb" -gt "$MEMCHECK_KILL_MB" ]; then status="killed:$mb"; kill -9 "$PID" 2>/dev/null; break; fi
        fi
        if [ "$s" -gt $((MEMCHECK_SECONDS * 2 + 60)) ]; then status=timeout; kill -9 "$PID" 2>/dev/null; break; fi
        sleep 2
    done
    finish
    echo "end $name ${status:-exit:$CODE}"
    [ -z "$status" ] && [ "$CODE" = 0 ] || tail -5 "$T/out.txt" 2>/dev/null | sed "s/^/log $name /"
}

memcheck_all() { # DIR ROMS WORK
    local game id title rewound=""
    for game in $MEMCHECK_GAMES; do
        id=${game%%:*} title=${game#*:}
        [ "$title" != "$game" ] || title=0
        [ -x "$1/$id" ] || { echo "error: no program $1/$id" >&2; echo "case $id"; echo "end $id missing"; continue; }
        run_case "$id" "$id" "$title" free_play=1 "$1" "$2" "$3"
        [ "$id" != "$MEMCHECK_REWIND" ] || rewound=$title
    done
    if [ -n "$rewound" ]; then
        run_case "$MEMCHECK_REWIND+rewind" "$MEMCHECK_REWIND" "$rewound" free_play=1,rewind=1 "$1" "$2" "$3"
    fi
}

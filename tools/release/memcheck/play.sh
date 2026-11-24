# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's play and its sampling, for the runners of macOS and Linux (mac.sh, linux.sh source this; win.ps1
# does the same in PowerShell, and must be kept alike). A runner defines:
#   launch NAME PROGRAM ARGS...   starts the program, sets PID (0 if it could not)
#   alive                         true while the program runs (not once it has ended, even if not yet reaped)
#   mem_sample                    prints its memory now in KB: "RSS [VRAM]" (nothing if it could not be read); never
#                                 waits long, whatever the program or its driver is doing
#   ask_stop / force_stop         asks it to quit (SIGTERM: SDL's quit event, the game's own ending) / kills it
#   ended                         (may) once alive has said no: 0 if it has truly ended (CODE set if it knows it), or
#                                 1 if it is still running after all (the sampling goes on: alive was wrong)
#   finish                        sets CODE (its exit status) once it has ended, and cleans up
# then calls memcheck_all DIR ROMS WORK, which prints, for tools/release/memcheck.sh to judge:
#   memory <what RSS is>          vram <what VRAM is> (when there is one)
#   case NAME   sample NAME SECONDS MB [VRAM_MB] (every 2 s)   end NAME STATUS   (error: ... and log NAME ... between)
# STATUS: exit:N (the program ended by itself, after its frames: the run as it should be), or why it was stopped:
# killed:MB, killed-vram:MB, no-samples, no-progress:PICTURE@SECONDS, timeout; with +stuck if it could not be stopped.
# (bash 3.2: the Mac's own)

MEMCHECK_GAMES="${MEMCHECK_GAMES:-mmpork:1260 ddpsdoj:1920 ddonpach:2940 ket:1320}" # (ID:TITLE, below)
MEMCHECK_SECONDS="${MEMCHECK_SECONDS:-120}"
MEMCHECK_REWIND="${MEMCHECK_REWIND:-ddpsdoj}" # (the game also run with rewind on; none: no such run)
MEMCHECK_KILL_MB="${MEMCHECK_KILL_MB:-2048}"  # (a run is stopped here: it has failed, and the machine is spared)
MEMCHECK_KILL_VRAM_MB="${MEMCHECK_KILL_VRAM_MB:-3072}"

# The play, as the cabinet's controls (machine/inputs.h, every board alike). A program goes straight to the title, its
# board's frames to game.cfg's title_frame (TITLE) run unseen as fast as the machine goes; the time is the board's from
# there (--real-time: a check's --frames otherwise runs as fast as the machine goes), so the run is TITLE and SECONDS
# of frames, and ends by itself. Free play on, and also a coin (0x04, system) and Start (0x10, system) three times over
# at 1, 15 and 30 s after the title, then a new credit every 50 s; from 1 s on the shot (0x10) held 85 frames of every
# 90 and the stick left (0x04) and right (0x08) by turns, 90 frames each. Its pictures (--window-shot) are the run's
# progress: start.png 5 s after the title, middle.png halfway, late.png 2 s before the end.
play_args() { # $1 the seconds, $2 the title's frame; sets PLAY
    local t=$2 frames=$(($2 + $1 * 60)) c i
    PLAY=(--frames "$frames" --real-time --mute --window-shot "$((t + 300)):$T/start.png"
        --window-shot "$((t + $1 * 30)):$T/middle.png" --window-shot "$((frames - 120)):$T/late.png")
    credit() { PLAY+=(--system "$1:5:0x04" --system "$(($1 + 100)):5:0x10" --system "$(($1 + 200)):5:0x10" --system "$(($1 + 300)):5:0x10"); }
    credit $((t + 60))
    credit $((t + 900))
    for ((c = t + 1800; c < frames; c += 3000)); do credit "$c"; done
    for ((c = t + 60, i = 0; c < frames; c += 90, i++)); do
        PLAY+=(--press "$c:85:0x10" --press "$c:90:$((i % 2 ? 0x08 : 0x04))")
    done
}

# Stops the program: asked first, and given 10 s to end (as it does at its last frame, its files closed and its GPU's
# work finished); killed only then, and given 20 s more. Fails if it has not ended even so (a process stuck in its
# GPU driver cannot be killed: nothing more should be run on that machine before it is restarted)
stop_gently() {
    local k
    ask_stop
    for ((k = 0; k < 20; k++)); do alive || return 0; sleep 0.5; done
    echo "error: $1 did not quit within 10 s of being asked: killed"
    force_stop
    for ((k = 0; k < 40; k++)); do alive || return 0; sleep 0.5; done
    return 1
}

# One run: NAME ID TITLE FEATURES DIR ROMS WORK (DIR: the folder holding the game programs). Its picture at 5 s must be
# there by 40 s, and the one halfway by three quarters of the run and 30 s (a slow machine's board pace is not quite
# the board's), or the run is stopped: the game is not getting on (as when its GPU's memory is gone)
run_case() {
    local name=$1 id=$2 title=$3 features=$4 dir=$5 roms=$6 work=$7 t0 now s got=0 mem kb vkb mb vmb status=""
    T="$work/$name"
    rm -rf "$T" && mkdir -p "$T/saves"
    play_args "$MEMCHECK_SECONDS" "$title"
    echo "case $name"
    launch "$name" "$dir/$id" --roms "$roms/$id.zip" --settings "$T/settings.ini" --saves "$T/saves" \
        --features "$features" "${PLAY[@]}"
    [ "${PID:-0}" != 0 ] || { echo "end $name no-start"; return 0; }
    t0=$(date +%s)
    got=$t0
    local resumed=0
    while :; do
        while alive; do
            now=$(date +%s)
            s=$((now - t0))
            mem=$(mem_sample)
            kb=${mem%% *} vkb=""
            [ "$mem" = "${mem#* }" ] || vkb=${mem#* }
            if [ "${kb:-0}" -gt 0 ] 2>/dev/null; then # (none, or 0: it is ending)
                mb=$((kb / 1024)) vmb=""
                [ -z "$vkb" ] || vmb=$((vkb / 1024))
                echo "sample $name $s $mb${vmb:+ $vmb}"
                got=$now
                if [ "$mb" -gt "$MEMCHECK_KILL_MB" ]; then status="killed:$mb"; fi
                if [ -n "$vmb" ] && [ "$vmb" -gt "$MEMCHECK_KILL_VRAM_MB" ]; then status="killed-vram:$vmb"; fi
            elif [ $((now - got)) -ge 10 ]; then
                echo "error: no sample within 10 s ($name, pid $PID: $(sample_why))"
                status=no-samples
            fi
            if [ -z "$status" ] && [ "$s" -gt 40 ] && [ ! -e "$T/start.png" ]; then status="no-progress:start.png@${s}s"; fi
            if [ -z "$status" ] && [ "$s" -gt $((MEMCHECK_SECONDS * 3 / 4 + 30)) ] && [ ! -e "$T/middle.png" ]; then
                status="no-progress:middle.png@${s}s"
            fi
            if [ -z "$status" ] && [ "$s" -gt $((MEMCHECK_SECONDS * 2 + 60)) ]; then status=timeout; fi
            [ -z "$status" ] || break
            sleep 2
        done
        [ -z "$status" ] || break
        ended && break
        # (still running after all: sampled on, three times at the most)
        resumed=$((resumed + 1))
        [ "$resumed" -lt 3 ] || { echo "error: $name: the liveness check kept disagreeing: stopped"; status=liveness; break; }
    done
    if [ -n "$status" ] && ! stop_gently "$name"; then
        echo "error: $name could not be stopped, even killed (stuck in its GPU driver?): no more runs on this machine;" \
            "it may need restarting"
        echo "end $name $status+stuck"
        STUCK=1
        return 0
    fi
    finish
    [ "$CODE" != stuck ] || status=stuck
    echo "end $name ${status:-exit:$CODE}"
    [ -z "$status" ] && [ "$CODE" = 0 ] || tail -5 "$T/out.txt" 2>/dev/null | sed "s/^/log $name /"
}
sample_why() { echo "no reading"; } # (a runner may say more)
ended() { return 0; }                # (a runner may look again)

memcheck_all() { # DIR ROMS WORK
    local game id title rewound=""
    STUCK=0
    for game in $MEMCHECK_GAMES; do
        id=${game%%:*} title=${game#*:}
        [ "$title" != "$game" ] || title=0
        [ -x "$1/$id" ] || { echo "error: no program $1/$id" >&2; echo "case $id"; echo "end $id missing"; continue; }
        run_case "$id" "$id" "$title" free_play=1 "$1" "$2" "$3"
        [ "$STUCK" = 0 ] || return 0
        [ "$id" != "$MEMCHECK_REWIND" ] || rewound=$title
    done
    if [ -n "$rewound" ]; then
        run_case "$MEMCHECK_REWIND+rewind" "$MEMCHECK_REWIND" "$rewound" free_play=1,rewind=1 "$1" "$2" "$3"
    fi
}

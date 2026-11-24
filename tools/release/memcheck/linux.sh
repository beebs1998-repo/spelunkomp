#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's runner on Linux (tools/release/memcheck.sh copies it and play.sh over and starts it by ssh): each
# game's program started by systemd-run --user in the desktop's session (gamescope's on a Steam Machine or Deck), with
# that session's display and bus, taken from a process of it (MEMCHECK_ENV_FROM, steam by default); its RSS read from
# /proc every 2 s, and its VRAM from its DRM fdinfo (below). Stopped with SIGTERM first (SDL's quit event).
#   linux.sh DIR ROMS WORK    (DIR: the Spelunkomp folder as unpacked from Spelunkomp-linux-x86_64.tar.gz, or the
#                              folder it was unpacked in; ROMS: the folder of the ROM sets, <id>.zip; WORK: a folder for
#                              each run's settings, saves, log and pictures)
set -u
. "$(dirname "$0")/play.sh"
[ $# -eq 3 ] || { echo "usage: linux.sh DIR ROMS WORK" >&2; exit 2; }
DIR="$1"
[ -x "$DIR/spelunkomp-launcher" ] || [ ! -x "$DIR/Spelunkomp/spelunkomp-launcher" ] || DIR="$DIR/Spelunkomp"
[ -x "$DIR/spelunkomp-launcher" ] || { echo "error: no spelunkomp-launcher in $1 (DIR: the unpacked Spelunkomp folder)" >&2; exit 2; }
DIR="$(cd "$DIR" && pwd)"
ROMS="$(cd "$2" && pwd)" || exit 2
mkdir -p "$3" && WORK="$(cd "$3" && pwd)"

# The desktop session's environment, from one of its processes
FROM="${MEMCHECK_ENV_FROM:-steam}"
P=$(pgrep -u "$(id -u)" -x "$FROM" | head -1)
[ -n "$P" ] || { echo "error: no $FROM process of this user: is the desktop (gamescope) session running? (MEMCHECK_ENV_FROM)" >&2; exit 2; }
SETENV=()
while IFS= read -r line; do
    case "$line" in
        DISPLAY=* | WAYLAND_DISPLAY=* | XDG_RUNTIME_DIR=* | XDG_SESSION_TYPE=* | DBUS_SESSION_BUS_ADDRESS=*)
            SETENV+=("--setenv=$line")
            export "${line?}" ;;
    esac
done < <(tr '\0' '\n' < "/proc/$P/environ")

# The program is started through sh, which writes its own PID and then becomes the program (exec): the PID is the
# program's from its first instruction, not looked up from the unit afterwards
launch() {
    local name=$1 k
    shift
    # (a unit of its own each time: one left by a run before, its program held in the driver, is never waited on)
    UNIT="spelunkomp-memcheck-${name//[^a-zA-Z0-9]/-}-$(date +%s)"
    PIDFILE="$WORK/$name/pid"
    rm -f "$PIDFILE"
    VRAM_BASE=$(vram_card) # (the card's count before the run: the desktop's, taken off)
    VRAM_BASE=${VRAM_BASE:-0}
    PID=0
    systemd-run --user --quiet --unit="$UNIT" -p RemainAfterExit=yes --working-directory="$DIR" "${SETENV[@]}" \
        -p StandardOutput="file:$WORK/$name/out.txt" -p StandardError="file:$WORK/$name/out.txt" \
        -- /bin/sh -c 'echo $$ > "$0.new" && mv "$0.new" "$0" && exec "$@"' "$PIDFILE" "$@" ||
        { echo "error: systemd-run failed for $name"; return; }
    for ((k = 0; k < 50; k++)); do
        [ -s "$PIDFILE" ] && PID=$(cat "$PIDFILE") && break
        sleep 0.2
    done
    [ "${PID:-0}" != 0 ] || echo "error: $name's PID never came (systemctl --user status $UNIT)"
}
# (alive until it has ended: a zombie, Z, has, though a thread of it may still be held in the kernel)
state() { [ "${PID:-0}" != 0 ] && timeout -s KILL 3 awk '{ sub(/^.*\) /, ""); print $1 }' "/proc/$PID/stat" 2>/dev/null; }
alive() { local st; st=$(state) && [ -n "$st" ] && [ "$st" != Z ] && [ "$st" != X ]; }
ask_stop() { kill -TERM "$PID" 2>/dev/null; }
force_stop() { kill -KILL "$PID" 2>/dev/null; }
sample_why() {
    echo "state $(state || echo gone), status $(timeout -s KILL 3 grep -c . "/proc/$PID/status" 2>/dev/null || echo unreadable) lines"
}

# The GPU's memory. Per process: the sum, over the program's DRM file descriptors, of their fdinfo's VRAM lines
# (drm-memory-vram, or drm-resident-vram, or drm-total-vram: the kernel's own names, by version), one count for each
# drm-client-id. Where no fd says (an older kernel), the card's own count, mem_info_vram_used: the whole machine's,
# the desktop's too, so less what it was before the program started. A read of the driver's can wait on its locks
# (the run of beta.1 left amdgpu deadlocked), so it is done aside and waited for 3 s at the most.
VRAM_KIND=""
vram_proc() { # the program's VRAM in KB, or nothing
    awk '
        FNR == 1 { id = FILENAME }
        $1 == "drm-client-id:" { id = "c" $2 }
        $1 ~ /^drm-(memory|resident|total)-vram:$/ {
            v = $2; u = $3
            if (u == "KiB") v *= 1; else if (u == "MiB") v *= 1024; else if (u == "GiB") v *= 1048576; else v /= 1024
            kind = $1; sub(/:$/, "", kind)
            if (!(kind in pri)) pri[kind] = kind ~ /memory/ ? 3 : kind ~ /resident/ ? 2 : 1
            val[id, kind] = v; ids[id] = 1; kinds[kind] = 1
        }
        END {
            for (i in ids) {
                best = ""; for (k in kinds) if ((i, k) in val && (best == "" || pri[k] > pri[best])) best = k
                t += val[i, best]; any = 1
            }
            if (any) printf "%d\n", t
        }' "/proc/$PID/fdinfo/"* 2>/dev/null
}
vram_card() { # the card's VRAM in use, in KB (the biggest card's: the one that plays)
    local f b best=""
    for f in /sys/class/drm/card*/device/mem_info_vram_used; do
        [ -r "$f" ] || continue
        b=$(timeout -s KILL 3 cat "$f" 2>/dev/null) || continue
        [ -z "$best" ] || [ "$b" -gt "$best" ] && best=$b
    done
    [ -z "$best" ] || echo $((best / 1024))
}
# (mem_sample runs in a command substitution, a subshell: what it keeps between samples is kept in files)
mem_sample() {
    local rss v k job
    rss=$(timeout -s KILL 3 awk '/^VmRSS:/ {print $2}' "/proc/$PID/status" 2>/dev/null)
    [ -n "$rss" ] || return 0
    job=$(cat "$WORK/vram.job" 2>/dev/null)
    # (the reading before is still waiting: the driver is held; no other is started)
    if [ -z "$job" ] || ! kill -0 "$job" 2>/dev/null; then
        rm -f "$WORK/vram.out"
        { # (written even empty, none to be had: not waited for again)
            v=$(vram_proc)
            if [ -n "$v" ]; then echo "proc $v"; else v=$(vram_card) && [ -n "$v" ] && echo "card $v"; fi
            true
        } > "$WORK/vram.new" 2>/dev/null && mv "$WORK/vram.new" "$WORK/vram.out" &
        echo $! > "$WORK/vram.job"
        for ((k = 0; k < 15; k++)); do [ -e "$WORK/vram.out" ] && break; sleep 0.2; done
    fi
    v=$(cat "$WORK/vram.out" 2>/dev/null)
    case "$v" in
        "proc "*) v=${v#proc } kind="the program's (DRM fdinfo)" ;;
        "card "*) v=$((${v#card } - VRAM_BASE)) kind="the card's, less what it was before the run (mem_info_vram_used)"
            [ "$v" -gt 0 ] || v=0 ;;
        *) v="" ;;
    esac
    [ -z "$v" ] || [ -e "$WORK/vram.kind" ] || { echo "$kind" > "$WORK/vram.kind"; echo "vram $kind" >&3; }
    echo "$rss${v:+ $v}"
}
# (ended: systemd has its exit status once it has reaped it, MainPID 0. A zombie never reaped, a thread of it held in
# the driver, is stuck: no more runs)
finish() {
    local k main
    CODE=stuck
    for ((k = 0; k < 40; k++)); do
        main=$(timeout 5 systemctl --user show -p MainPID --value "$UNIT" 2>/dev/null)
        if [ "$main" = 0 ]; then
            CODE=$(timeout 5 systemctl --user show -p ExecMainStatus --value "$UNIT" 2>/dev/null)
            CODE=${CODE:-unknown}
            break
        fi
        sleep 0.5
    done
    if [ "$CODE" = stuck ]; then
        echo "error: $UNIT's program ended but was never reaped (held in its GPU driver?): no more runs on this machine;" \
            "it may need restarting"
        STUCK=1
    fi
    timeout 10 systemctl --user --no-block stop "$UNIT" 2>/dev/null
    timeout 10 systemctl --user reset-failed "$UNIT" 2>/dev/null
}

# (a game's program still there from a run before: held in the GPU driver, as beta.1's leak left one. Nothing run now
# would tell anything)
if left=$(pgrep -u "$(id -u)" -f "^$DIR/" | tr '\n' ' ') && [ -n "$left" ]; then
    echo "error: a game's program from before is still there (pid $left): stop it, or restart this machine if it cannot be" >&2
    exit 2
fi
exec 3>&1 # (the vram line, from inside mem_sample's subshell)
rm -f "$WORK/vram.kind" "$WORK/vram.job"
echo "memory RSS (/proc/PID/status VmRSS)"
memcheck_all "$DIR" "$ROMS" "$WORK"

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's runner on Linux (tools/release/memcheck.sh copies it and play.sh over and starts it by ssh): each
# game's program started by systemd-run --user in the desktop's session (gamescope's on a Steam Machine or Deck), with
# that session's display and bus, taken from a process of it (MEMCHECK_ENV_FROM, steam by default); its RSS read from
# /proc every 2 s.
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

launch() {
    local name=$1
    shift
    UNIT="spelunkomp-memcheck-${name//[^a-zA-Z0-9]/-}"
    systemctl --user stop "$UNIT" 2>/dev/null
    systemctl --user reset-failed "$UNIT" 2>/dev/null
    systemd-run --user --quiet --unit="$UNIT" -p RemainAfterExit=yes --working-directory="$DIR" "${SETENV[@]}" \
        -p StandardOutput="file:$WORK/$name/out.txt" -p StandardError="file:$WORK/$name/out.txt" -- "$@" ||
        { echo "error: systemd-run failed" >&2; PID=0; return; }
    PID=0
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        PID=$(systemctl --user show -p MainPID --value "$UNIT")
        [ "${PID:-0}" != 0 ] && break
        sleep 0.2
    done
}
alive() { [ "${PID:-0}" != 0 ] && kill -0 "$PID" 2>/dev/null; }
mem_kb() { awk '/^VmRSS:/ {print $2}' "/proc/$PID/status" 2>/dev/null; }
finish() {
    while alive; do sleep 0.5; done
    CODE=$(systemctl --user show -p ExecMainStatus --value "$UNIT" 2>/dev/null)
    CODE=${CODE:-unknown}
    systemctl --user stop "$UNIT" 2>/dev/null
    systemctl --user reset-failed "$UNIT" 2>/dev/null
}

echo "memory RSS (/proc/PID/status VmRSS)"
memcheck_all "$DIR" "$ROMS" "$WORK"

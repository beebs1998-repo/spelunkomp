#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The release's memory check (docs/RELEASING.md): the downloads, as unpacked on Windows, Linux and macOS, each play one
# game of every way the picture is made, for 2 minutes each (MEMCHECK_SECONDS), with the scripted play of
# memcheck/play.sh (coin, Start, free play, the shot held, the stick from side to side): mmpork (CV1000-B), ddpsdoj
# (CV1000-D), ddonpach (the Cave 68000 boards), ket (PGM), then ddpsdoj again with rewind on. Each program's memory is
# logged every 2 s: private bytes on Windows, RSS on Linux and macOS. The three systems run at once (about 10 minutes);
# Windows and Linux by ssh (the runners in memcheck/ are copied over first), macOS on this Mac.
#
# A run fails if its program's memory goes over 1 GB (MEMCHECK_LIMIT_MB), if it keeps climbing after its first 30 s,
# or if the program does not end by itself, with status 0, after its frames. Climbing: the most it had in its last
# 30 s is more than the most it had from 30 to 60 s by over 64 MB or a tenth, whichever is more (a game's memory goes
# up and down a little with its scenes; a leak keeps going). The rewind run is held to the 1 GB alone: rewind fills a
# minute of moments, so it climbs, by design, for its first minute.
#
#   tools/release/memcheck.sh [--dry-run | --judge] [win] [linux] [mac]   (none named: every one set below; --dry-run
#                                prints the commands, runs none; --judge judges the logs of the last run again)
#   Windows: MEMCHECK_WIN=user@host  MEMCHECK_WIN_DIR=the unpacked Spelunkomp-windows-x64.zip (its Spelunkomp folder,
#            or the folder holding it)  MEMCHECK_WIN_ROMS=the ROM folder (<id>.zip)  (Windows paths, as C:\...)
#            The user must be logged in at the desktop: the games are started by a scheduled task in that session.
#   Linux:   MEMCHECK_LINUX=user@host  MEMCHECK_LINUX_DIR=the unpacked Spelunkomp-linux-x86_64.tar.gz
#            MEMCHECK_LINUX_ROMS=the ROM folder  (a running desktop session: gamescope's on a Steam Machine or Deck;
#            MEMCHECK_ENV_FROM=a process of it, steam by default)
#   macOS:   MEMCHECK_MAC_APP=the unpacked Spelunkomp.app  MEMCHECK_MAC_ROMS=the ROM folder
#   MEMCHECK_OUT=build/memcheck (the logs: <system>.log, and the Mac's runs)  MEMCHECK_SSH_OPTS (more for ssh/scp)
#   MEMCHECK_GAMES, MEMCHECK_SECONDS=120, MEMCHECK_REWIND=ddpsdoj (none: no rewind run), MEMCHECK_LIMIT_MB=1024
# Exits non-zero if any run fails, or a system named cannot be run.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$ROOT/tools/release/memcheck"
OUT="${MEMCHECK_OUT:-$ROOT/build/memcheck}"
LIMIT="${MEMCHECK_LIMIT_MB:-1024}"
GAMES="${MEMCHECK_GAMES:-mmpork ddpsdoj ddonpach ket}"
# (the runners are given each game's title frame, game.cfg's, as ID:TITLE: memcheck/play.sh)
MEMCHECK_GAMES=$(for id in $GAMES; do
    [ -f "$ROOT/games/$id/game.cfg" ] || { echo "error: no game $id (games/$id/game.cfg)" >&2; exit 2; }
    printf '%s:%s ' "$id" "$(sed -n 's/^title_frame = //p' "$ROOT/games/$id/game.cfg" | head -1)"
done) || exit 2
export MEMCHECK_GAMES="${MEMCHECK_GAMES% }"
export MEMCHECK_SECONDS="${MEMCHECK_SECONDS:-120}"
export MEMCHECK_REWIND="${MEMCHECK_REWIND:-ddpsdoj}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=30 ${MEMCHECK_SSH_OPTS:-})
DRY=0
JUDGE=0
SYSTEMS=()
for a in "$@"; do
    case "$a" in
        --dry-run) DRY=1 ;;
        --judge) JUDGE=1 ;;
        win | linux | mac) SYSTEMS+=("$a") ;;
        *) sed -n '2,/^set -u/p' "$0" | sed '$d' | cut -c3- >&2; exit 2 ;;
    esac
done
if [ "$JUDGE" = 1 ] && [ ${#SYSTEMS[@]} -eq 0 ]; then
    for s in win linux mac; do [ ! -f "$OUT/$s.log" ] || SYSTEMS+=("$s"); done
    [ ${#SYSTEMS[@]} -gt 0 ] || { echo "error: no logs in $OUT" >&2; exit 2; }
elif [ ${#SYSTEMS[@]} -eq 0 ]; then
    [ -n "${MEMCHECK_WIN:-}" ] && SYSTEMS+=(win)
    [ -n "${MEMCHECK_LINUX:-}" ] && SYSTEMS+=(linux)
    [ -n "${MEMCHECK_MAC_APP:-}" ] && SYSTEMS+=(mac)
    [ ${#SYSTEMS[@]} -gt 0 ] || { echo "error: no system set (MEMCHECK_WIN, MEMCHECK_LINUX, MEMCHECK_MAC_APP)" >&2; exit 2; }
fi
need() { for v in "$@"; do [ -n "${!v:-}" ] || { echo "error: $v is not set" >&2; exit 2; }; done; }
for s in "${SYSTEMS[@]}"; do
    [ "$JUDGE" = 0 ] || break
    case "$s" in
        win) need MEMCHECK_WIN MEMCHECK_WIN_DIR MEMCHECK_WIN_ROMS ;;
        linux) need MEMCHECK_LINUX MEMCHECK_LINUX_DIR MEMCHECK_LINUX_ROMS ;;
        mac) need MEMCHECK_MAC_APP MEMCHECK_MAC_ROMS ;;
    esac
done

# (a command as it is run, or, with --dry-run, as it would be)
run() {
    if [ "$DRY" = 1 ]; then
        local a out=""
        for a in "$@"; do
            case "$a" in
                *[!A-Za-z0-9_./:@=,+-]* | "") out="$out '${a//\'/\'\\\'\'}'" ;;
                *) out="$out $a" ;;
            esac
        done
        echo "${out# }"
    else
        "$@"
    fi
}
# (a word for the remote shell, sh's; and for Windows', cmd's or PowerShell's: quoted, no \ before the closing quote)
q() { printf '%q' "$1"; }
wq() { local w=$1; while [ "${w%\\}" != "$w" ]; do w=${w%\\}; done; printf '"%s"' "$w"; }

# Each system's run; its lines (play.sh's) in $OUT/<system>.log
run_win() {
    local h=$MEMCHECK_WIN
    run scp -q "${SSH_OPTS[@]}" "$HERE/win.ps1" "$h:spelunkomp-memcheck.ps1" &&
        run ssh "${SSH_OPTS[@]}" "$h" "powershell -NoProfile -ExecutionPolicy Bypass -File spelunkomp-memcheck.ps1" \
            "-Dir $(wq "$MEMCHECK_WIN_DIR") -Roms $(wq "$MEMCHECK_WIN_ROMS") -Games $(wq "$MEMCHECK_GAMES")" \
            "-Seconds $MEMCHECK_SECONDS -Rewind $MEMCHECK_REWIND"
}
run_linux() {
    local h=$MEMCHECK_LINUX w=spelunkomp-memcheck
    run ssh "${SSH_OPTS[@]}" "$h" "mkdir -p $w" &&
        run scp -q "${SSH_OPTS[@]}" "$HERE/play.sh" "$HERE/linux.sh" "$h:$w/" &&
        run ssh "${SSH_OPTS[@]}" "$h" "MEMCHECK_GAMES=$(q "$MEMCHECK_GAMES") MEMCHECK_SECONDS=$MEMCHECK_SECONDS" \
            "MEMCHECK_REWIND=$MEMCHECK_REWIND MEMCHECK_ENV_FROM=$(q "${MEMCHECK_ENV_FROM:-steam}")" \
            "bash $w/linux.sh $(q "$MEMCHECK_LINUX_DIR") $(q "$MEMCHECK_LINUX_ROMS") $w/runs"
}
run_mac() {
    run bash "$HERE/mac.sh" "$MEMCHECK_MAC_APP" "$MEMCHECK_MAC_ROMS" "$OUT/mac"
}

status=0
if [ "$JUDGE" = 0 ]; then
    [ "$DRY" = 1 ] || { rm -rf "$OUT" && mkdir -p "$OUT"; }
    echo "memory check: ${SYSTEMS[*]}; $GAMES, rewind: $MEMCHECK_REWIND; ${MEMCHECK_SECONDS} s each; logs in $OUT"
    PIDS=()
    for s in "${SYSTEMS[@]}"; do
        if [ "$DRY" = 1 ]; then
            echo "# $s"
            "run_$s"
        else
            "run_$s" > "$OUT/$s.log" 2> "$OUT/$s.err" &
            PIDS+=($!)
        fi
    done
    [ "$DRY" = 1 ] && exit 0
    for i in "${!PIDS[@]}"; do
        wait "${PIDS[$i]}" || { echo "error: ${SYSTEMS[$i]}'s runner stopped (status $?): $(tail -3 "$OUT/${SYSTEMS[$i]}.err")" >&2; status=1; }
    done
fi

# The verdicts: system, run, peak, end, pass or fail and why
printf '\n%-6s %-16s %8s %8s  %s\n' system game peak end result
for s in "${SYSTEMS[@]}"; do
    awk -v sys="$s" -v limit="$LIMIT" -v secs="$MEMCHECK_SECONDS" -v want="$GAMES" -v rewind="$MEMCHECK_REWIND" '
    function verdict(n,   why, a, b, margin, i) {
        why = ""
        if (!(n in endst)) why = "no end (runner stopped)"
        else if (endst[n] != "exit:0") why = endst[n]
        if (peak[n] > limit) why = (why ? why "; " : "") "over " limit " MB"
        if (why == "" && last[n] < secs - 10) why = "ended at " last[n] " s"
        if (n !~ /\+rewind$/ && n in a3060) {
            a = a3060[n]; b = 0
            for (i = 1; i <= cnt[n]; i++) if (T[n, i] >= last[n] - 30 && M[n, i] > b) b = M[n, i]
            margin = a / 10 > 64 ? a / 10 : 64
            if (b > a + margin) why = (why ? why "; " : "") "climbing: " a " MB at 30-60 s, " b " MB in the last 30 s"
        }
        if (cnt[n] == 0 && why == "") why = "no samples"
        printf "%-6s %-16s %5d MB %5d MB  %s\n", sys, n, peak[n], endmb[n], why ? "FAIL (" why ")" : "pass"
        return why == ""
    }
    $1 == "case" { order[++k] = $2; cnt[$2] = 0; peak[$2] = 0; endmb[$2] = 0; last[$2] = 0 }
    $1 == "sample" && $4 > 0 { n = $2; c = ++cnt[n]; T[n, c] = $3; M[n, c] = $4; last[n] = $3; endmb[n] = $4
        if ($4 > peak[n]) peak[n] = $4
        if ($3 >= 30 && $3 < 60 && (!(n in a3060) || $4 > a3060[n])) a3060[n] = $4 }
    $1 == "end" { endst[$2] = $3 }
    END {
        bad = 0
        split(want, g, " "); for (i in g) expect[g[i]] = 1
        if (rewind != "none") expect[rewind "+rewind"] = 1
        for (i = 1; i <= k; i++) { if (!verdict(order[i])) bad = 1; delete expect[order[i]] }
        for (n in expect) { printf "%-6s %-16s %8s %8s  FAIL (not run)\n", sys, n, "-", "-"; bad = 1 }
        exit bad
    }' "$OUT/$s.log" || status=1
done
for s in "${SYSTEMS[@]}"; do
    m=$(sed -n 's/^memory //p' "$OUT/$s.log" | head -1)
    [ -z "$m" ] || echo "$s: $m"
done
[ "$status" = 0 ] && echo "memory check passed" || echo "MEMORY CHECK FAILED (logs: $OUT)"
exit "$status"

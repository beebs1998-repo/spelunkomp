#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The release's memory check (docs/RELEASING.md): the downloads, as unpacked on Windows, Linux and macOS, each play one
# game of every way the picture is made, for 2 minutes each (MEMCHECK_SECONDS), with the scripted play of
# memcheck/play.sh (coin, Start, free play, the shot held, the stick from side to side): mmpork (CV1000-B), ddpsdoj
# (CV1000-D), ddonpach (the Cave 68000 boards), ket (PGM), then ddpsdoj again with rewind on. Each program's memory is
# logged every 2 s: private bytes on Windows, RSS on Linux and macOS; and the GPU's memory where there is a way to read
# it: on Linux the program's VRAM from its DRM fdinfo (or the card's, less what it was before the run, on an older
# kernel), on Windows its dedicated GPU memory from the GPU Process Memory counters (the Mac's GPU shares its memory).
# The three systems run at once (about 10 minutes); Windows and Linux by ssh (the runners in memcheck/ are copied over
# first), macOS on this Mac.
#
# A run fails if its program's memory goes over 1 GB (MEMCHECK_LIMIT_MB) or its VRAM over 1.5 GB
# (MEMCHECK_VRAM_LIMIT_MB), if either keeps climbing after its first 30 s, or if the program does not end by itself,
# with status 0, after its frames. Climbing: the most it had in its last 30 s is more than the most it had from 30 to
# 60 s by over 64 MB or a tenth, whichever is more (a game's memory goes up and down a little with its scenes; a leak
# keeps going). The rewind run's memory is held to the 1 GB alone: rewind fills a minute of moments, so it climbs, by
# design, for its first minute. A run is stopped (asked to quit, given 10 s, killed only then) if: its memory passes
# 2 GB or its VRAM 3 GB; no sample comes for 10 s; its pictures do not come (the one 5 s after the title by 40 s, the
# one halfway by three quarters of the run and 30 s); or it is still going at twice its time and a minute. A program
# that cannot be stopped even so (held in its GPU driver) ends that machine's runs: it may need restarting.
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
VLIMIT="${MEMCHECK_VRAM_LIMIT_MB:-1536}"
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

# Each system's run; its lines (play.sh's) in $OUT/<system>.log. Linux's remote paths are absolute, under the remote
# home (a relative path given to scp was not the home on SteamOS: the copy went elsewhere)
step() { # what failed, then the command: stops this system's run, saying so
    local what=$1
    shift
    run "$@" || { echo "error: $what failed (status $?): $(printf '%s ' "$@")" >&2; exit 1; }
}
remote_home() { # $1 the host, $2 what --dry-run shows for it; sets RHOME
    if [ "$DRY" = 1 ]; then
        run ssh "${SSH_OPTS[@]}" "$1" 'printf %s "$HOME"'
        RHOME="$2"
        return
    fi
    RHOME=$(ssh "${SSH_OPTS[@]}" "$1" 'printf %s "$HOME"') || { echo "error: ssh to $1 failed (status $?)" >&2; exit 1; }
    RHOME=${RHOME%$'\r'}
    [ -n "$RHOME" ] || { echo "error: ssh to $1 gave no home folder" >&2; exit 1; }
}
run_win() (
    # (Windows' OpenSSH puts a relative path in the user's home, scp's and the shell's alike, cmd's or PowerShell's;
    # its shell may be cmd, which has no printf for remote_home)
    local h=$MEMCHECK_WIN
    step "copying win.ps1 to $h" scp -q "${SSH_OPTS[@]}" "$HERE/win.ps1" "$h:spelunkomp-memcheck.ps1"
    step "the Windows run on $h" ssh "${SSH_OPTS[@]}" "$h" "powershell -NoProfile -ExecutionPolicy Bypass -File spelunkomp-memcheck.ps1" \
        "-Dir $(wq "$MEMCHECK_WIN_DIR") -Roms $(wq "$MEMCHECK_WIN_ROMS") -Games $(wq "$MEMCHECK_GAMES")" \
        "-Seconds $MEMCHECK_SECONDS -Rewind $MEMCHECK_REWIND"
)
run_linux() (
    local h=$MEMCHECK_LINUX w
    remote_home "$h" /home/USER
    w="$RHOME/spelunkomp-memcheck"
    step "making $w on $h" ssh "${SSH_OPTS[@]}" "$h" "mkdir -p $(q "$w")"
    step "copying the runner to $h" scp -q "${SSH_OPTS[@]}" "$HERE/play.sh" "$HERE/linux.sh" "$h:$w/"
    step "checking the copy on $h" ssh "${SSH_OPTS[@]}" "$h" "test -f $(q "$w/linux.sh") && test -f $(q "$w/play.sh")"
    step "the Linux run on $h" ssh "${SSH_OPTS[@]}" "$h" "cd && MEMCHECK_GAMES=$(q "$MEMCHECK_GAMES") MEMCHECK_SECONDS=$MEMCHECK_SECONDS" \
        "MEMCHECK_REWIND=$MEMCHECK_REWIND MEMCHECK_ENV_FROM=$(q "${MEMCHECK_ENV_FROM:-steam}")" \
        "bash $(q "$w/linux.sh") $(q "$MEMCHECK_LINUX_DIR") $(q "$MEMCHECK_LINUX_ROMS") $(q "$w/runs")"
)
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

# The verdicts: system, run, peak, end (memory; and VRAM, where it was measured), pass or fail and why
printf '\n%-6s %-16s %8s %8s %8s %8s  %s\n' system game peak end "vram pk" "vram end" result
for s in "${SYSTEMS[@]}"; do
    awk -v sys="$s" -v limit="$LIMIT" -v vlimit="$VLIMIT" -v secs="$MEMCHECK_SECONDS" -v want="$GAMES" -v rewind="$MEMCHECK_REWIND" '
    function add(w, x) { return w ? w "; " x : x }
    # (climbing: the most of the last 30 s over the most of 30 to 60 s by more than 64 MB or a tenth; column 1 the
    # memory, 2 the VRAM)
    function climbing(n, col, what,   a, b, i, margin) {
        if (!((n, col) in w3060)) return ""
        a = w3060[n, col]; b = 0
        for (i = 1; i <= cnt[n]; i++) if (T[n, i] >= last[n] - 30 && ((n, i, col) in M) && M[n, i, col] > b) b = M[n, i, col]
        margin = a / 10 > 64 ? a / 10 : 64
        return b > a + margin ? what "climbing: " a " MB at 30-60 s, " b " MB in the last 30 s" : ""
    }
    function verdict(n,   why, c, vp, ve) {
        why = ""
        if (!(n in endst)) why = "no end (runner stopped)"
        else if (endst[n] != "exit:0") why = endst[n]
        if (peak[n, 1] > limit) why = add(why, "over " limit " MB")
        if ((n, 2) in peak && peak[n, 2] > vlimit) why = add(why, "VRAM over " vlimit " MB")
        if (why == "" && last[n] < secs - 10) why = "ended at " last[n] " s"
        if (n !~ /\+rewind$/ && (c = climbing(n, 1, ""))) why = add(why, c) # (rewind: its minute of moments fills)
        if ((c = climbing(n, 2, "VRAM "))) why = add(why, c)
        if (cnt[n] == 0 && why == "") why = "no samples"
        vp = (n, 2) in peak ? sprintf("%5d MB", peak[n, 2]) : "-"; ve = (n, 2) in endv ? sprintf("%5d MB", endv[n, 2]) : "-"
        printf "%-6s %-16s %5d MB %5d MB %8s %8s  %s\n", sys, n, peak[n, 1], endv[n, 1], vp, ve, why ? "FAIL (" why ")" : "pass"
        return why == ""
    }
    function bad_line() { if (++perr <= 3) pex[perr] = FNR ": " $0 }
    function known(n) { if (n in seen) return 1; bad_line(); return 0 }
    function value(n, c, col, v) {
        M[n, c, col] = v; endv[n, col] = v
        if (!((n, col) in peak) || v > peak[n, col]) peak[n, col] = v
        if (T[n, c] >= 30 && T[n, c] < 60 && (!((n, col) in w3060) || v > w3060[n, col])) w3060[n, col] = v
    }
    # (a run on Windows comes back with CRLF lines: the CR is dropped, from the line and its fields)
    { sub(/\r+$/, "") }
    NF == 0 || $1 == "memory" || $1 == "vram" || $1 == "log" { next }
    $1 ~ /^(error|warning):$/ { notes[++nn] = $0; next }
    $1 == "case" && NF == 2 { if (!($2 in seen)) { seen[$2] = 1; order[++k] = $2; cnt[$2] = 0; peak[$2, 1] = 0; endv[$2, 1] = 0; last[$2] = 0 }; next }
    $1 == "sample" && (NF == 4 || NF == 5) && $3 ~ /^[0-9]+$/ && $4 ~ /^[0-9]+$/ && (NF == 4 || $5 ~ /^[0-9]+$/) {
        n = $2
        if (!known(n)) next
        if ($4 + 0 == 0) next
        c = ++cnt[n]; T[n, c] = $3 + 0; last[n] = $3 + 0
        value(n, c, 1, $4 + 0)
        if (NF == 5) value(n, c, 2, $5 + 0)
        next
    }
    $1 == "end" && NF == 3 { if (known($2)) endst[$2] = $3; next }
    { bad_line() }
    END {
        bad = 0
        split(want, g, " "); for (i in g) expect[g[i]] = 1
        if (rewind != "none") expect[rewind "+rewind"] = 1
        for (i = 1; i <= k; i++) { if (!verdict(order[i])) bad = 1; delete expect[order[i]] }
        for (n in expect) { printf "%-6s %-16s %8s %8s %8s %8s  FAIL (not run)\n", sys, n, "-", "-", "-", "-"; bad = 1 }
        if (perr) {
            printf "%-6s PARSE ERROR: %d line(s) of %s.log not understood, as:\n", sys, perr, sys; bad = 1
            for (i = 1; i <= perr && i <= 3; i++) printf "         line %s\n", pex[i]
        }
        for (i = 1; i <= nn; i++) printf "%-6s %s\n", sys, notes[i]
        exit bad
    }' "$OUT/$s.log" || status=1
done
for s in "${SYSTEMS[@]}"; do
    m=$(tr -d '\r' < "$OUT/$s.log" | sed -n 's/^memory //p' | head -1)
    v=$(tr -d '\r' < "$OUT/$s.log" | sed -n 's/^vram //p' | head -1)
    [ -z "$m" ] || echo "$s: memory: $m${v:+; VRAM: $v}"
done
[ "$status" = 0 ] && echo "memory check passed" || echo "MEMORY CHECK FAILED (logs: $OUT)"
exit "$status"

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from Fighting Vipers Recomp: https://github.com/tmcleroy/fighting-vipers-recomp
# Starts what was built, with no game and no window: each game program built (build/<id>/<id>, from `make` or
# `make games`) lists its features, and the collection's launcher (`make launcher`) draws its pages to pictures.
# Used by CI and by `make smoke`. Run from the repository's top.
set -eu
cd "$(dirname "$0")/.."
EXE=""
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe ;; esac
started=0
for cfg in games/*/game.cfg; do
    id=$(basename "$(dirname "$cfg")")
    program="build/$id/$id$EXE"
    [ -f "$program" ] || continue
    features="$("$program" --list-features)"
    echo "$features" | grep -q free_play || { echo "$id: the program's features have no free_play"; exit 1; }
    started=$((started + 1))
done
[ "$started" -gt 0 ] || { echo "no game program built (make, or make games)"; exit 1; }
echo "ok: $started game programs start and list their features"

L="build/collection/build"
OUT="$L/smoke"
rm -rf "$OUT" && mkdir -p "$OUT/home"
env SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy "SPELUNKOMP_HOME=$OUT/home" \
    "$L/spelunkomp-launcher$EXE" --screenshot "$OUT/page"
pages="$(ls "$OUT"/page*.png | wc -l | tr -d ' ')"
[ "$pages" -ge 3 ] || { echo "the launcher drew $pages pages, not 3"; exit 1; }
echo "ok: the launcher starts and draws its pages ($pages)"

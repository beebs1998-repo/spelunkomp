#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#
# Whether each game has its translated code (work/<id>/gen/*.cpp, made from the builder's own ROM set): a program
# built without it runs the game on the interpreters, far slower (what CI builds). The app, the collection and the
# release scripts refuse to build without it (tools/make_app.sh, tools/collection.sh, tools/release/lib.sh); plain
# make and CMake builds do not ask.
#   tools/check_gen.sh [GAME...]          fails, naming each game that has none and the command that makes it
#   tools/check_gen.sh --missing [GAME...] only prints the ids of those games, and does not fail
# (every game in games/ unless named; WORK=folder: where the translations are, work by default; ROMS=folder: the ROM
# sets' folder the commands name; OVERRIDE: how the caller is told to build without, INTERPRETED=1 unless it says)
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
missing_only=0
if [ "${1:-}" = "--missing" ]; then missing_only=1; shift; fi
GAMES=${*:-$(cd "$ROOT/games" && for cfg in */game.cfg; do printf '%s ' "${cfg%/game.cfg}"; done)}
WORK=${WORK:-$ROOT/work}
ROMS=${ROMS:-path/to}
missing=""
for game in $GAMES; do
    ls "$WORK/$game/gen/"*.cpp > /dev/null 2>&1 || missing="$missing $game"
done
if [ "$missing_only" = 1 ]; then
    [ -z "$missing" ] || echo $missing
    exit 0
fi
[ -n "$missing" ] || exit 0
echo "error: no translated code (work/<id>/gen) for$missing" >&2
echo "Their programs would run on the interpreters, far slower. Translate each from its ROM set first (docs/BUILDING.md):" >&2
for game in $missing; do
    cfg="$ROOT/games/$game/game.cfg"
    set=$(sed -n 's/^rom_set = //p' "$cfg" | head -1)
    hw=$(sed -n 's/^hardware = //p' "$cfg" | head -1)
    zip="$ROMS/${set:-$game}.zip"
    case "$hw" in
        cave68k|pgm)
            cmd="GAME=$game tools/translate68k.sh $zip"
            # (a PGM game's sound driver, and the Cave 68000 games whose driver is in a ROM: tools/translatez80.sh)
            if [ "$hw" = pgm ] || grep -q '^z80 = ' "$cfg"; then
                cmd="$cmd && GAME=$game tools/translatez80.sh $zip"
            fi
            ;;
        *) cmd="GAME=$game STATIC=4 FOUND=1 tools/translate.sh $zip" ;;
    esac
    echo "  $game: $cmd" >&2
done
if [ -n "${OVERRIDE:-}" ]; then
    echo "$OVERRIDE builds without." >&2
else
    echo "To install Spelunkomp, use the release zip or tools/release/macos.sh. INTERPRETED=1 builds without (marked interpreted)." >&2
fi
exit 1

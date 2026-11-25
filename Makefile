# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The everyday commands (docs/BUILDING.md). A game's program is built with its translated code when work/<id>/gen
# holds it (tools/translate.sh, tools/translate68k.sh: made from your own ROM set), and without it otherwise: then
# it runs the game in the board's interpreters, which is what CI builds, having no game.
#
#   make [GAME=<id>]   one game's program, build/<id>/<id> (mushisam unless named)
#   make games         every game's program
#   make launcher      the collection's launcher, build/collection/build/spelunkomp-launcher
#   make audit         nothing from a game, no keys, no personal paths, the license lines (tools/audit.sh)
#   make check-common  the copy of recomp-common is as recomp-common gave it (tools/sync-common.sh --check)
#   make smoke         the programs built start, with no game and no window (tools/ci_smoke.sh)
#   make check         the three above (no game needed)
#   make collection    every game and the launcher together in build/collection (tools/collection.sh)
#   make app           the macOS app with every game (tools/make_app.sh --collection)
# These two stop unless every game has its translated code (tools/check_gen.sh names those that have none and the
# command that makes it); INTERPRETED=1 builds them anyway, marked "interpreted". To install Spelunkomp, use the
# release zip or tools/release/macos.sh.
#   make android       the Android app with every game (build/release/Spelunkomp-android.apk: tools/release/android.sh)
GAME ?= mushisam
BUILD_TYPE ?= Release
CMAKE_FLAGS ?=
GAMES := $(notdir $(patsubst %/game.cfg,%,$(wildcard games/*/game.cfg)))

.PHONY: all game games launcher audit check-common smoke check collection app android

all: game

game:
	cmake -S . -B build/$(GAME) -G Ninja -DGAME=$(GAME) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)
	ninja -C build/$(GAME) game

games:
	@for id in $(GAMES); do $(MAKE) --no-print-directory game GAME=$$id || exit 1; done

launcher:
	cmake -S . -B build/collection/build -G Ninja -DGAME=$(GAME) -DCOLLECTION=ON -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)
	ninja -C build/collection/build launcher

audit:
	@tools/audit.sh

check-common:
	@tools/sync-common.sh --check

smoke:
	@tools/ci_smoke.sh

check: audit check-common smoke

collection:
	tools/collection.sh

app: collection
	tools/make_app.sh --collection

android:
	tools/release/android.sh

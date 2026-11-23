#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# The collection: one launcher for every game (recomp-common's launcher with LAUNCHER_COLLECTION; launcher/src/
# description.cpp), in build/collection/ with each game's program beside it. Each game is built in build/<id>/ with
# its translations (work/<id>/gen, from tools/translate.sh; a game with none is built without, and runs interpreted).
# Run from the repository's top.
#   B=folder   where (build/collection)
set -eu
B=${B:-build/collection}
mkdir -p "$B"
for cfg in games/*/game.cfg; do
    id=$(basename "$(dirname "$cfg")")
    cmake -S . -B "build/$id" -G Ninja -DGAME="$id" -DCMAKE_BUILD_TYPE=Release > /dev/null
    ninja -C "build/$id" "$id" > /dev/null
    cp -p "build/$id/$id" "$B/"
    echo "$id: build/$id/$id"
done
cmake -S . -B "$B/build" -G Ninja -DGAME="$(basename "$(dirname "$(ls games/*/game.cfg | head -1)")")" -DCOLLECTION=ON \
    -DCMAKE_BUILD_TYPE=Release > /dev/null
ninja -C "$B/build" spelunkomp-launcher > /dev/null
cp -p "$B/build/spelunkomp-launcher" "$B/"
echo "built $B/spelunkomp-launcher"

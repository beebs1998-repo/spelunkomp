#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# MAME's split and merged sets, made from the player's own (non-merged) sets and checked as the launcher checks a
# set (tests/romset_test.cpp): for each game with a parent (game.cfg's rom_parent), its split zip with the parent's
# beside it, chosen and in a folder; the split zip alone, which must say what it needs; and the merged parent zip.
# The parent is the player's own set where they have it (its files the clone shares are left out of the clone's),
# made up from the clone's own files where they don't (all but the program's then go in it).
#   tools/check_split_sets.sh [ROM_FOLDER]   (default ../cv1k-roms). Run from the repository's top.
set -eu
cd "$(dirname "$0")/.."
ROMS="${1:-../cv1k-roms}"
B=build/romtest
cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
ninja -C "$B" romset-test >/dev/null
OUT="$B/split"
rm -rf "$OUT" && mkdir -p "$OUT"
tested=0
for cfg in games/*/game.cfg; do
    parents=$(sed -n 's/^rom_parent = //p' "$cfg")
    [ -n "$parents" ] || continue
    set_name=$(sed -n 's/^rom_set = //p' "$cfg")
    full="$ROMS/$set_name.zip"
    [ -f "$full" ] || { echo "$set_name: no $full, not checked"; continue; }
    parent=${parents%% *}
    d="$OUT/$set_name"
    mkdir -p "$d/split" "$d/alone" "$d/merged"
    python3 -I - "$cfg" "$full" "$ROMS/$parent.zip" "$d" "$set_name" "$parent" <<'EOF'
import os, sys, zipfile
cfg, full, real_parent, d, name, parent = sys.argv[1:]
program = set()
for line in open(cfg):
    if line.startswith("program = "):
        for one in line.split("=", 1)[1].split():
            program.add(int(one.split("@")[0].split(":")[1], 16))
child = zipfile.ZipFile(full)
if os.path.exists(real_parent):
    theirs = zipfile.ZipFile(real_parent)
    shared = {i.CRC for i in theirs.infolist()}
    parent_files = [(theirs, i) for i in theirs.infolist()]
else:
    shared = {i.CRC for i in child.infolist() if i.CRC not in program}
    parent_files = [(child, i) for i in child.infolist() if i.CRC in shared]
own = [i for i in child.infolist() if i.CRC not in shared]
assert own and parent_files, "nothing to split"
def write(path, files):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for source, info in files:
            z.writestr(info.filename, source.read(info))
write(f"{d}/split/{name}.zip", [(child, i) for i in own])
write(f"{d}/split/{parent}.zip", parent_files)
os.link(f"{d}/split/{name}.zip", f"{d}/alone/{name}.zip")
# (a merged set: the clone's own files in a folder named for it inside the parent's zip, as MAME's merged sets have)
with zipfile.ZipFile(f"{d}/merged/{parent}.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for source, info in parent_files:
        z.writestr(info.filename, source.read(info))
    for info in own:
        z.writestr(f"{name}/{info.filename}", child.read(info))
print(f"{name}: {len(own)} own files, {len(parent_files)} in {parent}.zip")
EOF
    t="$B/romset-test"
    "$t" "$cfg" "$d/split/$set_name.zip" >/dev/null || { echo "$set_name: the split zip, chosen, is not usable: $("$t" "$cfg" "$d/split/$set_name.zip")"; exit 1; }
    "$t" "$cfg" "$d/split" >/dev/null || { echo "$set_name: a folder of the split zips is not usable"; exit 1; }
    if message=$("$t" "$cfg" "$d/alone/$set_name.zip"); then
        echo "$set_name: the split zip with no parent beside it was taken as usable"; exit 1
    fi
    echo "$message" | grep -q "split set also needs $parent.zip" || { echo "$set_name: the message does not name the parent: $message"; exit 1; }
    "$t" "$cfg" "$d/merged/$parent.zip" >/dev/null || { echo "$set_name: the merged zip, chosen, is not usable"; exit 1; }
    "$t" "$cfg" "$d/merged" >/dev/null || { echo "$set_name: a folder holding the merged zip is not usable"; exit 1; }
    "$t" "$cfg" "$full" >/dev/null || { echo "$set_name: the full set is not usable"; exit 1; }
    tested=$((tested + 1))
done
[ "$tested" -gt 0 ] || { echo "no game with a parent had its set in $ROMS"; exit 1; }
echo "ok: $tested games' split and merged sets are usable; a split set alone names its parent"

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# buffi's CV1000 benchmark (homebrew, October 2026) on the port's board, with "Slowdown as the board" off and on,
# beside buffi's results from a PCB, the MiSTer core and MAME (docs/plans/BENCHMARK.md). Nothing of the benchmark is
# in the repository: give it the autorun build's zip (mame/autorun/cv1kbench.zip in buffi's download, Ibara's file
# names). The zip is unpacked into a folder of its own and run as an ibara set with its own checksums, by a game.cfg
# written beside it; the board's interpreter runs it (the translated code is Ibara's, not this program).
#   tools/cv1kbench.sh ZIP [OUT]   (B=build/ibara: a build with the headless runner, `ninja -C build/ibara spelunk-headless`)
set -eu
ZIP=${1:?usage: tools/cv1kbench.sh path/to/cv1kbench.zip [OUT]}
cd "$(dirname "$0")/.."
B=${B:-build/ibara}
OUT=${2:-work/cv1kbench}
FRAMES=${FRAMES:-12000}
RUNNER=$B/spelunk-headless
[ -x "$RUNNER" ] || { echo "no $RUNNER: cmake -S . -B $B -G Ninja -DGAME=ibara && ninja -C $B spelunk-headless"; exit 1; }
mkdir -p "$OUT/rom"
unzip -o -q "$ZIP" -d "$OUT/rom"
# (the four files' checksums, from the zip, into a copy of Ibara's game.cfg)
python3 -I - "$ZIP" games/ibara/game.cfg "$OUT/bench.cfg" <<'EOF'
import sys, zipfile
crc = {i.filename: "%08x" % i.CRC for i in zipfile.ZipFile(sys.argv[1]).infolist()}
need = ["ibara_u4", "u2", "u23", "u24"]
missing = [n for n in need if n not in crc]
if missing:
    sys.exit("the zip has no " + ", ".join(missing) + " (the autorun build's MAME zip, Ibara's file names, is wanted)")
out = []
for line in open(sys.argv[2]):
    key = line.split("=")[0].strip()
    line = {"rom_set": "rom_set = bench\n", "program": "program = ibara_u4:%s\n" % crc["ibara_u4"],
            "nand": "nand = u2:%s\n" % crc["u2"], "sound": "sound = u23:%s u24:%s\n" % (crc["u23"], crc["u24"])}.get(key, line)
    out.append(line)
open(sys.argv[3], "w").writelines(out)
EOF
last=$((FRAMES - 1))
run() { # name [--timing]
    "$RUNNER" --game "$OUT/bench.cfg" --roms "$OUT/rom" --cpu interpreter --frames "$FRAMES" "$@" \
        --shot "$last:$OUT/$name.png" --dump-ram "$last:$OUT/$name.ram" > "$OUT/$name.log" 2>&1
}
name=off; run &
name=on; run --timing &
wait
# The results screen's lines are kept in main RAM as text, 16 characters of name and 16 of value each
python3 -I - "$OUT/off.ram" "$OUT/on.ram" <<'EOF'
import re, sys
board = {  # buffi's: PCB, MiSTer (ikacore_CV1k), MAME
    "RAMP 8X8 SPR": (4800, 5650, 4875), "RAMP 8X8 STILL": (9085, 9085, 9123), "RAMP 32X32 SPR": (1170, 1170, 1175),
    "RAMP CPU LOGIC": (1503, 1769, 2135), "RAMP GAME": (1694, 1715, 1701), "BLIT LATENCY": (5, 4, 5),
    "8X8 SPRITES": (603, 604, 604), "32X32 SPRITES": (77, 77, 77), "SCREEN COPY": (90.6, 90.6, 91.0),
    "ALPHA 32X32": (77, 77, 77), "UPLOAD TO VRAM": (5632, 8198, 5665), "GAME FRAME": (171.3, 171.2, 171.8),
    "BLITTER PAIN": (19.7, 19.7, 19.9), "CPU INTEGER": (3656, 3937, 4653), "CPU DIVIDE": (1176, 1256, 1304),
    "CPU MEMORY": (20.1, 19.7, 14.7), "CPU CACHE HIT": (17.0, 20.4, 17.0), "CPU FLUSH": (886, 734, 604),
    "CPU LIST BUILD": (352, 431, 368), "CPU QUICKSORT": (140, 155, 142),
}
def results(path):
    ram = open(path, "rb").read()
    at = ram.find(b"  RAMP 8X8 SPR")
    if at < 0:
        sys.exit(path + ": no results screen yet (more FRAMES?)")
    got = {}
    for k in range(20):
        rec = ram[at + 32 * k: at + 32 * k + 32]
        name = rec[:16].decode().strip()
        value = rec[16:].split(b"\0")[0].decode().strip()
        got[name] = (float(re.match(r"[0-9.]+", value).group()), value)
    return got
off, on = results(sys.argv[1]), results(sys.argv[2])
print("%-15s %8s %8s %8s %8s %8s %8s  %s" % ("test", "PCB", "on", "off", "MiSTer", "MAME", "on/PCB", "unit"))
for name, (pcb, mister, mame) in board.items():
    print("%-15s %8g %8g %8g %8g %8g %+7.1f%%  %s" % (name, pcb, on[name][0], off[name][0], mister, mame,
          100.0 * (on[name][0] / pcb - 1), on[name][1].split(" ", 1)[1]))
EOF

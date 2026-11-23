#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The CV1000 CPU timing test (tools/cv1ktest/cv1ktest.py, docs/plans/BENCHMARK.md "A board test") on the port's
# board: its program flash written, run as an ibara set (the NAND and sound ROMs taken from ROMS/ibara.zip, which the
# test does not read), with the model on (--timing) and the board's interpreter, and its results read from main RAM.
#   tools/cv1ktest/run.sh ROMS [OUT]   (B=build/ibara: a build with the headless runner)
set -eu
ROMS=${1:?usage: tools/cv1ktest/run.sh ROMS [OUT]}
cd "$(dirname "$0")/../.."
B=${B:-build/ibara}
OUT=${2:-work/cv1ktest}
mkdir -p "$OUT/rom"
python3 -I tools/cv1ktest/cv1ktest.py "$OUT/rom/test_u4" > "$OUT/tests.txt"
unzip -o -q "$ROMS/ibara.zip" u2 u23 u24 -d "$OUT/rom"
python3 -I - "$OUT/rom" games/ibara/game.cfg "$OUT/test.cfg" <<'PY'
import sys, zlib
crc = lambda n: "%08x" % (zlib.crc32(open(sys.argv[1] + "/" + n, "rb").read()) & 0xFFFFFFFF)
out = []
for line in open(sys.argv[2]):
    key = line.split("=")[0].strip()
    line = {"rom_set": "rom_set = cv1ktest\n", "program": "program = test_u4:%s\n" % crc("test_u4"),
            "nand": "nand = u2:%s\n" % crc("u2"), "sound": "sound = u23:%s u24:%s\n" % (crc("u23"), crc("u24"))}.get(key, line)
    out.append(line)
open(sys.argv[3], "w").writelines(out)
PY
"$B/spelunk-headless" --game "$OUT/test.cfg" --roms "$OUT/rom" --cpu interpreter --frames 30 --timing \
    --dump-ram "29:$OUT/ram" > "$OUT/log" 2>&1
python3 -I - "$OUT/ram" "$OUT/tests.txt" <<'PY'
import struct, sys
ram = open(sys.argv[1], "rb").read()
names = [l.strip().split(" ", 1)[1] for l in open(sys.argv[2]) if l.strip()]
if struct.unpack_from(">I", ram, 0x8FC)[0] != 0xC0DEC0DE:
    sys.exit("the test did not finish")
bodies = 16 * 4096
ticks = [struct.unpack_from(">I", ram, 0x800 + 4 * k)[0] for k in range(len(names))]
print("%2s %-26s %8s %10s" % ("", "test", "ticks", "cyc/body"))
for k, name in enumerate(names):  # (cycles a body: the loop's own turns taken off, as the empty loop measures them)
    print("%2d %-26s %8d %10.3f" % (k, name, ticks[k], 16.0 * (ticks[k] - ticks[0]) / bodies))
PY

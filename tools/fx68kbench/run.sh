#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# fx68k (Jorge Cwik's cycle-exact 68000, GPL-3; not in this repository: give the folder holding fx68k.sv, as
# Arcade-Cave_MiSTer's rtl/fx68k) alone on memory with no waits, against the port's interpreter (machine/m68k.cpp)
# on the same test images:
#   movem.py      every MOVEM form with A7 as the base ((A7), -(A7), (A7)+, d(A7), d(A7,Xn)) or in the list, words and
#                 longs, beside the same forms on A0, absolute and PC-relative: each one's time (between two marker
#                 writes, less a NOP's), and the registers and memory after
#   irq_stack.py  DoDonPachi's digits built below the stack pointer (057A82..057A8C: movea.l a7,a0; three move.b
#                 to -(a0); clr.b -(a0); move.l (a0),d2) with the level-1 line raised at every cycle across them:
#                 which instruction the interrupt follows, what it pushes over the digits, what is read back, the time
#   mask.py       the line raised at every cycle around a MOVE to SR or CCR from a register or memory (Ketsui's
#                 0A738C `move d1,sr` lowering the mask), with the mask 7 and 0 before: after which instruction
#   tools/fx68kbench/run.sh FX68K_DIR [OUT]   (needs Verilator; OUT: build/fx68kbench)
set -eu
FX=$(cd "${1:?usage: run.sh FX68K_DIR [OUT]}" && pwd)
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${2:-$ROOT/build/fx68kbench}
mkdir -p "$OUT"
cd "$OUT"
cp "$FX"/microrom.mem "$FX"/nanorom.mem .
[ -x obj_dir/fxbench ] || verilator --cc --exe --build -j 2 -O3 -Wno-fatal -Wno-lint -Wno-style --top-module fx68k \
    "$FX/fx68k.sv" "$FX/fx68kAlu.sv" "$FX/uaddrPla.sv" "$HERE/tb.cpp" -o fxbench > verilator.log
c++ -std=c++20 -O2 -iquote "$ROOT" "$HERE/pb.cpp" "$ROOT/machine/m68k.cpp" -o pb
python3 "$HERE/movem.py" movem.img movem.cases
./obj_dir/fxbench movem.img fx.mem > fx.log
./pb movem.img pb.mem movem.cases > pb.log
python3 "$HERE/compare.py" timing fx.log pb.log fx.mem pb.mem
python3 "$HERE/irq_stack.py" irq.img
: > irq.out
for k in $(seq 0 80); do
    # (the bench raises the line when the marker's write's address strobe falls, a cycle into its bus cycle; the
    # port's harness counts from the bus cycle's start: k there is k + 1 here)
    ./obj_dir/fxbench irq.img fxi.mem 0 $((k + 1)) > fxi.log
    ./pb irq.img pbi.mem - 0 $k > pbi.log
    python3 "$HERE/compare.py" irq fxi.log pbi.log fxi.mem pbi.mem $k >> irq.out
done
echo "irq_stack: $(grep -c ' same$' irq.out) of $(wc -l < irq.out) rise times the same on both"
grep -v ' same$' irq.out || true
: > mask.out
for sr in 2700 2000; do
    for ins in 46C1 46D0 46E8,0000 46F9,0002,0000 44C1 44D0 40C1 40D0; do
        python3 "$HERE/mask.py" mask.img $sr $ins
        for k in $(seq 0 60); do
            ./obj_dir/fxbench mask.img fxi.mem 0 $((k + 1)) > fxi.log
            ./pb mask.img pbi.mem - 0 $k > pbi.log
            echo "$sr $ins $(python3 "$HERE/compare.py" irq fxi.log pbi.log fxi.mem pbi.mem $k)" >> mask.out
        done
    done
done
echo "mask: $(grep -c ' same$' mask.out) of $(wc -l < mask.out) the same on both"
grep -v ' same$' mask.out || true

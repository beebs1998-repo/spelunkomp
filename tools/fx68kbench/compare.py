# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# tools/fx68kbench/run.sh: fx68k's log and memory against the port's.
#   compare.py timing FX.log PORT.log FX.mem PORT.mem   each case's time (fx68k: between the markers, less a NOP's)
#   compare.py irq FX.log PORT.log FX.mem PORT.mem K    one rise time of irq_stack.py: a line, "same" or "DIFF"
import struct, sys
kind, fxlog, pblog, fxmem, pbmem = sys.argv[1:6]
a, b = open(fxmem, 'rb').read(), open(pbmem, 'rb').read()
def marks(path):
    x = [l.split() for l in open(path) if l[0] in 'AB']
    return [int(v[1]) for v in x if v[0] == 'A'], [int(v[1]) for v in x if v[0] == 'B']
A, B = marks(fxlog)
if kind == 'timing':
    over = B[0] - A[0] - 4
    bad = 0
    for i, line in enumerate(open(pblog)):
        c, name = line.rstrip('\n').split(' ', 1)
        t = B[i] - A[i] - over - (4 if name == 'nop2' else 0)
        if t != int(c):
            bad += 1
            print('DIFF %-36s fx68k %3d port %s' % (name, t, c))
    print('movem: %d forms, %d with another time, %d bytes of registers and memory differing' % (i + 1, bad, sum(x != y for x, y in zip(a, b))))
else:
    PA, PB = marks(pblog)
    tf, tp = B[0] - A[0], PB[0] - PA[0]
    d2 = lambda m: struct.unpack('>I', m[0xE0008:0xE000C])[0]
    print('%s fx68k: %3d cycles d2 %08X below a7 %s | port: %3d cycles d2 %08X below a7 %s %s' % (sys.argv[6], tf, d2(a), a[0x5FFFA:0x60000].hex(),
          tp, d2(b), b[0x5FFFA:0x60000].hex(), 'same' if a == b and tf == tp else 'DIFF'))

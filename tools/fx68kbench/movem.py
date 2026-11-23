# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# tools/fx68kbench/run.sh's MOVEM image: every form with A7 as the base or in the list, beside the same forms on A0
# (and absolute, PC-relative), words and longs, and a NOP to measure the markers by.
#   movem.py IMAGE CASES
import struct, sys
img = bytearray(0x100000)
def w16(a, v): img[a:a+2] = struct.pack('>H', v & 0xFFFF)
def w32(a, v): img[a:a+4] = struct.pack('>I', v & 0xFFFFFFFF)
# memory pattern: every word even, inside 0x20000-0x7FFFE (an address register loaded from it stays usable)
for a in range(0x10000, 0x100000, 2):
    w16(a, (a * 7) & 0xFFFE)
pc = 0x1000
code = []
def emit(*words):
    global pc
    for x in words:
        w16(pc, x); pc += 2
w32(0, 0x00070000); w32(4, 0x1000)
cases = []
REG = {**{'d%d' % i: i for i in range(8)}, **{'a%d' % i: 8 + i for i in range(8)}}
def mask(regs, predec):
    m = 0
    for r in regs:
        n = REG[r]
        m |= 1 << (15 - n if predec else n)
    return m
def ea_words(mode):
    # returns (mode field, reg field, extension words)
    import re
    if m := re.fullmatch(r'\(a(\d)\)', mode): return 2, int(m[1]), []
    if m := re.fullmatch(r'\(a(\d)\)\+', mode): return 3, int(m[1]), []
    if m := re.fullmatch(r'-\(a(\d)\)', mode): return 4, int(m[1]), []
    if m := re.fullmatch(r'(-?\d+)\(a(\d)\)', mode): return 5, int(m[2]), [int(m[1]) & 0xFFFF]
    if m := re.fullmatch(r'(-?\d+)\(a(\d),d(\d)\.w\)', mode): return 6, int(m[2]), [(int(m[3]) << 12) | (int(m[1]) & 0xFF)]
    if m := re.fullmatch(r'\$(\w+)\.w', mode): return 7, 0, [int(m[1], 16)]
    if m := re.fullmatch(r'\$(\w+)\.l', mode): return 7, 1, [int(m[1], 16) >> 16, int(m[1], 16) & 0xFFFF]
    if m := re.fullmatch(r'(\d+)\(pc\)', mode): return 7, 2, [int(m[1])]
    raise ValueError(mode)
def movem(size, regs, mode, to_regs):
    md, rg, ext = ea_words(mode)
    op = 0x4880 | (0x400 if to_regs else 0) | (0x40 if size == 'l' else 0) | (md << 3) | rg
    return [op, mask(regs, md == 4)] + ext
def setregs(n):
    for i in range(8):
        v = [0x10000 + i, 4, 0x80, 0xFFFF8001, 0x7FFF0002, 0x12345678, 0x0BADF00D, 0xCAFEBABE][i]
        emit(0x203C | (i << 9), v >> 16, v & 0xFFFF)
    for i in range(7):
        v = 0x40000 + i * 0x1000 + n * 0x40
        emit(0x207C | (i << 9), v >> 16, v & 0xFFFF)
    v = 0x60000 + n * 0x40
    emit(0x2E7C, v >> 16, v & 0xFFFF)  # movea.l #v,a7
def case(name, words):
    n = len(cases)
    setregs(n)
    emit(0x33C0, 0x00F0, 0x0000)  # marker A: move.w d0,$F00000
    at = pc
    emit(*words)
    emit(0x33C0, 0x00F0, 0x0002)  # marker B
    dump = 0xE0000 + n * 0x40
    emit(0x48F9, 0x7FFF, dump >> 16, dump & 0xFFFF)   # movem.l d0-d7/a0-a6,dump
    emit(0x23CF, (dump + 60) >> 16, (dump + 60) & 0xFFFF)  # move.l a7,dump+60
    cases.append((name, at, dump))
case('nop', [0x4E71])
case('nop2', [0x4E71, 0x4E71])
lists = {'d0': ['d0'], 'd0-d3': ['d0','d1','d2','d3'], 'a7': ['a7'], 'd0/a7': ['d0','a7'], 'd2/a0/a7': ['d2','a0','a7'],
         'd0-d7/a0-a6': [f'd{i}' for i in range(8)] + [f'a{i}' for i in range(7)], 'all': list(REG)}
tomem = ['(a7)', '-(a7)', '8(a7)', '-8(a7)', '4(a7,d1.w)', '(a0)', '-(a0)', '8(a0)', '4(a0,d1.w)', '$4000.w', '$40000.l']
toreg = ['(a7)', '(a7)+', '8(a7)', '4(a7,d1.w)', '(a0)', '(a0)+', '8(a0)', '4(a0,d1.w)', '$4000.w', '$40000.l', '16(pc)']
for size in 'wl':
    for ln, regs in lists.items():
        for m in tomem:
            case(f'movem.{size} {ln},{m}', movem(size, regs, m, False))
        for m in toreg:
            case(f'movem.{size} {m},{ln}', movem(size, regs, m, True))
emit(0x33C0, 0x00F0, 0x0004)  # end
emit(0x60FE)
open(sys.argv[1], 'wb').write(img)
with open(sys.argv[2], 'w') as f:
    for name, at, dump in cases:
        f.write('%06X %06X %s\n' % (at, dump, name))
print(len(cases), 'cases, code to', hex(pc), file=sys.stderr)

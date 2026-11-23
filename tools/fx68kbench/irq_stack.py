# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# tools/fx68kbench/run.sh's interrupt image: DoDonPachi's digits below the stack (057A82..057A8C), the handler an RTE.
#   irq_stack.py IMAGE
import sys
import struct
img = bytearray(0x100000)
def w16(a, v): img[a:a+2] = struct.pack('>H', v & 0xFFFF)
def w32(a, v): img[a:a+4] = struct.pack('>I', v & 0xFFFFFFFF)
w32(0, 0x00070000); w32(4, 0x1000); w32(0x64, 0x800)
w16(0x800, 0x4E73)  # rte
pc = 0x1000
def emit(*ws):
    global pc
    for x in ws: w16(pc, x); pc += 2
emit(0x46FC, 0x2000)                    # move #$2000,sr
emit(0x223C, 0, 0x12)                   # move.l #$12,d1
emit(0x243C, 0, 0x34)                   # move.l #$34,d2
emit(0x263C, 0, 0x56)                   # move.l #$56,d3
emit(0x2E7C, 0x0006, 0x0000)            # movea.l #$60000,a7
emit(0x33C0, 0x00F0, 0x0000)            # marker A
emit(0x204F)                            # movea.l a7,a0
emit(0x1101, 0x1102, 0x1103, 0x4220)    # move.b d1,-(a0); d2; d3; clr.b -(a0)
emit(0x2410)                            # move.l (a0),d2
emit(0x33C0, 0x00F0, 0x0002)            # marker B
emit(0x48F9, 0x7FFF, 0x000E, 0x0000)    # movem.l d0-d7/a0-a6,$E0000
emit(0x33C0, 0x00F0, 0x0004)
emit(0x60FE)
open(sys.argv[1], 'wb').write(img)

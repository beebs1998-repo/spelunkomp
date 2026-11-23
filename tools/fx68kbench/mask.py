# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# tools/fx68kbench/run.sh's mask image: the level-1 line raised around a MOVE to SR or CCR (Ketsui's 0A738C
# `move d1,sr`, with d1 = 2000), the status register SR before: after which instruction the interrupt is taken.
#   mask.py IMAGE SR INSTRUCTION-WORDS (hex, by commas)
import struct, sys
img = bytearray(0x100000)
def w16(a, v): img[a:a+2] = struct.pack('>H', v & 0xFFFF)
def w32(a, v): img[a:a+4] = struct.pack('>I', v & 0xFFFFFFFF)
w32(0, 0x00070000); w32(4, 0x1000); w32(0x64, 0x800)
w16(0x800, 0x4E73)  # rte
pc = 0x1000
def emit(*ws):
    global pc
    for x in ws: w16(pc, x); pc += 2
emit(0x46FC, int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x2700)  # move #SR,sr
emit(0x223C, 0, 0x2000)                 # move.l #$2000,d1
emit(0x2E7C, 0x0006, 0x0000)            # movea.l #$60000,a7
emit(0x207C, 0x0002, 0x0000)            # movea.l #$20000,a0
emit(0x33C1, 0x0002, 0x0000)            # move.w d1,$20000
emit(0x2F00)                            # move.l d0,-(a7)
emit(0x33C0, 0x00F0, 0x0000)            # marker A
emit(0x4E71, 0x4E71)                    # nop; nop
emit(*[int(x,16) for x in sys.argv[3].split(',')])  # the instruction
emit(0x201F)                            # move.l (a7)+,d0
emit(0x4E71, 0x4E71)                    # nop; nop
emit(0x33C0, 0x00F0, 0x0002)            # marker B
emit(0x48F9, 0x7FFF, 0x000E, 0x0000)    # movem.l d0-d7/a0-a6,$E0000
emit(0x33C0, 0x00F0, 0x0004)
emit(0x60FE)
open(sys.argv[1], 'wb').write(img)

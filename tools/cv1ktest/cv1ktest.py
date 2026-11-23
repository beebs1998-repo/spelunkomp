#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""A CV1000 CPU timing test: timed SH-3 loops that each isolate one pipeline rule (docs/plans/BENCHMARK.md, "A board
test"). Written here, from the SH-3 manuals; nothing of anyone else's program is in it.

    cv1ktest.py OUT_U4      writes the program flash image (U4, 2 MB, its words byte-swapped as the chip is read)

The program sets the bus and the clock as the CV1000's games do at boot, copies its tests into main RAM, turns the
cache on (CCR 0D: flushed, P1 copy-back, as buffi's benchmark has it) and runs them from P1 with interrupts blocked.
Each test is a loop of REP copies of a short body run COUNT times, timed with timer 0 at Pphi/4 (16 CPU cycles a
tick at 102.4 MHz). The ticks of test k are written to main RAM at 0C000800 + 4k (a longword each; 0C0008FC holds
C0DEC0DE when all are done). No display: to read them on a board, the loops go into a program that has one (buffi's
benchmark), or the RAM is read by other means. On the port: tools/cv1ktest/run.sh.
"""
import struct
import sys

REP, COUNT = 16, 4096  # (65,536 bodies a test)
RESULTS = 0xAC000800   # (main RAM, uncached)
TESTS = [
    # name, setup (before the loop), body; r0 = 1, r1 = 2 (so cmp/eq r0,r1 is false), r2 = a cached word
    ("empty loop", [], []),
    ("nop nop", [], ["nop", "nop"]),
    ("cmp/eq(F) bt", [], ["cmp/eq r0,r1", "bt -1"]),
    ("cmp/eq(F) nop bt", [], ["cmp/eq r0,r1", "nop", "bt -1"]),
    ("nop bt (T from dt)", [], ["nop", "bt -1"]),
    ("cmp/eq(F) bt/s nop", [], ["cmp/eq r0,r1", "bt/s +0", "nop"]),
    ("cmp/eq(F) nop bt/s nop", [], ["cmp/eq r0,r1", "nop", "bt/s +0", "nop"]),
    ("cmp/eq(T) bt taken", [], ["cmp/eq r0,r0", "bt -1"]),
    ("cmp/eq(T) nop bt taken", [], ["cmp/eq r0,r0", "nop", "bt -1"]),
    ("bra even target", [], ["bra +0", "nop"]),
    ("bra odd target", ["odd"], ["bra +0", "nop"]),
    ("load hit", [], ["mov.l @r2,r3", "nop"]),
    ("store hit", [], ["mov.l r3,@r2", "nop"]),
    ("load then use", [], ["mov.l @r2,r3", "add r3,r4"]),
    ("load no use", [], ["mov.l @r2,r3", "add r5,r4"]),
]


class Asm:
    def __init__(self, origin):
        self.origin, self.items, self.labels = origin, [], {}

    def label(self, name):
        self.items.append(("label", name))

    def __call__(self, text):
        self.items.append(("insn", text))

    def word(self, value):
        self.items.append(("long", value))

    def align(self, n):
        self.items.append(("align", n))

    def _layout(self):
        pc, out = self.origin, []
        for kind, value in self.items:
            if kind == "label":
                self.labels[value] = pc
            elif kind == "align":
                while pc % value:
                    out.append((pc, "insn", "nop"))
                    pc += 2
            elif kind == "long":
                if pc % 4:
                    out.append((pc, "insn", "nop"))
                    pc += 2
                out.append((pc, "long", value))
                pc += 4
            else:
                out.append((pc, "insn", value))
                pc += 2
        return out

    def assemble(self):
        out = self._layout()
        self._layout()  # (labels known)
        data = bytearray()
        for pc, kind, value in out:
            if kind == "long":
                v = self.labels[value] if isinstance(value, str) else value
                data += struct.pack(">I", v & 0xFFFFFFFF)
            else:
                data += struct.pack(">H", self.encode(pc, value))
        return bytes(data)

    def target(self, pc, arg):
        if arg.startswith(("+", "-")):  # (in instructions after the next: +0 is pc + 4)
            return pc + 4 + 2 * int(arg)
        return self.labels[arg]

    def encode(self, pc, text):
        op, _, rest = text.partition(" ")
        a, depth, cur = [], 0, ""
        for ch in rest:
            if ch == "," and depth == 0:
                a.append(cur.strip())
                cur = ""
                continue
            depth += ch == "("
            depth -= ch == ")"
            cur += ch
        if cur.strip():
            a.append(cur.strip())

        def r(x):
            assert x[0] == "r", text
            return int(x[1:])

        def disp(bits, value, scale):
            assert value % scale == 0, text
            d = value // scale
            assert -(1 << (bits - 1)) <= d < (1 << (bits - 1)), text
            return d & ((1 << bits) - 1)

        fixed = {"nop": 0x0009, "clrt": 0x0008, "sett": 0x0018, "rts": 0x000B}
        if op in fixed:
            return fixed[op]
        if op == "mov" and a[0].startswith("#"):
            return 0xE000 | r(a[1]) << 8 | (int(a[0][1:], 0) & 0xFF)
        if op == "mov":
            return 0x6003 | r(a[1]) << 8 | r(a[0]) << 4
        if op == "mov.l" and a[0].startswith("@pc:"):  # mov.l @pc:LABEL,rn (a literal)
            t = self.labels.get(a[0][4:], pc)
            d = (t - ((pc & ~3) + 4)) // 4
            assert 0 <= d < 256 or a[0][4:] not in self.labels, text
            return 0xD000 | r(a[1]) << 8 | (d & 0xFF)
        if op in ("mov.l", "mov.w", "mov.b") and a[0].startswith("@r") and "+" not in a[0]:
            return 0x6000 | r(a[1]) << 8 | r(a[0][1:]) << 4 | {"mov.b": 0, "mov.w": 1, "mov.l": 2}[op]
        if op in ("mov.l", "mov.w", "mov.b") and a[1].startswith("@r"):
            return 0x2000 | r(a[1][1:]) << 8 | r(a[0]) << 4 | {"mov.b": 0, "mov.w": 1, "mov.l": 2}[op]
        if op == "mov.l" and a[0].startswith("@r") and a[0].endswith("+"):
            return 0x6006 | r(a[1]) << 8 | r(a[0][1:-1]) << 4
        if op == "add" and a[0].startswith("#"):
            return 0x7000 | r(a[1]) << 8 | (int(a[0][1:], 0) & 0xFF)
        two = {"add": 0x300C, "sub": 0x3008, "cmp/eq": 0x3000, "cmp/gt": 0x3007, "xor": 0x200A}
        if op in two:
            return two[op] | r(a[1]) << 8 | r(a[0]) << 4
        one = {"dt": 0x4010, "jmp": 0x402B, "jsr": 0x400B, "shll2": 0x4008}
        if op in one:
            return one[op] | r(a[0].lstrip("@")) << 8
        ldc = {"sr": 0x400E, "gbr": 0x401E, "vbr": 0x402E}
        if op == "ldc":
            return ldc[a[1]] | r(a[0]) << 8
        if op in ("bt", "bf", "bt/s", "bf/s"):
            base = {"bt": 0x8900, "bf": 0x8B00, "bt/s": 0x8D00, "bf/s": 0x8F00}[op]
            return base | disp(8, self.target(pc, a[0]) - (pc + 4), 2)
        if op in ("bra", "bsr"):
            return (0xA000 if op == "bra" else 0xB000) | disp(12, self.target(pc, a[0]) - (pc + 4), 2)
        if op == "mov.w" and a[1].startswith("@(") and a[1].endswith(",gbr)"):  # mov.w r0,@(disp,gbr)
            return 0xC100 | disp(9, int(a[1][2:-5], 0), 2)
        if op == "mov.l" and a[1].startswith("@(") and a[1].endswith(",gbr)"):
            return 0xC200 | disp(10, int(a[1][2:-5], 0), 4)
        if op == "mov.l" and a[0].startswith("@(") and a[0].endswith(")"):  # mov.l @(disp,rm),rn
            d = [x.strip() for x in a[0][2:-1].split(",")]
            return 0x5000 | r(a[1]) << 8 | r(d[1]) << 4 | disp(5, int(d[0], 0), 4)
        if op == "mov.l" and a[1].startswith("@(") and a[1].endswith(")"):  # mov.l rm,@(disp,rn)
            d = [x.strip() for x in a[1][2:-1].split(",")]
            return 0x1000 | r(d[1]) << 8 | r(a[0]) << 4 | disp(5, int(d[0], 0), 4)
        raise ValueError("cannot assemble: " + text)


def boot():
    """Flash at A0000000: the bus, the clock and the SDRAM set as the games set them (the register values the
    CV1000's games write at boot, docs/SLOWDOWN.md), then the tests copied to RAM and run from P1."""
    b = Asm(0xA0000000)
    regs = [  # (GBR = FFFFFF00: offset, value) the watchdog, FRQCR, BCR1, BCR2, WCR1, WCR2, MCR, RTCOR, RTCNT, RTCSR
        (0x86, 0xA500), (0x86, 0xA507), (0x84, 0x5A00), (0x80, 0x0112), (0x60, 0xC008), (0x62, 0x39F0),
        (0x64, 0x9551), (0x66, 0xFDD7), (0x68, 0x543C), (0x72, 0xA560), (0x70, 0xA500), (0x6E, 0xA510)]
    b("mov.l @pc:sp,r15")
    b("mov.l @pc:sr,r0")
    b("ldc r0,sr")
    b("mov.l @pc:gbr,r0")
    b("ldc r0,gbr")
    for i, (offset, _) in enumerate(regs):
        b("mov.l @pc:reg%d,r0" % i)
        b("mov.w r0,@(%d,gbr)" % offset)
    b("mov.l @pc:sdmr,r1")    # (the SDRAM's mode register: a write to area 3's SDMR address)
    b("mov #0,r0")
    b("mov.w r0,@r1")
    b("mov.l @pc:src,r1")     # (copy the tests: flash A0001000 to RAM AC001000)
    b("mov.l @pc:dst,r2")
    b("mov.l @pc:words,r3")
    b.label("copy")
    b("mov.l @r1+,r0")
    b("dt r3")
    b("mov.l r0,@r2")
    b("bf/s copy")
    b("add #4,r2")
    b("mov.l @pc:ccr,r1")     # (CCR: flush, cache on, P1 copy-back)
    b("mov #13,r0")
    b("mov.l r0,@r1")
    b("mov.l @pc:entry,r0")
    b("jmp @r0")
    b("nop")
    b.align(4)
    for name, value in [("sp", 0x0C800000), ("sr", 0x700000F0), ("gbr", 0xFFFFFF00), ("sdmr", 0xFFFFE880),
                        ("src", 0xA0001000), ("dst", 0xAC001000), ("words", 0x1000), ("ccr", 0xFFFFFFEC),
                        ("entry", 0x8C001000)] + [("reg%d" % i, v) for i, (_, v) in enumerate(regs)]:
        b.label(name)
        b.word(value)
    return b.assemble()


def tests():
    t = Asm(0x8C001000)
    t("mov.l @pc:tstr,r11")      # (timer 0: stopped, its constant and count all ones, Pphi/4, started)
    t("mov.l @pc:tcor0,r13")     # (r13: TCOR0; TCNT0 is 4 past it, TCR0 8)
    t("mov #0,r0")
    t("mov.b r0,@r11")
    t("mov #-1,r0")
    t("mov.l r0,@r13")
    t("mov.l r0,@(4,r13)")
    t("mov #0,r0")
    t("mov.l @pc:tcr0,r1")
    t("mov.w r0,@r1")
    t("mov #1,r0")
    t("mov.b r0,@r11")
    t("mov.l @pc:results,r12")
    t("mov.l @pc:cell,r2")       # (a cached word for the load and store tests; its line is brought in by the first)
    t("mov.l @pc:count,r14")
    t("mov.l @pc:done,r6")
    t("mov.l @pc:donecell,r7")
    t("mov #1,r0")
    t("mov #2,r1")
    t("bra start")
    t("nop")
    t.align(4)
    for name, value in [("tstr", 0xFFFFFE92), ("tcor0", 0xFFFFFE94), ("tcr0", 0xFFFFFE9C), ("results", RESULTS),
                        ("cell", 0x8C000F00), ("count", COUNT), ("done", 0xC0DEC0DE), ("donecell", RESULTS + 0xFC)]:
        t.label(name)
        t.word(value)
    t.label("start")
    for k, (name, setup, body) in enumerate(TESTS):
        t.align(16)
        t("mov r14,r8")
        t("mov.l @(4,r13),r9")    # (the count at the start: the timer counts down)
        t.align(16)
        if "odd" in setup:
            t("nop")
        t.label("loop%d" % k)
        for _ in range(REP):
            for insn in body:
                t(insn)
        if "odd" not in setup and any(i.startswith("bra") for i in body):
            t("nop")              # (the same count of instructions as the odd case)
        t("dt r8")
        t("bf/s loop%d" % k)
        t("nop")
        t("mov.l @(4,r13),r10")
        t("sub r10,r9")
        t("mov.l r9,@r12")
        t("add #4,r12")
    t("mov.l r6,@r7")             # (the done mark)
    t.label("forever")
    t("bra forever")
    t("nop")
    return t.assemble()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    image = bytearray(b"\xff" * 0x200000)
    b, t = boot(), tests()
    assert len(b) <= 0x1000 and len(t) <= 0x4000
    image[0:len(b)] = b
    image[0x1000:0x1000 + len(t)] = t
    swapped = bytearray(len(image))  # (the chip's file has each word's bytes swapped)
    swapped[0::2], swapped[1::2] = image[1::2], image[0::2]
    open(sys.argv[1], "wb").write(swapped)
    for k, (name, _, _) in enumerate(TESTS):
        print("%2d %s" % (k, name))


if __name__ == "__main__":
    main()

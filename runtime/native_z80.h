// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp: https://github.com/tmcleroy/cps2recomp
//
// What the PGM sound driver's translated code is written in terms of (tools/recompz80.py writes the code), and the
// tables that say where it is. The scheme is cps2recomp's: routines over the Z80's state, entered at the block the
// PC names, left when control leaves them or time is up; the instructions are calls of what the interpreter is made
// of (machine/z80.h), so the two cannot differ in what an instruction does.
//
// The driver is not in a ROM: the 68000 copies it into the Z80's RAM. So the code comes with the bytes it was made
// from, and runs only while the RAM holds exactly those (the board counts the bytes that differ as they are
// written: Z80::codeDiffers); otherwise the interpreter runs it, whatever the 68000 put there.
#pragma once

#include "../machine/z80.h"

#include <cstddef>
#include <cstdint>

namespace spelunk {

struct Z80Entry {
    uint16_t address;
    Z80Fn fn;
};
struct Z80Range {
    uint16_t start, end;
};
struct Z80Code {
    const Z80Entry *entries;
    size_t entryCount;
    const Z80Range *ranges;
    size_t rangeCount;
    const uint8_t *bytes; // (each range's bytes in turn, as the RAM held them when the code was made)
};
// The sound driver's translated code (generated; an empty set when there is none)
extern const Z80Code kZ80Code;
bool HasZ80Code();
// How many of the bytes the translated code was made from `ram` (the Z80's 64 KB) does not hold
uint32_t Z80CodeDiffers(const uint8_t *ram);
// A byte of the Z80's RAM written (`was` before): keeps z80.codeDiffers, and stops translated code at once when the
// code under it changes
void Z80CodeWritten(Z80 &z80, uint16_t address, uint8_t was, uint8_t value);

namespace z80native {
    constexpr unsigned kMaxDepth = 32;
    [[noreturn]] void NotHere(Z80 &c, uint16_t routine);
} // namespace z80native

} // namespace spelunk

#define ZBLOCK(label, address)                                                                                         \
    label:                                                                                                             \
    if (c.cycles >= c.budget) {                                                                                        \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    }
#define ZLEAVE(address)                                                                                                \
    do {                                                                                                               \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    } while (0)
// (after an instruction that runs on: the time looked at, as the interpreter looks after each instruction)
#define ZNEXT(address)                                                                                                 \
    if (c.cycles >= c.budget) {                                                                                        \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    }
#define ZGOTO(label, address) goto label
#define ZFALL(address)
#define ZCALL(fn, address, label, back)                                                                                \
    do {                                                                                                               \
        c.pc = address;                                                                                                \
        if (c.depth >= spelunk::z80native::kMaxDepth) {                                                                \
            return;                                                                                                    \
        }                                                                                                              \
        c.depth++;                                                                                                     \
        fn(c);                                                                                                         \
        c.depth--;                                                                                                     \
        if (c.pc != back) {                                                                                            \
            return;                                                                                                    \
        }                                                                                                              \
        goto label;                                                                                                    \
    } while (0)

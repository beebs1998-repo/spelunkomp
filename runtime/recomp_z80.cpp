// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp: https://github.com/tmcleroy/cps2recomp
//
// Running the PGM's Z80: the loop around the sound driver's translated code and the interpreter (Z80::Run with a
// mode). It does what Z80::Step does, in the same order, so a run on translated code and one on the interpreter go
// the same way cycle for cycle (tools/checkz80.sh): interrupts are looked at where the interpreter looks, which in
// translated code can only matter after an instruction that leaves it (native_z80.h).
#include "native_z80.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace spelunk {

namespace {
    struct Tables {
        Z80Fn fn[0x10000] = {};
        uint8_t covered[0x10000] = {};
        uint8_t expected[0x10000] = {};
        Tables() {
            for (size_t n = 0; n < kZ80Code.entryCount; n++) {
                fn[kZ80Code.entries[n].address] = kZ80Code.entries[n].fn;
            }
            size_t at = 0;
            for (size_t n = 0; n < kZ80Code.rangeCount; n++) {
                for (unsigned a = kZ80Code.ranges[n].start; a < kZ80Code.ranges[n].end; a++) {
                    covered[a] = 1;
                    expected[a] = kZ80Code.bytes[at++];
                }
            }
        }
    };
    const Tables &GetTables() {
        static const Tables *tables = new Tables;
        return *tables;
    }
} // namespace

bool HasZ80Code() {
    return kZ80Code.entryCount != 0;
}

uint32_t Z80CodeDiffers(const uint8_t *ram) {
    if (!HasZ80Code()) {
        return 1;
    }
    const Tables &t = GetTables();
    uint32_t differ = 0;
    for (unsigned a = 0; a < 0x10000; a++) {
        differ += t.covered[a] && ram[a] != t.expected[a];
    }
    return differ;
}

void Z80CodeWritten(Z80 &z80, uint16_t address, uint8_t was, uint8_t value) {
    const Tables &t = GetTables();
    if (!t.covered[address] || was == value) [[likely]] {
        return;
    }
    z80.codeDiffers += (value != t.expected[address]);
    z80.codeDiffers -= (was != t.expected[address]);
    if (z80.codeDiffers) {
        z80.budget = 0; // (translated code running leaves after this instruction)
    }
}

namespace z80native {
    void NotHere(Z80 &c, uint16_t routine) {
        fprintf(stderr, "translated sound routine %04X entered at %04X, where it has no block\n", routine, c.pc);
        abort();
    }
} // namespace z80native

void Z80::Record(uint16_t address) {
    if (recording && !GetTables().fn[address]) {
        newEntries.push_back(address);
        if (newEntries.size() > 100000) {
            std::sort(newEntries.begin(), newEntries.end());
            newEntries.erase(std::unique(newEntries.begin(), newEntries.end()), newEntries.end());
        }
    }
}

void Z80::Run(uint64_t until, Mode runMode) {
    if (runMode == Mode::Interpreter && !recording) {
        Run(until);
        return;
    }
    const Tables &tables = GetTables();
    while (cycles < until) {
        // (Step's order: the interrupt, the instruction after EI, a halt, then the instruction)
        m_cycles = 0;
        if (const unsigned taken = TakeInterrupt()) {
            cycles += taken;
            interpretedCycles += taken;
            Record(pc);
            continue;
        }
        const bool afterEI = m_afterEI;
        m_afterEI = false;
        if (halted) {
            cycles += 4;
            continue;
        }
        if (runMode == Mode::Native && !afterEI && codeDiffers == 0) {
            if (const Z80Fn fn = tables.fn[pc]) {
                const uint64_t from = cycles;
                budget = until;
                depth = 0;
                fn(*this);
                nativeCycles += cycles - from;
                if (cycles < budget) {
                    Record(pc); // (where it left to: not where time ran out, which can be any instruction)
                }
                continue;
            }
        }
        const uint64_t from = cycles;
        m_jumped = false;
        refresh++;
        Execute(FetchOpcode(), 0);
        cycles += m_cycles;
        interpretedCycles += cycles - from;
        if (m_jumped) {
            Record(pc);
        }
    }
}

} // namespace spelunk

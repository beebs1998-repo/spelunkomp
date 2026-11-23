// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
//
// An ARM7TDMI: the processor inside IGS's IGS027A, the chip on a PGM cartridge that runs a program of its own beside
// the 68000 (machine/pgm.h; DoDonPachi II's has its ROM read out, so its program is run, not imitated).
//
// An interpreter of both instruction sets (ARM, 32 bits an instruction; Thumb, 16), the seven modes with their own
// registers, and the two interrupt lines. Time is counted as the ARM7TDMI's data sheet counts it, a cycle for each
// sequential, non-sequential and internal cycle of an instruction (no wait states: the chip's memories are not known
// to add any). Little-endian. Not done, as the IGS027A's programs use none of it: coprocessors (an instruction for
// one is undefined, as on a chip without any), aborts (no memory here raises one), the 26-bit modes.
//
// Written from the processor's behaviour as ARM documents it (the ARM7TDMI data sheet, ARM DDI 0029); checked by
// tests/arm7_test.cpp and by the game it runs.
#pragma once

#include <cstdint>
#include <cstring>

namespace spelunk {

class Archive;

// What the processor is connected to: 32-bit addresses, little-endian. The addresses given are whole ones (a word's
// low two bits and a halfword's low bit already cleared).
struct Arm7Bus {
    virtual ~Arm7Bus() = default;
    virtual uint32_t ArmRead32(uint32_t address) = 0;
    virtual uint16_t ArmRead16(uint32_t address) = 0;
    virtual uint8_t ArmRead8(uint32_t address) = 0;
    virtual void ArmWrite32(uint32_t address, uint32_t value) = 0;
    virtual void ArmWrite16(uint32_t address, uint16_t value) = 0;
    virtual void ArmWrite8(uint32_t address, uint8_t value) = 0;
};

class Arm7 {
public:
    explicit Arm7(Arm7Bus &bus)
        : bus(bus) {}

    // The modes (the low five bits of the status register)
    enum Mode : uint32_t { kUser = 0x10, kFiq = 0x11, kIrq = 0x12, kSupervisor = 0x13, kAbort = 0x17, kUndefined = 0x1B, kSystem = 0x1F };

    // The reset line: supervisor mode, both interrupts off, ARM instructions from address 0
    void Reset();
    // The interrupt lines (held until dropped): taken before an instruction while the status register lets them
    void SetFiq(bool on) {
        m_fiq = on;
    }
    void SetIrq(bool on) {
        m_irq = on;
    }
    // One instruction (or an interrupt taken); `cycles` goes on by what it took
    void Step();
    // Runs until `cycles` reaches `until` (to the end of the instruction that gets there).
    //
    // A program waiting for an interrupt goes round a loop that changes nothing, and most of this processor's time on
    // the PGM is that. Run sees it: when a branch backwards comes to where the last one came with every register and
    // the status as they were then, and nothing was written and nothing read that anything else can change (`touched`)
    // in between, the same will happen again and again until an interrupt, so the whole rounds that fit before `until`
    // are counted and not run. The state at the end is exactly what running them gives.
    void Run(uint64_t until) {
        while (cycles < until) {
            const uint32_t at = m_next;
            Step();
            if (m_next <= at) {
                Looped(until);
            }
        }
    }
    // Set by the bus at every write, and at every read of something the processor is not alone in changing (memory
    // shared with another, a latch): the loop it happened in is not a wait
    bool touched = false;
    bool skipWaits = true; // (off: every round is run, for checking that the count gives the same)
    uint64_t skipped = 0; // cycles counted and not run (for the tools; not part of the state)

    uint32_t r[16] = {};   // the registers of the mode in use; r[15] while an instruction runs: its address + 8 (Thumb: + 4)
    uint64_t cycles = 0;   // clock cycles since reset
    uint64_t steps = 0;    // instructions run (for the tools)
    // The address of the next instruction
    uint32_t Pc() const {
        return m_next;
    }
    bool Thumb() const {
        return m_thumb;
    }
    uint32_t CurrentMode() const {
        return m_mode;
    }
    bool FiqLine() const {
        return m_fiq;
    }
    // The status register: the flags, the interrupt masks, the instruction set, the mode
    uint32_t Cpsr() const {
        return (m_n << 31) | (m_z << 30) | (m_c << 29) | (m_v << 28) | (m_irqOff << 7) | (m_fiqOff << 6) | (m_thumb << 5) | m_mode;
    }

    // Memory the processor fetches its instructions from directly (and nothing else is read through these): up to
    // two, each `size` bytes at `base`. An instruction anywhere else is read through the bus.
    struct Code {
        const uint8_t *bytes = nullptr;
        uint32_t base = 0, size = 0;
    } code[2];

    Arm7Bus &bus;
    void Serialize(Archive &ar); // save states (runtime/state.h)
    // The state mixed into a hash (the board's StateHash)
    template <typename Mix>
    void Hash(Mix &&mix) const {
        const uint32_t status = Cpsr();
        mix(r, sizeof r);
        mix(&m_next, sizeof m_next);
        mix(&status, sizeof status);
        mix(m_banked, sizeof m_banked);
        mix(m_spsr, sizeof m_spsr);
        mix(&cycles, sizeof cycles);
    }

private:
    uint32_t m_next = 0; // the address of the next instruction
    bool m_n = false, m_z = false, m_c = false, m_v = false;
    bool m_irqOff = true, m_fiqOff = true, m_thumb = false;
    uint32_t m_mode = kSupervisor;
    bool m_fiq = false, m_irq = false;
    // The registers each kind of mode has to itself, while another is in use. Banks: 0 user and system, 1 FIQ, 2 IRQ,
    // 3 supervisor, 4 abort, 5 undefined. [bank][0..6] are r8 to r14 (only FIQ has its own r8 to r12: the others share
    // the user's, kept in bank 0).
    uint32_t m_banked[6][7] = {};
    uint32_t m_spsr[6] = {}; // the saved status register of each bank (bank 0 has none)

    // The last branch backwards: where it came to, and everything as it was then
    uint32_t m_loopAt = ~0u, m_loopStatus = 0, m_loopRegs[15] = {}, m_loopBanked[6][7] = {}, m_loopSpsr[6] = {};
    uint64_t m_loopCycles = 0, m_loopSteps = 0;
    void Looped(uint64_t until);

    static unsigned Bank(uint32_t mode);
    void SwitchMode(uint32_t mode);
    void SetCpsr(uint32_t value);
    void Exception(uint32_t mode, uint32_t vector, uint32_t returnAddress, bool fiqOff);
    void Branch(uint32_t address) {
        m_next = address & (m_thumb ? ~1u : ~3u);
    }
    void SetReg(unsigned n, uint32_t value) {
        if (n == 15) {
            Branch(value);
        } else {
            r[n] = value;
        }
    }
    uint32_t Fetch32(uint32_t address);
    uint32_t Fetch16(uint32_t address);
    uint32_t Load32(uint32_t address);
    bool Condition(unsigned cc) const;
    uint32_t Shifted(unsigned kind, uint32_t value, uint32_t amount, bool immediate, bool &carry) const;
    uint32_t Add(uint32_t a, uint32_t b, uint32_t carryIn, bool flags);
    uint32_t Sub(uint32_t a, uint32_t b, uint32_t carryIn, bool flags);
    void Logic(uint32_t result) {
        m_n = result >> 31;
        m_z = result == 0;
    }
    uint32_t Multiply(uint32_t a, uint32_t b);
    void Arm(uint32_t op);
    void ArmDataProcessing(uint32_t op);
    void ArmBlock(uint32_t op);
    void ThumbOp(uint32_t op);
    void Block(bool load, uint32_t list, uint32_t start, bool userBank, bool restoreStatus);
    void Undefined(uint32_t returnAddress);
};

} // namespace spelunk

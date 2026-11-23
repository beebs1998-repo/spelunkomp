// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The ARM7TDMI's interpreter: see arm7.h.
#include "arm7.h"

#include "../runtime/state.h"

#include <bit>

namespace spelunk {

namespace {
    inline uint32_t Ror(uint32_t value, unsigned by) {
        return std::rotr(value, static_cast<int>(by));
    }
    inline uint32_t SignExtend(uint32_t value, unsigned bits) {
        const uint32_t sign = 1u << (bits - 1);
        return (value ^ sign) - sign;
    }
    // The cycles a multiplication spends on its multiplier: one for each of its bytes, from the low one, until the
    // rest is all zeros (or, where the sign counts, all ones)
    inline unsigned MultiplierCycles(uint32_t multiplier, bool isSigned) {
        for (unsigned m = 1; m < 4; m++) {
            const uint32_t rest = multiplier >> (8 * m);
            if (rest == 0 || (isSigned && rest == (0xFFFFFFFFu >> (8 * m)))) {
                return m;
            }
        }
        return 4;
    }
} // namespace

unsigned Arm7::Bank(uint32_t mode) {
    switch (mode) {
    case kFiq: return 1;
    case kIrq: return 2;
    case kSupervisor: return 3;
    case kAbort: return 4;
    case kUndefined: return 5;
    default: return 0;
    }
}

void Arm7::Reset() {
    std::memset(r, 0, sizeof r);
    std::memset(m_banked, 0, sizeof m_banked);
    std::memset(m_spsr, 0, sizeof m_spsr);
    m_n = m_z = m_c = m_v = false;
    m_irqOff = m_fiqOff = true;
    m_thumb = false;
    m_mode = kSupervisor;
    m_fiq = m_irq = false;
    m_next = 0;
    m_loopAt = ~0u;
}

// The registers of the mode left are put away and the new mode's taken out
void Arm7::SwitchMode(uint32_t mode) {
    const unsigned from = Bank(m_mode), to = Bank(mode);
    m_mode = mode;
    if (from == to) {
        return;
    }
    m_banked[from][5] = r[13];
    m_banked[from][6] = r[14];
    if (from == 1 || to == 1) {
        // (only FIQ has r8 to r12 of its own; every other mode's are the user's)
        const unsigned others = from == 1 ? 1 : 0, own = from == 1 ? 0 : 1;
        for (unsigned n = 0; n < 5; n++) {
            m_banked[others][n] = r[8 + n];
            r[8 + n] = m_banked[own][n];
        }
    }
    r[13] = m_banked[to][5];
    r[14] = m_banked[to][6];
}

void Arm7::SetCpsr(uint32_t value) {
    SwitchMode(value & 0x1F);
    m_n = value >> 31;
    m_z = (value >> 30) & 1;
    m_c = (value >> 29) & 1;
    m_v = (value >> 28) & 1;
    m_irqOff = (value >> 7) & 1;
    m_fiqOff = (value >> 6) & 1;
    m_thumb = (value >> 5) & 1;
}

void Arm7::Exception(uint32_t mode, uint32_t vector, uint32_t returnAddress, bool fiqOff) {
    const uint32_t status = Cpsr();
    SwitchMode(mode);
    m_spsr[Bank(mode)] = status;
    r[14] = returnAddress;
    m_thumb = false;
    m_irqOff = true;
    m_fiqOff = fiqOff;
    m_next = vector;
    cycles += 3;
}

// An instruction that is none (or a coprocessor's, with no coprocessor to take it)
void Arm7::Undefined(uint32_t returnAddress) {
    Exception(kUndefined, 0x04, returnAddress, m_fiqOff);
}

uint32_t Arm7::Fetch32(uint32_t address) {
    for (const Code &c : code) {
        if (address - c.base < c.size) {
            uint32_t value;
            std::memcpy(&value, c.bytes + (address - c.base), 4);
            return value;
        }
    }
    return bus.ArmRead32(address);
}

uint32_t Arm7::Fetch16(uint32_t address) {
    for (const Code &c : code) {
        if (address - c.base < c.size) {
            uint16_t value;
            std::memcpy(&value, c.bytes + (address - c.base), 2);
            return value;
        }
    }
    return bus.ArmRead16(address);
}

// A word loaded: from an address that is not a whole one, the word there turned so the byte addressed is the lowest
uint32_t Arm7::Load32(uint32_t address) {
    const uint32_t value = bus.ArmRead32(address & ~3u);
    return (address & 3) ? Ror(value, (address & 3) * 8) : value;
}

bool Arm7::Condition(unsigned cc) const {
    switch (cc) {
    case 0: return m_z;
    case 1: return !m_z;
    case 2: return m_c;
    case 3: return !m_c;
    case 4: return m_n;
    case 5: return !m_n;
    case 6: return m_v;
    case 7: return !m_v;
    case 8: return m_c && !m_z;
    case 9: return !m_c || m_z;
    case 10: return m_n == m_v;
    case 11: return m_n != m_v;
    case 12: return !m_z && m_n == m_v;
    case 13: return m_z || m_n != m_v;
    case 14: return true;
    default: return false;
    }
}

// The barrel shifter. kind: 0 left, 1 right, 2 right keeping the sign, 3 rotated right. `immediate`: the amount is
// the instruction's own 5 bits (0 then means 32, or for a rotation one place through the carry); otherwise a
// register's low byte (0: nothing is done). `carry` comes in as the C flag and goes out as the last bit shifted out.
uint32_t Arm7::Shifted(unsigned kind, uint32_t value, uint32_t amount, bool immediate, bool &carry) const {
    if (amount == 0) {
        if (!immediate || kind == 0) {
            return value;
        }
        if (kind == 3) {
            const uint32_t result = (static_cast<uint32_t>(carry) << 31) | (value >> 1);
            carry = value & 1;
            return result;
        }
        amount = 32;
    }
    switch (kind) {
    case 0:
        if (amount < 32) {
            carry = (value >> (32 - amount)) & 1;
            return value << amount;
        }
        carry = amount == 32 ? (value & 1) : false;
        return 0;
    case 1:
        if (amount < 32) {
            carry = (value >> (amount - 1)) & 1;
            return value >> amount;
        }
        carry = amount == 32 ? (value >> 31) : false;
        return 0;
    case 2:
        if (amount < 32) {
            carry = (value >> (amount - 1)) & 1;
            return static_cast<uint32_t>(static_cast<int32_t>(value) >> amount);
        }
        carry = value >> 31;
        return carry ? 0xFFFFFFFFu : 0;
    default:
        amount &= 31;
        if (amount == 0) {
            carry = value >> 31;
            return value;
        }
        carry = (value >> (amount - 1)) & 1;
        return Ror(value, amount);
    }
}

uint32_t Arm7::Add(uint32_t a, uint32_t b, uint32_t carryIn, bool flags) {
    const uint64_t sum = static_cast<uint64_t>(a) + b + carryIn;
    const uint32_t result = static_cast<uint32_t>(sum);
    if (flags) {
        m_n = result >> 31;
        m_z = result == 0;
        m_c = sum >> 32;
        m_v = (~(a ^ b) & (a ^ result)) >> 31;
    }
    return result;
}

// a - b - (1 - carryIn): the carry out is set when there was no borrow
uint32_t Arm7::Sub(uint32_t a, uint32_t b, uint32_t carryIn, bool flags) {
    return Add(a, ~b, carryIn, flags);
}

// A branch backwards was taken (Run): the rounds of a loop that changes nothing counted, or this round remembered
void Arm7::Looped(uint64_t until) {
    const uint32_t status = Cpsr();
    if (m_next == m_loopAt && !touched && status == m_loopStatus && !std::memcmp(r, m_loopRegs, sizeof m_loopRegs) &&
        !std::memcmp(m_banked, m_loopBanked, sizeof m_banked) && !std::memcmp(m_spsr, m_loopSpsr, sizeof m_spsr)) {
        // (an interrupt line that is up is masked, or the loop would not have come round; it stays masked, the status
        // being the same each round)
        const uint64_t period = cycles - m_loopCycles, steppedIn = steps - m_loopSteps;
        if (skipWaits && period && cycles < until && until - cycles > period) {
            const uint64_t rounds = (until - cycles - 1) / period;
            cycles += rounds * period;
            steps += rounds * steppedIn;
            skipped += rounds * period;
        }
    } else {
        m_loopAt = m_next;
        m_loopStatus = status;
        std::memcpy(m_loopRegs, r, sizeof m_loopRegs);
        std::memcpy(m_loopBanked, m_banked, sizeof m_banked);
        std::memcpy(m_loopSpsr, m_spsr, sizeof m_spsr);
    }
    touched = false;
    m_loopCycles = cycles;
    m_loopSteps = steps;
}

void Arm7::Step() {
    if (m_fiq && !m_fiqOff) {
        Exception(kFiq, 0x1C, m_next + 4, true);
        return;
    }
    if (m_irq && !m_irqOff) {
        Exception(kIrq, 0x18, m_next + 4, m_fiqOff);
        return;
    }
    steps++;
    const uint32_t at = m_next;
    if (m_thumb) {
        m_next = at + 2;
        r[15] = at + 4;
        ThumbOp(Fetch16(at));
        return;
    }
    m_next = at + 4;
    r[15] = at + 8;
    const uint32_t op = Fetch32(at);
    if ((op >> 28) != 14 && !Condition(op >> 28)) {
        cycles += 1;
        return;
    }
    Arm(op);
}

// ---- ARM instructions ----

void Arm7::ArmDataProcessing(uint32_t op) {
    const unsigned opcode = (op >> 21) & 15, rn = (op >> 16) & 15, rd = (op >> 12) & 15;
    const bool s = op & 0x00100000;
    bool carry = m_c;
    uint32_t a = r[rn], b;
    unsigned time = 1;
    if (op & 0x02000000) {
        const unsigned rot = (op >> 7) & 30;
        b = op & 0xFF;
        if (rot) {
            b = Ror(b, rot);
            carry = b >> 31;
        }
    } else {
        uint32_t rm = r[op & 15];
        if (op & 0x10) {
            // (shifted by a register: the PC is read a word further on)
            if ((op & 15) == 15) {
                rm += 4;
            }
            if (rn == 15) {
                a += 4;
            }
            b = Shifted((op >> 5) & 3, rm, r[(op >> 8) & 15] & 0xFF, false, carry);
            time++;
        } else {
            b = Shifted((op >> 5) & 3, rm, (op >> 7) & 31, true, carry);
        }
    }
    // (with the PC as the destination, S means "and the status register from the saved one": the flags are not set here)
    const bool toPc = rd == 15 && opcode != 8 && opcode != 9 && opcode != 10 && opcode != 11;
    const bool flags = s && !toPc;
    uint32_t result = 0;
    bool logic = false, keep = true;
    switch (opcode) {
    case 0: result = a & b; logic = true; break;                    // AND
    case 1: result = a ^ b; logic = true; break;                    // EOR
    case 2: result = Sub(a, b, 1, flags); break;                    // SUB
    case 3: result = Sub(b, a, 1, flags); break;                    // RSB
    case 4: result = Add(a, b, 0, flags); break;                    // ADD
    case 5: result = Add(a, b, m_c, flags); break;                  // ADC
    case 6: result = Sub(a, b, m_c, flags); break;                  // SBC
    case 7: result = Sub(b, a, m_c, flags); break;                  // RSC
    case 8: result = a & b; logic = true; keep = false; break;      // TST
    case 9: result = a ^ b; logic = true; keep = false; break;      // TEQ
    case 10: Sub(a, b, 1, true); keep = false; break;               // CMP
    case 11: Add(a, b, 0, true); keep = false; break;               // CMN
    case 12: result = a | b; logic = true; break;                   // ORR
    case 13: result = b; logic = true; break;                       // MOV
    case 14: result = a & ~b; logic = true; break;                  // BIC
    default: result = ~b; logic = true; break;                      // MVN
    }
    if (logic && (flags || !keep)) {
        Logic(result);
        m_c = carry;
    }
    if (keep) {
        if (toPc) {
            if (s) {
                SetCpsr(m_spsr[Bank(m_mode)]);
            }
            Branch(result);
            time += 2;
        } else {
            r[rd] = result;
        }
    }
    cycles += time;
}

// A block of registers loaded or stored (LDM, STM; Thumb's PUSH, POP, LDMIA, STMIA): `list` has a bit for each, the
// lowest register at the lowest address, from `start` up
void Arm7::Block(bool load, uint32_t list, uint32_t start, bool userBank, bool restoreStatus) {
    const uint32_t mode = m_mode;
    if (userBank) {
        SwitchMode(kUser);
    }
    uint32_t address = start & ~3u;
    unsigned count = 0;
    for (unsigned n = 0; n < 16; n++) {
        if (!(list & (1u << n))) {
            continue;
        }
        count++;
        if (load) {
            const uint32_t value = bus.ArmRead32(address);
            if (n == 15) {
                if (restoreStatus) {
                    SetCpsr(m_spsr[Bank(m_mode)]);
                }
                Branch(value);
            } else {
                r[n] = value;
            }
        } else {
            // (the PC is stored a word further on)
            bus.ArmWrite32(address, n == 15 ? r[15] + 4 : r[n]);
        }
        address += 4;
    }
    if (userBank) {
        SwitchMode(mode);
    }
    cycles += load ? count + 2 + ((list & 0x8000) ? 2 : 0) : count + 1;
}

void Arm7::ArmBlock(uint32_t op) {
    const bool pre = op & 0x01000000, up = op & 0x00800000, s = op & 0x00400000, writeBack = op & 0x00200000, load = op & 0x00100000;
    const unsigned rn = (op >> 16) & 15;
    const uint32_t list = op & 0xFFFF;
    const uint32_t size = 4 * static_cast<uint32_t>(std::popcount(list));
    const uint32_t base = r[rn];
    uint32_t start = up ? base : base - size;
    if (pre == up) {
        start += 4; // (before going up, and after going down, the lowest word is one on)
    }
    const bool restore = s && load && (list & 0x8000);
    Block(load, list, start, s && !restore, restore);
    // (a base that was loaded keeps what was loaded)
    if (writeBack && !(load && (list & (1u << rn)))) {
        r[rn] = up ? base + size : base - size;
    }
}

void Arm7::Arm(uint32_t op) {
    switch ((op >> 25) & 7) {
    case 0:
        if ((op & 0x0FFFFFF0) == 0x012FFF10) {
            // BX: to the address in a register, its low bit choosing the instruction set
            const uint32_t to = r[op & 15];
            m_thumb = to & 1;
            Branch(to);
            cycles += 3;
            return;
        }
        if ((op & 0x0FC000F0) == 0x00000090) {
            // MUL, MLA
            const unsigned rd = (op >> 16) & 15, rn = (op >> 12) & 15, rs = (op >> 8) & 15, rm = op & 15;
            uint32_t result = r[rm] * r[rs];
            unsigned time = 1 + MultiplierCycles(r[rs], true);
            if (op & 0x00200000) {
                result += r[rn];
                time++;
            }
            if (op & 0x00100000) {
                Logic(result);
            }
            r[rd] = result;
            cycles += time;
            return;
        }
        if ((op & 0x0F8000F0) == 0x00800090) {
            // UMULL, UMLAL, SMULL, SMLAL: 64 bits into two registers
            const unsigned high = (op >> 16) & 15, low = (op >> 12) & 15, rs = (op >> 8) & 15, rm = op & 15;
            const bool isSigned = op & 0x00400000;
            uint64_t result = isSigned ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(r[rm])) * static_cast<int32_t>(r[rs]))
                                       : static_cast<uint64_t>(r[rm]) * r[rs];
            unsigned time = 2 + MultiplierCycles(r[rs], isSigned);
            if (op & 0x00200000) {
                result += (static_cast<uint64_t>(r[high]) << 32) | r[low];
                time++;
            }
            if (op & 0x00100000) {
                m_n = result >> 63;
                m_z = result == 0;
            }
            r[low] = static_cast<uint32_t>(result);
            r[high] = static_cast<uint32_t>(result >> 32);
            cycles += time;
            return;
        }
        if ((op & 0x0FB00FF0) == 0x01000090) {
            // SWP: a register and memory exchanged
            const unsigned rn = (op >> 16) & 15, rd = (op >> 12) & 15, rm = op & 15;
            const uint32_t address = r[rn], value = r[rm];
            if (op & 0x00400000) {
                const uint32_t was = bus.ArmRead8(address);
                bus.ArmWrite8(address, static_cast<uint8_t>(value));
                r[rd] = was;
            } else {
                const uint32_t was = Load32(address);
                bus.ArmWrite32(address & ~3u, value);
                r[rd] = was;
            }
            cycles += 4;
            return;
        }
        if ((op & 0x00000090) == 0x00000090 && (op & 0x60)) {
            // LDRH, STRH, LDRSB, LDRSH
            const bool pre = op & 0x01000000, up = op & 0x00800000, writeBack = op & 0x00200000, load = op & 0x00100000;
            const unsigned rn = (op >> 16) & 15, rd = (op >> 12) & 15, kind = (op >> 5) & 3;
            const uint32_t offset = (op & 0x00400000) ? ((op >> 4) & 0xF0) | (op & 15) : r[op & 15];
            const uint32_t base = r[rn], moved = up ? base + offset : base - offset, address = pre ? moved : base;
            if (load) {
                uint32_t value;
                if (kind == 1) {
                    value = bus.ArmRead16(address & ~1u);
                } else if (kind == 2) {
                    value = SignExtend(bus.ArmRead8(address), 8);
                } else {
                    value = SignExtend(bus.ArmRead16(address & ~1u), 16);
                }
                if ((!pre || writeBack) && rn != 15) {
                    r[rn] = moved;
                }
                SetReg(rd, value);
                cycles += rd == 15 ? 5 : 3;
            } else {
                bus.ArmWrite16(address & ~1u, static_cast<uint16_t>(rd == 15 ? r[15] + 4 : r[rd]));
                if ((!pre || writeBack) && rn != 15) {
                    r[rn] = moved;
                }
                cycles += 2;
            }
            return;
        }
        if ((op & 0x0FBF0FFF) == 0x010F0000) {
            // MRS: the status register (or the mode's saved one) into a register
            r[(op >> 12) & 15] = (op & 0x00400000) ? m_spsr[Bank(m_mode)] : Cpsr();
            cycles += 1;
            return;
        }
        [[fallthrough]];
    case 1:
        if ((op & 0x0DB0F000) == 0x0120F000 && ((op & 0x02000000) || !(op & 0x00000FF0))) {
            // MSR: the status register's control byte and flags byte from a register or a constant, each if asked for
            const uint32_t value = (op & 0x02000000) ? Ror(op & 0xFF, (op >> 7) & 30) : r[op & 15];
            uint32_t mask = ((op & 0x00010000) ? 0x000000FFu : 0) | ((op & 0x00080000) ? 0xFF000000u : 0);
            if (op & 0x00400000) {
                uint32_t &saved = m_spsr[Bank(m_mode)];
                saved = (saved & ~mask) | (value & mask);
            } else {
                // (the instruction set's bit is not changed this way; user mode changes only the flags)
                mask &= m_mode == kUser ? 0xFF000000u : ~0x20u;
                SetCpsr((Cpsr() & ~mask) | (value & mask));
            }
            cycles += 1;
            return;
        }
        ArmDataProcessing(op);
        return;
    case 2:
    case 3: {
        if ((op & 0x02000010) == 0x02000010) {
            Undefined(m_next);
            return;
        }
        // LDR, STR, LDRB, STRB
        const bool pre = op & 0x01000000, up = op & 0x00800000, byte = op & 0x00400000, writeBack = op & 0x00200000, load = op & 0x00100000;
        const unsigned rn = (op >> 16) & 15, rd = (op >> 12) & 15;
        uint32_t offset = op & 0xFFF;
        if (op & 0x02000000) {
            bool carry = m_c;
            offset = Shifted((op >> 5) & 3, r[op & 15], (op >> 7) & 31, true, carry);
        }
        const uint32_t base = r[rn], moved = up ? base + offset : base - offset, address = pre ? moved : base;
        if (load) {
            const uint32_t value = byte ? bus.ArmRead8(address) : Load32(address);
            if ((!pre || writeBack) && rn != 15) {
                r[rn] = moved;
            }
            SetReg(rd, value); // (a base that is also the destination keeps what was loaded)
            cycles += rd == 15 ? 5 : 3;
        } else {
            const uint32_t value = rd == 15 ? r[15] + 4 : r[rd];
            if (byte) {
                bus.ArmWrite8(address, static_cast<uint8_t>(value));
            } else {
                bus.ArmWrite32(address & ~3u, value);
            }
            if ((!pre || writeBack) && rn != 15) {
                r[rn] = moved;
            }
            cycles += 2;
        }
        return;
    }
    case 4:
        ArmBlock(op);
        return;
    case 5: {
        // B, BL
        if (op & 0x01000000) {
            r[14] = m_next;
        }
        Branch(r[15] + (SignExtend(op & 0x00FFFFFF, 24) << 2));
        cycles += 3;
        return;
    }
    case 6:
        Undefined(m_next);
        return;
    default:
        if (op & 0x01000000) {
            // SWI
            Exception(kSupervisor, 0x08, m_next, m_fiqOff);
        } else {
            Undefined(m_next);
        }
        return;
    }
}

// ---- Thumb instructions ----

void Arm7::ThumbOp(uint32_t op) {
    const unsigned rd = op & 7, rs = (op >> 3) & 7;
    switch (op >> 13) {
    case 0: {
        if (((op >> 11) & 3) == 3) {
            // ADD, SUB of a register or a constant of 3 bits
            const uint32_t b = (op & 0x0400) ? (op >> 6) & 7 : r[(op >> 6) & 7];
            r[rd] = (op & 0x0200) ? Sub(r[rs], b, 1, true) : Add(r[rs], b, 0, true);
        } else {
            // LSL, LSR, ASR by a constant
            bool carry = m_c;
            r[rd] = Shifted((op >> 11) & 3, r[rs], (op >> 6) & 31, true, carry);
            Logic(r[rd]);
            m_c = carry;
        }
        cycles += 1;
        return;
    }
    case 1: {
        // MOV, CMP, ADD, SUB with a constant of 8 bits
        const unsigned reg = (op >> 8) & 7;
        const uint32_t value = op & 0xFF;
        switch ((op >> 11) & 3) {
        case 0:
            r[reg] = value;
            Logic(value);
            break;
        case 1: Sub(r[reg], value, 1, true); break;
        case 2: r[reg] = Add(r[reg], value, 0, true); break;
        default: r[reg] = Sub(r[reg], value, 1, true); break;
        }
        cycles += 1;
        return;
    }
    case 2: {
        if ((op & 0x1C00) == 0x0000) {
            // The sixteen operations on two low registers
            const uint32_t a = r[rd], b = r[rs];
            unsigned time = 1;
            bool carry = m_c;
            switch ((op >> 6) & 15) {
            case 0: Logic(r[rd] = a & b); break;                                        // AND
            case 1: Logic(r[rd] = a ^ b); break;                                        // EOR
            case 2: case 3: case 4:                                                     // LSL, LSR, ASR
                Logic(r[rd] = Shifted(((op >> 6) & 15) - 2, a, b & 0xFF, false, carry));
                m_c = carry;
                time = 2;
                break;
            case 5: r[rd] = Add(a, b, m_c, true); break;                                // ADC
            case 6: r[rd] = Sub(a, b, m_c, true); break;                                // SBC
            case 7:                                                                     // ROR
                Logic(r[rd] = Shifted(3, a, b & 0xFF, false, carry));
                m_c = carry;
                time = 2;
                break;
            case 8: Logic(a & b); break;                                                // TST
            case 9: r[rd] = Sub(0, b, 1, true); break;                                  // NEG
            case 10: Sub(a, b, 1, true); break;                                         // CMP
            case 11: Add(a, b, 0, true); break;                                         // CMN
            case 12: Logic(r[rd] = a | b); break;                                       // ORR
            case 13:                                                                    // MUL
                Logic(r[rd] = a * b);
                time = 1 + MultiplierCycles(a, true);
                break;
            case 14: Logic(r[rd] = a & ~b); break;                                      // BIC
            default: Logic(r[rd] = ~b); break;                                          // MVN
            }
            cycles += time;
            return;
        }
        if ((op & 0x1C00) == 0x0400) {
            // ADD, CMP, MOV with the high registers; BX
            const unsigned to = rd | ((op >> 4) & 8), from = (op >> 3) & 15;
            const uint32_t value = r[from];
            switch ((op >> 8) & 3) {
            case 0:
                SetReg(to, r[to] + value);
                cycles += to == 15 ? 3 : 1;
                break;
            case 1:
                Sub(r[to], value, 1, true);
                cycles += 1;
                break;
            case 2:
                SetReg(to, value);
                cycles += to == 15 ? 3 : 1;
                break;
            default:
                m_thumb = value & 1;
                Branch(value);
                cycles += 3;
                break;
            }
            return;
        }
        if ((op & 0x1800) == 0x0800) {
            // LDR from the constants after the code (the PC's word, then 8 bits of words on)
            r[(op >> 8) & 7] = bus.ArmRead32((r[15] & ~2u) + ((op & 0xFF) << 2));
            cycles += 3;
            return;
        }
        // Loads and stores at a base register plus a register
        const uint32_t address = r[rs] + r[(op >> 6) & 7];
        switch ((op >> 9) & 7) {
        case 0: bus.ArmWrite32(address & ~3u, r[rd]); break;                            // STR
        case 1: bus.ArmWrite16(address & ~1u, static_cast<uint16_t>(r[rd])); break;     // STRH
        case 2: bus.ArmWrite8(address, static_cast<uint8_t>(r[rd])); break;             // STRB
        case 3: r[rd] = SignExtend(bus.ArmRead8(address), 8); break;                    // LDSB
        case 4: r[rd] = Load32(address); break;                                         // LDR
        case 5: r[rd] = bus.ArmRead16(address & ~1u); break;                            // LDRH
        case 6: r[rd] = bus.ArmRead8(address); break;                                   // LDRB
        default: r[rd] = SignExtend(bus.ArmRead16(address & ~1u), 16); break;           // LDSH
        }
        cycles += ((op >> 9) & 7) < 3 ? 2 : 3;
        return;
    }
    case 3: {
        // LDR, STR, LDRB, STRB at a base register plus a constant of 5 bits
        const bool byte = op & 0x1000, load = op & 0x0800;
        const uint32_t address = r[rs] + (byte ? (op >> 6) & 31 : ((op >> 6) & 31) << 2);
        if (load) {
            r[rd] = byte ? bus.ArmRead8(address) : Load32(address);
            cycles += 3;
        } else {
            if (byte) {
                bus.ArmWrite8(address, static_cast<uint8_t>(r[rd]));
            } else {
                bus.ArmWrite32(address & ~3u, r[rd]);
            }
            cycles += 2;
        }
        return;
    }
    case 4: {
        const bool load = op & 0x0800;
        if (!(op & 0x1000)) {
            // LDRH, STRH at a base register plus a constant
            const uint32_t address = (r[rs] + (((op >> 6) & 31) << 1)) & ~1u;
            if (load) {
                r[rd] = bus.ArmRead16(address);
            } else {
                bus.ArmWrite16(address, static_cast<uint16_t>(r[rd]));
            }
        } else {
            // LDR, STR at the stack pointer plus a constant
            const unsigned reg = (op >> 8) & 7;
            const uint32_t address = r[13] + ((op & 0xFF) << 2);
            if (load) {
                r[reg] = Load32(address);
            } else {
                bus.ArmWrite32(address & ~3u, r[reg]);
            }
        }
        cycles += load ? 3 : 2;
        return;
    }
    case 5: {
        if (!(op & 0x1000)) {
            // An address: the PC's word or the stack pointer, plus a constant
            r[(op >> 8) & 7] = ((op & 0x0800) ? r[13] : (r[15] & ~2u)) + ((op & 0xFF) << 2);
            cycles += 1;
            return;
        }
        if ((op & 0x0F00) == 0x0000) {
            // The stack pointer moved by a constant
            const uint32_t by = (op & 0x7F) << 2;
            r[13] = (op & 0x80) ? r[13] - by : r[13] + by;
            cycles += 1;
            return;
        }
        if ((op & 0x0600) == 0x0400) {
            // PUSH (with the link register), POP (with the PC)
            const uint32_t list = op & 0xFF;
            const uint32_t size = 4 * static_cast<uint32_t>(std::popcount(list) + ((op >> 8) & 1));
            if (op & 0x0800) {
                const uint32_t from = r[13];
                r[13] = from + size;
                Block(true, list | ((op & 0x0100) ? 0x8000 : 0), from, false, false);
            } else {
                r[13] -= size;
                Block(false, list | ((op & 0x0100) ? 0x4000 : 0), r[13], false, false);
            }
            return;
        }
        Undefined(m_next);
        return;
    }
    case 6: {
        if (!(op & 0x1000)) {
            // LDMIA, STMIA: low registers at a base, which moves on
            const unsigned base = (op >> 8) & 7;
            const uint32_t list = op & 0xFF, from = r[base];
            const bool load = op & 0x0800;
            Block(load, list, from, false, false);
            if (!(load && (list & (1u << base)))) {
                r[base] = from + 4 * static_cast<uint32_t>(std::popcount(list));
            }
            return;
        }
        const unsigned cc = (op >> 8) & 15;
        if (cc == 15) {
            // SWI
            Exception(kSupervisor, 0x08, m_next, m_fiqOff);
            return;
        }
        if (cc == 14) {
            Undefined(m_next);
            return;
        }
        // A branch on a condition
        if (Condition(cc)) {
            Branch(r[15] + (SignExtend(op & 0xFF, 8) << 1));
            cycles += 3;
        } else {
            cycles += 1;
        }
        return;
    }
    default:
        switch ((op >> 11) & 3) {
        case 0: // B
            Branch(r[15] + (SignExtend(op & 0x7FF, 11) << 1));
            cycles += 3;
            return;
        case 2: // BL's first half: the high part of the way there, kept in the link register
            r[14] = r[15] + (SignExtend(op & 0x7FF, 11) << 12);
            cycles += 1;
            return;
        case 3: { // BL's second half
            const uint32_t to = r[14] + ((op & 0x7FF) << 1);
            r[14] = m_next | 1;
            Branch(to);
            cycles += 3;
            return;
        }
        default:
            Undefined(m_next);
            return;
        }
    }
}

void Arm7::Serialize(Archive &ar) {
    ar.Mark(0x41524D37);
    ar.Pod(r);
    ar.Pod(m_next);
    ar.Pod(m_n);
    ar.Pod(m_z);
    ar.Pod(m_c);
    ar.Pod(m_v);
    ar.Pod(m_irqOff);
    ar.Pod(m_fiqOff);
    ar.Pod(m_thumb);
    ar.Pod(m_mode);
    ar.Pod(m_fiq);
    ar.Pod(m_irq);
    ar.Pod(m_banked);
    ar.Pod(m_spsr);
    ar.Pod(cycles);
    ar.Pod(steps);
    if (!ar.Saving()) {
        m_loopAt = ~0u; // (what Run remembers of the last loop is only a short cut: begun again)
    }
}

} // namespace spelunk

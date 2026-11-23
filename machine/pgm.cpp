// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// As MAME's pgm.cpp, pgmprot_igs027a_type1.cpp and pgmprot_igs027a_type2.cpp describe the board (BSD-3-Clause, David
// Haywood, ElSemi, Xing Xing): see pgm.h.
#include "pgm.h"

#include "../runtime/state.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace spelunk {

namespace {
    // The sprite DMA begins at the horizontal sync (63 of the line's 640 pixel clocks, two CPU cycles a clock) of the
    // frame's line 261, three lines before the blank: the IGS023 starts it when its line counter, cleared as the
    // picture begins and counted at each sync, reads 221 (as measured on the PGM core for MiSTer; it was line 260's
    // 511th clock)
    constexpr uint64_t kDmaAt = (261 * 640 + 63) * 2;
    constexpr uint64_t kIrq4Period = 62 * Pgm::kLineCycles;
    // (a run ends exactly when an interrupt or the DMA comes this soon after its end: more than translated code's
    // longest block takes)
    constexpr uint64_t kEventNear = 8192;
    // (the IGS023 counts its lines for the level-4 interrupt at each line's horizontal sync, 63 of its pixels, 126
    // cycles, into the line: the first comes 62 lines and that much after the reset, as in the PGM core for MiSTer)
    constexpr uint64_t kIrq4First = kIrq4Period + 126;
    // The Z80's clock in the CPU's cycles; the ICS2115's is four times the Z80's. The board's sound clock is
    // 33.8656 MHz as measured on a board (the PGM core for MiSTer: 50 MHz x 615 / 908), the Z80 a quarter of it, the
    // CPU 20 MHz (50 MHz x 2 / 5): 3,075 / 7,264. (It was 8.4672 MHz, MAME's: 1,323 / 3,125)
    uint64_t Z80Cycles(uint64_t cpuCycles) {
        return cpuCycles * 3075 / 7264;
    }

    // Days since 2000-01-01 of a date (2000 to 2099), and back
    int64_t Days(int year, int month, int day) {
        static constexpr int kBefore[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int64_t d = (year - 2000) * 365 + (year - 1997) / 4 + kBefore[(month - 1) % 12] + day - 1;
        if (month > 2 && year % 4 == 0) {
            d++;
        }
        return d;
    }
    uint8_t Bcd(int v) {
        return static_cast<uint8_t>((v / 10) << 4 | (v % 10));
    }
    int FromBcd(uint8_t v) {
        return (v >> 4) * 10 + (v & 15);
    }
} // namespace

Pgm::Pgm()
    : cpu(*this)
    , z80(*this)
    , arm(*this)
    , ram(0x20000, 0)
    , program(0x400000, 0)
    , z80Ram(0x10000, 0)
    , soundRom(0x1000000, 0)
    , m_picture(static_cast<size_t>(Igs023::kWidth) * Igs023::kHeight, 0xFF000000u) {
    cpu.ram = ram.data();
    cpu.ramBase = 0x800000;
    cpu.ramSize = 0x20000;
    cpu.exactTail = 8192; // (M68K::exactTail)
    // (a time to start from: the start of 2003)
    m_clock.secondsAtReset = Days(2003, 1, 1) * 86400;
}

void Pgm::Reset() {
    cpu.rom = program.data();
    cpu.romSize = static_cast<uint32_t>(program.size());
    cpu.cycles = 0;
    cpu.Reset(true);
    z80.Reset();
    z80.cycles = 0;
    z80.codeDiffers = Z80CodeDiffers(z80Ram.data());
    m_z80Running = false; // (held until the 68000 lets it go)
    sound.rom = soundRom.data();
    sound.romSize = static_cast<uint32_t>(soundRom.size());
    sound.Reset();
    std::memset(m_latch, 0, sizeof m_latch);
    m_prot = Protection{};
    if (hasArm) {
        armRam.assign(0x10000, 0);
        armRamLow.assign(0x400, 0);
        armShared.assign(0x10000, 0);
        std::memset(m_xorTable, 0, sizeof m_xorTable);
        m_armCode = armExternal;
        m_latch68k = m_latchArm = m_latchSeen = 0;
        m_latchLaterCount = 0;
        arm.code[0] = {armRom.data(), 0, static_cast<uint32_t>(armRom.size())};
        arm.code[1] = {m_armCode.data(), 0x08000000, static_cast<uint32_t>(m_armCode.size())};
        arm.Reset();
        arm.cycles = 0;
    }
    video.ctrl &= ~0x000C;
    m_irq4 = m_irq6 = false;
    m_frameStart = 0;
    m_nextIrq4 = kIrq4First;
    frame = 0;
    const int64_t seconds = m_clock.secondsAtReset;
    m_clock = Clock{};
    m_clock.secondsAtReset = seconds;
    UpdateIrq();
}

void Pgm::UpdateIrq(uint64_t at) {
    cpu.SetInterruptLevel(m_irq6 ? 6 : m_irq4 ? 4 : 0, at);
}

uint16_t Pgm::Line() const {
    // (the frame begins at the blank, line 224)
    return static_cast<uint16_t>((224 + (cpu.cycles - m_frameStart) / kLineCycles) % 264);
}

// The board's waits on the CPU (SetBoardTiming): the IGS023's, as a real board has them (MiSTer-devel's
// Arcade-IGSPGM_MiSTer, util/vram_bench/results/LA_FINDINGS.md: logic-analyser captures of a board, and its test ROMs'
// results run on one; the facts only). A write to the chip's registers or the palette waits one cycle, a read none
// (as the core has them; the board's test ROMs do not measure them). The video memory: every access, read or write,
// byte or word, waits 2 cycles. On each line of the picture the chip takes that memory for itself first: its text
// layer's row for the next line and the background's first columns, from 6 cycles before the line to 362 into it
// (11.8 us after the horizontal sync), shorter by 2 cycles for each pixel the background is scrolled within its
// 32-pixel tile; an access begun then waits to its end. From there to the next line's, the chip's 800-ns microcycle,
// counted from that end: its first half the background's, the second the CPU's. An access begun in the second half
// (or at the cycle's very start, a new one waiting at the boundary) goes on; one begun in the first waits for the
// second. Unless the game has given the CPU the memory (the control register's bit 10): then only the 2 cycles.
// docs/PGM.md has the board's results against this model's.
uint32_t Pgm::AccessWait(const void *board, uint32_t address, uint64_t cycles, bool write) {
    const Pgm &pgm = *static_cast<const Pgm *>(board);
    if (address < 0x900000 || address >= 0xC00000) {
        return 0;
    }
    if (address >= 0xA00000) {
        return write ? 1 : 0;
    }
    if (pgm.video.ctrl & 0x400) {
        return kVideoMemoryWait;
    }
    // (the frame's lines 40 to 263 are the picture's; the blank is 0 to 39)
    const int64_t t = static_cast<int64_t>((cycles + kFrameCycles - pgm.m_frameStart) % kFrameCycles);
    const int64_t line = t / static_cast<int64_t>(kLineCycles), into = t % static_cast<int64_t>(kLineCycles);
    // The line whose fetch is under way or was the last: none in the blank
    int64_t fetched = -1;
    if (into >= static_cast<int64_t>(kLineCycles - kFetchBefore) && line + 1 >= 40 && line + 1 < 264) {
        fetched = line + 1;
    } else if (line >= 40 && line < 264) {
        fetched = line;
    }
    if (fetched < 0) {
        return kVideoMemoryWait;
    }
    auto fetchEnd = [&](int64_t l) {
        const unsigned y = static_cast<unsigned>(l - 40) & 0x7FF;
        return l * static_cast<int64_t>(kLineCycles) + static_cast<int64_t>(kFetchEnd) -
               2 * ((pgm.video.bgX + pgm.video.rowScroll[y]) & 31);
    };
    const int64_t end = fetchEnd(fetched);
    if (t < end) {
        if (t >= fetched * static_cast<int64_t>(kLineCycles) - static_cast<int64_t>(kFetchBefore)) {
            return static_cast<uint32_t>(end - t) + kVideoMemoryWait;
        }
        // (before this line's fetch: in the last line's microcycles; the first picture line has none before it)
        if (fetched == 40) {
            return kVideoMemoryWait;
        }
    }
    const int64_t phase = (t - (t >= end ? end : fetchEnd(fetched - 1))) % 16;
    if (phase == 0 || phase >= 8) {
        return kVideoMemoryWait;
    }
    return static_cast<uint32_t>(8 - phase) + kVideoMemoryWait;
}

// ---- The sound side ----

// The Z80 and the ICS2115 run on to the CPU's time, the Z80 a sample's time at a go (the chip's timers and interrupts
// are seen within one)
void Pgm::RunSound(uint64_t cpuCycles) {
    const uint64_t to = Z80Cycles(cpuCycles);
    while (z80.cycles < to) {
        const uint64_t slice = std::min(to, z80.cycles + 256);
        if (m_z80Running) {
            z80.Run(slice, z80Mode);
        } else {
            z80.cycles = slice;
        }
        SoundCaughtUp();
    }
}

// What the 68000 reads of the sound side (the Z80's RAM, the latches) is as the Z80 has it at the time of the read: the
// Z80 is brought up to it first (the time of a read is the instruction's start, the same however the code is run). A
// translated block checked on the side cannot: its block is then not compared (sideReadUnsure)
void Pgm::SoundToNow() {
    if (Z80Cycles(cpu.cycles) <= z80.cycles) {
        return;
    }
    if (sideRun) {
        sideReadUnsure = true;
        return;
    }
    RunSound(cpu.cycles);
}

void Pgm::SoundCaughtUp() {
    sound.RunTo(z80.cycles * 4);
    z80.SetInterruptLine(sound.Irq());
}

uint8_t Pgm::In(uint16_t port) {
    if ((port & 0xFF00) == 0x8000 && port < 0x8004) {
        SoundCaughtUp();
        const uint8_t value = sound.Read(port & 3);
        z80.SetInterruptLine(sound.Irq());
        return value;
    }
    switch (port & 0xFF00) {
    case 0x8100: return m_latch[2];
    case 0x8200: return m_latch[0];
    case 0x8400: return m_latch[1];
    default: return 0;
    }
}

void Pgm::Out(uint16_t port, uint8_t value) {
    if ((port & 0xFF00) == 0x8000 && port < 0x8004) {
        SoundCaughtUp();
        soundWrites++;
        sound.Write(port & 3, value);
        z80.SetInterruptLine(sound.Irq());
        return;
    }
    switch (port & 0xFF00) {
    case 0x8100: m_latch[2] = value; break;
    case 0x8200: m_latch[0] = value; break;
    case 0x8400: m_latch[1] = value; break;
    default: break;
    }
}

// ---- The ARM (DoDonPachi II's IGS027A) ----

// The ARM run on to the 68000's time
void Pgm::RunArm(uint64_t cpuCycles) {
    // (what it put in its latch up to now is what the latch holds for the 68000 from now on)
    unsigned past = 0;
    while (past < m_latchLaterCount && m_latchLater[past].at <= cpuCycles) {
        m_latchSeen = m_latchLater[past++].value;
    }
    if (past) {
        std::copy(m_latchLater + past, m_latchLater + m_latchLaterCount, m_latchLater);
        m_latchLaterCount -= past;
    }
    if (arm.cycles >= cpuCycles) {
        return;
    }
    if (armTimed) [[unlikely]] {
        const auto began = std::chrono::steady_clock::now();
        arm.Run(cpuCycles);
        armNanoseconds += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began).count());
        return;
    }
    arm.Run(cpuCycles);
}

// The ARM's latch written, at the ARM's time: kept with the time for the 68000's reads
void Pgm::ArmLatchSet(uint32_t value) {
    m_latchArm = value;
    if (m_latchLaterCount == kLatchWrites) {
        // (more than are kept: the oldest is taken as seen)
        m_latchSeen = m_latchLater[0].value;
        std::copy(m_latchLater + 1, m_latchLater + kLatchWrites, m_latchLater);
        m_latchLaterCount--;
    }
    m_latchLater[m_latchLaterCount++] = {arm.cycles, value};
    if (armTrace) [[unlikely]] {
        fprintf(stderr, "arm: frame %llu ARM cycle %llu (the 68000 at %llu): the ARM's latch = %08X (ARM pc %08X)\n", static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(arm.cycles - m_frameStart), static_cast<unsigned long long>(cpu.cycles - m_frameStart), value, arm.Pc());
    }
}

// The 68000 has written its latch: the ARM's fast interrupt. The ARM is run on, ahead of the 68000, until it has
// taken the interrupt (it reads the latch, which drops the line) and is out of its handler again; at most a frame's
// time, should it never be.
void Pgm::ArmThroughInterrupt() {
    const uint64_t limit = arm.cycles + kFrameCycles;
    const auto began = armTimed ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
    while (arm.cycles < limit && (arm.FiqLine() || arm.CurrentMode() == Arm7::kFiq)) {
        arm.Step();
    }
    if (armTimed) [[unlikely]] {
        armNanoseconds += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began).count());
    }
}

// A byte of the table the cartridge's ROM is read through: the two odd bytes of every word whose number (in the ROM)
// ends in `index` are exclusive-ored with it
void Pgm::ArmTableSet(unsigned index, uint8_t value) {
    const uint8_t change = m_xorTable[index] ^ value;
    m_xorTable[index] = value;
    if (!change) {
        return;
    }
    for (size_t at = static_cast<size_t>(index) * 4; at + 3 < m_armCode.size(); at += 0x400) {
        m_armCode[at + 1] ^= change;
        m_armCode[at + 3] ^= change;
    }
}

// Where a byte of the ARM's RAMs is (null: the address is none of them)
uint8_t *Pgm::ArmByte(uint32_t address, bool write) {
    switch (address >> 24) {
    case 0x00: return !write && address < armRom.size() ? &armRom[address] : nullptr;
    case 0x08: return !write && address - 0x08000000 < m_armCode.size() ? &m_armCode[address - 0x08000000] : nullptr;
    case 0x10: return address - 0x10000000 < armRamLow.size() ? &armRamLow[address - 0x10000000] : nullptr;
    case 0x18: return address - 0x18000000 < armRam.size() ? &armRam[address - 0x18000000] : nullptr;
    case 0x48: return address - 0x48000000 < armShared.size() ? &armShared[address - 0x48000000] : nullptr;
    default: return nullptr;
    }
}

uint32_t Pgm::ArmRead32(uint32_t address) {
    // (the shared RAM and the latch are the 68000's to change too: a loop that reads them is not a wait, Arm7::Run)
    arm.touched |= (address >> 24) >= 0x38;
    if (const uint8_t *p = ArmByte(address, false)) {
        uint32_t value;
        std::memcpy(&value, p, 4);
        return value;
    }
    if ((address & ~3u) == 0x38000000) {
        // The 68000's latch read: the fast interrupt it raised is dropped
        arm.SetFiq(false);
        return m_latch68k;
    }
    if (address - 0x50000000 < 0x400) {
        return m_xorTable[(address >> 2) & 0xFF];
    }
    return 0;
}

uint16_t Pgm::ArmRead16(uint32_t address) {
    arm.touched |= (address >> 24) >= 0x38;
    if (const uint8_t *p = ArmByte(address, false)) {
        uint16_t value;
        std::memcpy(&value, p, 2);
        return value;
    }
    return static_cast<uint16_t>(ArmRead32(address & ~3u) >> ((address & 2) * 8));
}

uint8_t Pgm::ArmRead8(uint32_t address) {
    arm.touched |= (address >> 24) >= 0x38;
    if (const uint8_t *p = ArmByte(address, false)) {
        return *p;
    }
    return static_cast<uint8_t>(ArmRead32(address & ~3u) >> ((address & 3) * 8));
}

void Pgm::ArmWrite32(uint32_t address, uint32_t value) {
    arm.touched = true;
    if (uint8_t *p = ArmByte(address, true)) {
        std::memcpy(p, &value, 4);
    } else if ((address & ~3u) == 0x38000000) {
        ArmLatchSet(value);
    } else if (address - 0x50000000 < 0x400) {
        ArmTableSet((address >> 2) & 0xFF, static_cast<uint8_t>(value));
    }
}

void Pgm::ArmWrite16(uint32_t address, uint16_t value) {
    arm.touched = true;
    if (uint8_t *p = ArmByte(address, true)) {
        std::memcpy(p, &value, 2);
    } else if ((address & ~3u) == 0x38000000) {
        const unsigned shift = (address & 2) * 8;
        ArmLatchSet((m_latchArm & ~(0xFFFFu << shift)) | (static_cast<uint32_t>(value) << shift));
    } else if (address - 0x50000000 < 0x400 && !(address & 2)) {
        ArmTableSet((address >> 2) & 0xFF, static_cast<uint8_t>(value));
    }
}

void Pgm::ArmWrite8(uint32_t address, uint8_t value) {
    arm.touched = true;
    if (uint8_t *p = ArmByte(address, true)) {
        *p = value;
    } else if ((address & ~3u) == 0x38000000) {
        const unsigned shift = (address & 3) * 8;
        ArmLatchSet((m_latchArm & ~(0xFFu << shift)) | (static_cast<uint32_t>(value) << shift));
    } else if (address - 0x50000000 < 0x400 && !(address & 3)) {
        ArmTableSet((address >> 2) & 0xFF, value);
    }
}

// ---- The frame ----

void Pgm::SpriteDma() {
    if (!(video.ctrl & 1)) {
        return;
    }
    uint16_t words[256 * 5];
    for (unsigned n = 0; n < 256 * 5; n++) {
        words[n] = static_cast<uint16_t>((ram[n * 2] << 8) | ram[n * 2 + 1]);
    }
    const unsigned taken = video.CopyList(words);
    // (a list unlike the last: the game made a new one)
    uint32_t hash = 2166136261u;
    for (unsigned n = 0; n < taken; n++) {
        hash = (hash ^ words[n]) * 16777619u;
    }
    pictures += hash != m_listHash;
    m_listHash = hash;
    // The CPU is held off the bus while it runs: four of the chip's 50 MHz clocks a word and one more (0.4 of a CPU cycle each)
    const uint64_t until = m_frameStart + kDmaAt + ((static_cast<uint64_t>(taken) * 4 + 1) * 2 + 4) / 5;
    if (cpu.cycles < until) {
        cpu.cycles = until;
    }
}

void Pgm::RunFrame() {
    const uint64_t end = m_frameStart + kFrameCycles;
    bool dmaDone = false;
    unsigned soundStep = 1, armStep = hasArm ? 1 : armSteps + 1;
    while (true) {
        const uint64_t soundAt = m_frameStart + kFrameCycles * soundStep / kSoundSteps;
        const uint64_t armAt = m_frameStart + kFrameCycles * armStep / armSteps;
        uint64_t next = std::min({end, m_nextIrq4, soundAt, armAt});
        if (!dmaDone) {
            next = std::min(next, m_frameStart + kDmaAt);
        }
        if (cpu.cycles < next) {
            // (exact where an interrupt comes, to the instruction it comes in, and where the sprite DMA takes the bus,
            // which holds the CPU to a fixed time; and exact too when one of those follows soon after: translated
            // code run past `next` to its next block could otherwise pass it, and take the interrupt late, or have
            // work done for nothing in the DMA's hold, where the interpreter stops at `next`)
            uint64_t event = ~0ull;
            if (video.ctrl & 4) {
                event = m_nextIrq4;
            }
            event = std::min(event, end); // (and the frame's end always: what the frame leaves is then the same however
                                          // the code is run, its interrupt on or not)
            if (!dmaDone) {
                event = std::min(event, m_frameStart + kDmaAt);
            }
            cpu.Run(next, mode, event <= next + kEventNear);
        }
        if (cpu.cycles >= m_nextIrq4) {
            const uint64_t at = m_nextIrq4;
            m_nextIrq4 += kIrq4Period;
            if (video.ctrl & 4) {
                m_irq4 = true;
                UpdateIrq(at);
            }
        }
        if (!dmaDone && cpu.cycles >= m_frameStart + kDmaAt) {
            dmaDone = true;
            SpriteDma();
        }
        if (cpu.cycles >= soundAt && soundStep <= kSoundSteps) {
            RunSound(soundAt);
            soundStep++;
        }
        if (cpu.cycles >= armAt && armStep <= armSteps) {
            RunArm(armAt);
            armStep++;
        }
        if (cpu.cycles >= end && dmaDone && soundStep > kSoundSteps && armStep > armSteps) {
            break;
        }
    }
    // The vertical blank: the sprites latched at the last one go on show, the picture of the frame is made, the sprites
    // the DMA copied are latched for the next; the interrupt
    video.LatchSprites();
    if (!noPicture && rasterize) {
        video.Draw(m_picture.data());
    }
    if (video.ctrl & 8) {
        m_irq6 = true;
        UpdateIrq(end);
    }
    m_frameStart = end;
    frame++;
}

// ---- The IGS027A: what it answers Cave's games (MAME's simulation: a key that changes with each command, 32 slots
// of 24 bits, sums of them) ----

uint16_t Pgm::ProtectionRead(uint32_t offset) {
    const uint16_t key = static_cast<uint16_t>((m_prot.key >> 8) | m_prot.key);
    if (offset == 0) {
        return static_cast<uint16_t>((m_prot.response & 0xFFFF) ^ key);
    }
    if (offset == 1) {
        return static_cast<uint16_t>((m_prot.response >> 16) ^ key);
    }
    return 0xFFFF;
}

void Pgm::ProtectionWrite(uint32_t offset, uint16_t data) {
    if (offset == 0) {
        m_prot.value0 = data;
        return;
    }
    if (offset != 1) {
        return;
    }
    if ((data >> 8) == 0xFF) {
        m_prot.key = 0xFF00;
    }
    const uint16_t key = static_cast<uint16_t>((m_prot.key >> 8) | m_prot.key);
    m_prot.key = static_cast<uint16_t>((m_prot.key + 0x100) & 0xFF00);
    if (m_prot.key == 0xFF00) {
        m_prot.key = 0x100;
    }
    m_prot.value1 = data ^ key;
    m_prot.value0 ^= key;
    const uint16_t v = m_prot.value0;
    switch (m_prot.value1 & 0xFF) {
    case 0x40: // a sum of two slots into a third
        m_prot.response = 0x880000;
        m_prot.slots[(v >> 10) & 0x1F] = (m_prot.slots[(v >> 5) & 0x1F] + m_prot.slots[v & 0x1F]) & 0xFFFFFF;
        break;
    case 0x67: // a slot chosen, its high bits
        m_prot.response = 0x880000;
        m_prot.slot = (v & 0xFF00) >> 8;
        m_prot.slots[m_prot.slot] = static_cast<uint32_t>(v & 0xFF) << 16;
        break;
    case 0xE5: // its low bits
        m_prot.response = 0x880000;
        m_prot.slots[m_prot.slot] |= v;
        break;
    case 0x8E: // a slot read back
        m_prot.response = m_prot.slots[v & 0xFF];
        break;
    case 0x99: // reset (the region: 0)
        m_prot.key = 0x100;
        m_prot.response = 0x880000;
        break;
    default:
        m_prot.response = 0x880000;
        break;
    }
}

// ---- The V3021 clock: an address of 4 bits, then 8 of data, a bit at each access ----

void Pgm::ClockAccess(bool write, uint8_t bit) {
    Clock &c = m_clock;
    if (write) {
        c.io = bit & 1;
    }
    if (!c.dataMode) {
        c.address = static_cast<uint8_t>(((c.address >> 1) | (c.io << 3)) & 15);
        if (++c.count >= 4) {
            const int64_t now = c.secondsAtReset + static_cast<int64_t>(cpu.cycles / kCpuHz);
            if (c.address == 0xE) {
                // the clock set from the RAM (unless locked)
                if (!(c.ram[0] & 0x10)) {
                    c.ram[1] = 0;
                    const int64_t set = Days(2000 + FromBcd(c.ram[7]), std::max(1, FromBcd(c.ram[6])), std::max(1, FromBcd(c.ram[5]))) * 86400 +
                                        FromBcd(c.ram[4]) * 3600 + FromBcd(c.ram[3]) * 60 + FromBcd(c.ram[2]);
                    c.secondsAtReset = set - static_cast<int64_t>(cpu.cycles / kCpuHz);
                }
            } else if (c.address == 0xF) {
                // the clock into the RAM
                const int64_t days = now / 86400, inDay = now % 86400;
                int year = 2000;
                int64_t left = days;
                while (left >= (year % 4 ? 365 : 366)) {
                    left -= year % 4 ? 365 : 366;
                    year++;
                }
                int month = 1;
                while (month < 12 && left >= Days(year, month + 1, 1) - Days(year, month, 1)) {
                    left -= Days(year, month + 1, 1) - Days(year, month, 1);
                    month++;
                }
                const uint8_t fields[8] = {Bcd(static_cast<int>(inDay % 60)), Bcd(static_cast<int>(inDay / 60 % 60)), Bcd(static_cast<int>(inDay / 3600)),
                                           Bcd(static_cast<int>(left + 1)), Bcd(month), Bcd(year % 100),
                                           Bcd(static_cast<int>((days + 6) % 7) + 1), // (2000-01-01 was a Saturday: Sunday is 1)
                                           Bcd(static_cast<int>((left + 1) % 7) + 1)};
                c.ram[1] = 0;
                for (int n = 0; n < 8; n++) {
                    c.ram[1] |= c.ram[2 + n] != fields[n] ? static_cast<uint8_t>(1 << n) : 0;
                    c.ram[2 + n] = fields[n];
                }
            } else {
                c.data = c.ram[c.address];
                c.dataMode = true;
            }
            c.count = 0;
        }
    } else {
        c.data = static_cast<uint8_t>((c.data >> 1) | (c.io << 7));
        if (++c.count >= 8) {
            if (c.address != 1 && c.address <= 9) {
                c.ram[c.address] = c.data;
            }
            c.dataMode = false;
            c.count = 0;
        }
    }
}

uint8_t Pgm::ClockRead() {
    const uint8_t bit = m_clock.data & 1;
    if (!sideRun) {
        ClockAccess(false, 0);
    }
    return bit;
}

// ---- The 68000's bus ----

uint16_t Pgm::Fetch16(uint32_t address) {
    address &= 0xFFFFFE;
    if (address < program.size()) {
        return static_cast<uint16_t>((program[address] << 8) | program[address + 1]);
    }
    return Read16(address);
}

uint16_t Pgm::Read16(uint32_t address) {
    address &= 0xFFFFFE;
    const uint32_t top = address >> 16;
    if (address < program.size()) {
        return static_cast<uint16_t>((program[address] << 8) | program[address + 1]);
    }
    if (address - protection < 6) {
        return ProtectionRead((address - protection) >> 1);
    }
    if (top >= 0x80 && top < 0x90) {
        const uint32_t at = address & 0x1FFFF;
        return static_cast<uint16_t>((ram[at] << 8) | ram[at + 1]);
    }
    if (top >= 0x90 && top < 0xA0) {
        return video.ReadVideoRam(address & 0x7FFF);
    }
    if (top == 0xA0) {
        const uint32_t at = address & 0xFFFF;
        return at < 0x1400 ? video.paletteRam[at >> 1] : 0;
    }
    if (top == 0xB0) {
        return video.ReadReg(address & 0xFFFF, Line());
    }
    if (top == 0xC0) {
        const uint32_t low = address & 0xFFFF;
        switch (low) {
        case 0x0002: SoundToNow(); return m_latch[0];
        case 0x0004: SoundToNow(); return m_latch[1];
        case 0x0006: return ClockRead();
        case 0x000C: SoundToNow(); return m_latch[2];
        case 0x8000: {
            if (onInputRead && !sideRun) {
                onInputRead(onInputReadContext);
            }
            const uint32_t sys = system | systemHeld;
            auto player = [&](int n, uint32_t start) {
                const uint32_t in = inputs[n];
                return static_cast<uint16_t>(((sys & start) ? 1 : 0) | ((in & kInUp) ? 2 : 0) | ((in & kInDown) ? 4 : 0) | ((in & kInLeft) ? 8 : 0) |
                                             ((in & kInRight) ? 16 : 0) | ((in & kInButton1) ? 32 : 0) | ((in & kInButton2) ? 64 : 0) |
                                             ((in & kInButton3) ? 128 : 0));
            };
            return static_cast<uint16_t>(~(player(0, kSysStart1) | player(1, kSysStart2) << 8));
        }
        case 0x8002: return 0xFFFF;
        case 0x8004: {
            const uint32_t sys = system | systemHeld;
            uint16_t held = 0;
            held |= (sys & kSysCoin1) ? 0x0001 : 0;
            held |= (sys & kSysCoin2) ? 0x0002 : 0;
            held |= (sys & kSysTest) ? 0x0010 : 0;
            held |= (sys & kSysService) ? 0x0020 : 0;
            held |= (inputs[0] & kInButton4) ? 0x0100 : 0;
            held |= (inputs[1] & kInButton4) ? 0x0200 : 0;
            return static_cast<uint16_t>(~held);
        }
        case 0x8006:
            // (the eight switches in the low byte; the high byte's lines are not driven and read high, as the PGM core
            // for MiSTer has them: Dai-Ou-Jou keeps the word's complement at 803764. MAME reads them low)
            return static_cast<uint16_t>(0xFF00 | (dips & 0xFF));
        default: break;
        }
        return 0;
    }
    if (top == 0xC1) {
        SoundToNow();
        const uint32_t at = address & 0xFFFF;
        return static_cast<uint16_t>((z80Ram[at] << 8) | z80Ram[at + 1]);
    }
    if (hasArm) {
        // (the ARM's memory is low byte first: the 68000 sees each 16 bits of it as one of its own words, a 32-bit word's
        // low half at the lower address)
        if (top == 0xD0) {
            const uint32_t at = address & 0xFFFF;
            return static_cast<uint16_t>((armShared[at + 1] << 8) | armShared[at]);
        }
        if (address == 0xD10000) {
            // The ARM's latch as it is at the time of the read (the instruction's start)
            uint32_t value = m_latchSeen;
            for (unsigned n = 0; n < m_latchLaterCount && m_latchLater[n].at <= cpu.cycles; n++) {
                value = m_latchLater[n].value;
            }
            return static_cast<uint16_t>(value);
        }
    }
    return 0;
}

uint8_t Pgm::Read8(uint32_t address) {
    address &= 0xFFFFFF;
    if (address < program.size()) {
        return program[address];
    }
    const uint32_t top = address >> 16;
    if (top >= 0x80 && top < 0x90) {
        return ram[address & 0x1FFFF];
    }
    if (top == 0xC1) {
        SoundToNow();
        return z80Ram[address & 0xFFFF];
    }
    const uint16_t word = Read16(address);
    return static_cast<uint8_t>((address & 1) ? word : word >> 8);
}

void Pgm::WriteByteOrWord(uint32_t address, uint16_t value, uint16_t mask) {
    const uint32_t top = address >> 16, even = address & 0xFFFFFE;
    if (even - protection < 6) {
        // (MAME's handler takes the whole word)
        ProtectionWrite((even - protection) >> 1, value);
        return;
    }
    if (top >= 0x80 && top < 0x90) {
        const uint32_t at = even & 0x1FFFF;
        if (mask & 0xFF00) {
            ram[at] = static_cast<uint8_t>(value >> 8);
        }
        if (mask & 0x00FF) {
            ram[at + 1] = static_cast<uint8_t>(value);
        }
        return;
    }
    if (top >= 0x90 && top < 0xA0) {
        video.WriteVideoRam(even & 0x7FFF, value, mask);
        return;
    }
    if (top == 0xA0) {
        const uint32_t at = even & 0xFFFF;
        if (at < 0x1400) {
            uint16_t &word = video.paletteRam[at >> 1];
            word = static_cast<uint16_t>((word & ~mask) | (value & mask));
        }
        return;
    }
    if (top == 0xB0) {
        if (!video.WriteReg(even & 0xFFFF, value, mask)) {
            // (the interrupts stay up until their enable bits are cleared)
            if (!(video.ctrl & 4)) {
                m_irq4 = false;
            }
            if (!(video.ctrl & 8)) {
                m_irq6 = false;
            }
            UpdateIrq();
        }
        return;
    }
    if (top == 0xC0) {
        const uint32_t low = even & 0xFFFF;
        const uint8_t byte = static_cast<uint8_t>(value);
        const bool lowByte = mask & 0x00FF;
        switch (low) {
        case 0x0002:
            if (lowByte) {
                RunSound(cpu.cycles);
                m_latch[0] = byte;
                latchWrites++;
                z80.Nmi();
            }
            break;
        case 0x0004:
            if (lowByte) {
                RunSound(cpu.cycles);
                m_latch[1] = byte;
            }
            break;
        case 0x0006:
            if (lowByte) {
                ClockAccess(true, byte & 1);
            }
            break;
        case 0x0008:
            RunSound(cpu.cycles);
            if (value == 0x5050) {
                // the Z80 let go, and reset with the ICS2115
                const uint64_t now = sound.Now();
                sound.Reset();
                sound.RunTo(now);
                z80.Reset();
                m_z80Running = true;
            } else {
                m_z80Running = false;
            }
            break;
        case 0x000C:
            if (lowByte) {
                RunSound(cpu.cycles);
                m_latch[2] = byte;
            }
            break;
        default: break;
        }
        return;
    }
    if (hasArm && top == 0xD0) {
        armLateWrites += arm.cycles > cpu.cycles + 16;
        RunArm(cpu.cycles);
        const uint32_t at = even & 0xFFFF;
        if (mask & 0xFF00) {
            armShared[at + 1] = static_cast<uint8_t>(value >> 8);
        }
        if (mask & 0x00FF) {
            armShared[at] = static_cast<uint8_t>(value);
        }
        return;
    }
    if (hasArm && even == 0xD10000) {
        // The 68000's latch: the ARM's fast interrupt, up until the ARM reads the latch
        armLateWrites += arm.cycles > cpu.cycles + 16;
        RunArm(cpu.cycles);
        m_latch68k = (m_latch68k & ~static_cast<uint32_t>(mask)) | (value & mask);
        arm.SetFiq(true);
        if (armTrace) [[unlikely]] {
            fprintf(stderr, "arm: frame %llu cycle %llu: the 68000's latch = %04X (68000 pc %06X, ARM pc %08X)\n", static_cast<unsigned long long>(frame),
                    static_cast<unsigned long long>(cpu.cycles - m_frameStart), m_latch68k, cpu.instruction, arm.Pc());
        }
        ArmThroughInterrupt();
        return;
    }
    if (top == 0xC1) {
        RunSound(cpu.cycles);
        const uint32_t at = even & 0xFFFF;
        if (mask & 0xFF00) {
            Write(static_cast<uint16_t>(at), static_cast<uint8_t>(value >> 8));
        }
        if (mask & 0x00FF) {
            Write(static_cast<uint16_t>(at + 1), static_cast<uint8_t>(value));
        }
    }
}

void Pgm::Write16(uint32_t address, uint16_t value) {
    WriteByteOrWord(address & 0xFFFFFE, value, 0xFFFF);
}

void Pgm::Write8(uint32_t address, uint8_t value) {
    address &= 0xFFFFFF;
    if (address & 1) {
        WriteByteOrWord(address, value, 0x00FF);
    } else {
        WriteByteOrWord(address, static_cast<uint16_t>(value << 8), 0xFF00);
    }
}

// ---- State ----

uint64_t Pgm::StateHash() const {
    uint64_t h = 0xCBF29CE484222325ull;
    auto mix = [&](const void *p, size_t size) {
        const uint8_t *bytes = static_cast<const uint8_t *>(p);
        for (size_t i = 0; i < size; i++) {
            h = (h ^ bytes[i]) * 0x100000001B3ull;
        }
    };
    mix(ram.data(), ram.size());
    mix(cpu.d, sizeof cpu.d);
    mix(cpu.a, sizeof cpu.a);
    mix(&cpu.pc, sizeof cpu.pc);
    const uint16_t sr = cpu.SR();
    mix(&sr, sizeof sr);
    mix(&cpu.cycles, sizeof cpu.cycles);
    mix(video.bgRam, sizeof video.bgRam);
    mix(video.textRam, sizeof video.textRam);
    mix(video.rowScroll, sizeof video.rowScroll);
    mix(video.paletteRam, sizeof video.paletteRam);
    mix(video.spriteBuffer, sizeof video.spriteBuffer);
    mix(z80Ram.data(), z80Ram.size());
    mix(&z80.pc, sizeof z80.pc);
    mix(z80.r, sizeof z80.r);
    mix(&z80.f, sizeof z80.f);
    mix(&z80.sp, sizeof z80.sp);
    mix(&z80.ix, sizeof z80.ix);
    mix(&z80.iy, sizeof z80.iy);
    mix(&z80.cycles, sizeof z80.cycles);
    if (hasArm) {
        arm.Hash(mix);
        mix(armRam.data(), armRam.size());
        mix(armRamLow.data(), armRamLow.size());
        mix(armShared.data(), armShared.size());
        mix(m_xorTable, sizeof m_xorTable);
        mix(&m_latch68k, sizeof m_latch68k);
        mix(&m_latchArm, sizeof m_latchArm);
        mix(&m_latchSeen, sizeof m_latchSeen);
        for (unsigned n = 0; n < m_latchLaterCount; n++) {
            mix(&m_latchLater[n].at, sizeof m_latchLater[n].at);
            mix(&m_latchLater[n].value, sizeof m_latchLater[n].value);
        }
    }
    return h;
}

void Pgm::Serialize(Archive &ar) {
    ar.Mark(0x50474D31);
    cpu.Serialize(ar);
    ar.Vec(ram);
    ar.Vec(z80Ram);
    z80.Serialize(ar);
    video.Serialize(ar);
    sound.Serialize(ar);
    ar.Pod(m_latch);
    ar.Pod(m_z80Running);
    ar.Pod(m_prot);
    ar.Pod(m_clock);
    ar.Pod(m_irq4);
    ar.Pod(m_irq6);
    ar.Pod(m_frameStart);
    ar.Pod(m_nextIrq4);
    ar.Pod(m_listHash);
    ar.Pod(frame);
    ar.Pod(pictures);
    ar.Pod(systemHeld);
    ar.Pod(featureWords);
    if (hasArm) {
        // The whole ARM: its registers, its RAMs, the latches, the table (the cartridge's ROM is made again from it)
        ar.Mark(0x49323741);
        arm.Serialize(ar);
        ar.Vec(armRam);
        ar.Vec(armRamLow);
        ar.Vec(armShared);
        ar.Pod(m_latch68k);
        ar.Pod(m_latchArm);
        ar.Pod(m_latchSeen);
        ar.Pod(m_latchLaterCount);
        for (LatchWrite &w : m_latchLater) {
            ar.Pod(w.at);
            ar.Pod(w.value);
        }
        uint8_t table[256];
        std::memcpy(table, m_xorTable, sizeof table);
        ar.Pod(table);
        if (!ar.Saving()) {
            for (unsigned n = 0; n < 256; n++) {
                ArmTableSet(n, table[n]);
            }
        }
    }
    if (!ar.Saving()) {
        UpdateIrq();
        z80.codeDiffers = Z80CodeDiffers(z80Ram.data());
    }
}

void Pgm::Redraw() {
    video.Draw(m_picture.data());
}

void Pgm::KeepNow(bool exact, int which) {
    m_kept[which].clear();
    Archive ar(m_kept[which], true);
    Serialize(ar);
    // (and the controls as the last frame left them, which a game's features read before the next frame reads its own:
    // free play's Start)
    ar.Pod(inputs);
    ar.Pod(system);
    sound.held = !exact;
}

void Pgm::PutBack(int which) {
    sound.held = false;
    Archive ar(m_kept[which], false);
    Serialize(ar);
    ar.Pod(inputs);
    ar.Pod(system);
}

} // namespace spelunk

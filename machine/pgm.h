// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// IGS's PGM board as Cave's three games on it have it (DoDonPachi Dai-Ou-Jou and its Black Label, Ketsui, Espgaluda:
// 2002 to 2003): a 68000 at 20 MHz with 128 KB of work RAM kept by a battery, the IGS023 video chip
// (machine/igs023.h), a Z80 at 8.4664 MHz running the sound driver the 68000 gives it with an ICS2115 synthesizer
// (machine/ics2115.h), a V3021 clock, and the IGS027A: an ARM7 that deciphers the program (machine/pgm_crypt.h) and
// answers the game's questions. Its own ROM has never been read; what it answers Cave's games is small and known
// (sums of numbers the game hands it, behind a changing key) and is done here as MAME does it.
//
// IGS's own DoDonPachi II (2001) is on the board too, and its IGS027A is of another kind (`hasArm`): the chip's ROM
// has been read, it has a second ROM of program on the cartridge, and it does a good part of the game's work, so the
// ARM7 itself is run (machine/arm7.h) beside the 68000. The two share 64 KB of RAM (D00000) and a latch each way
// (D10000); a write to the 68000's latch raises the ARM's fast interrupt. See "The ARM" below and docs/PGM.md.
//
//   000000  the program (4 MB: DoDonPachi Dai-Ou-Jou's starts with a PGM BIOS, the others are the game alone)
//   400000 or 500000  the IGS027A (game.cfg's `protection`)
//   800000  work RAM (128 KB, again up to 8FFFFF)       900000  the video chip's memory
//   A00000  the palette                                 B00000  the video chip's registers and sprite buffer
//   C00002-C0000D  the sound latches, the clock, the Z80's reset    C08000  the controls, the switches
//   C10000  the Z80's RAM
//   D00000  the RAM shared with the ARM, D10000 the latches (DoDonPachi II)
//
// The memory map, the frame's timing and the interrupts are as MAME's pgm.cpp and pgmprot_igs027a_type1.cpp describe
// them, and the ARM's side of DoDonPachi II's cartridge as its pgmprot_igs027a_type2.cpp does (BSD-3-Clause, David
// Haywood, ElSemi, Xing Xing): see THIRD_PARTY_NOTICES.md.
#pragma once

#include "arm7.h"
#include "igs023.h"
#include "ics2115.h"
#include "inputs.h"
#include "m68k.h"
#include "z80.h"
#include "../runtime/native_z80.h"

#include <cstring>
#include <string>
#include <vector>

namespace spelunk {

class Pgm : public M68KBus, public Z80Bus, public Arm7Bus {
public:
    Pgm();

    M68K cpu;
    Z80 z80;
    // How the Z80 is run: its translated code where there is some for the driver in its RAM (runtime/native_z80.h),
    // or the interpreter alone
    Z80::Mode z80Mode = Z80::Mode::Native;
    Igs023 video;
    Ics2115 sound;
    std::vector<uint8_t> ram;      // 128 KB, big-endian as the board holds it (kept by the battery: the settings, scores)
    std::vector<uint8_t> program;  // 4 MB, deciphered, as the CPU sees it (big-endian)
    std::vector<uint8_t> z80Ram;   // 64 KB
    std::vector<uint8_t> soundRom; // 16 MB, as the ICS2115 sees it
    uint32_t protection = 0x500000; // where the IGS027A answers
    uint16_t dips = 0x00FF;         // the board's eight switches (0: on)
    M68K::Mode mode = M68K::Mode::Interpreter;

    // ---- The ARM (DoDonPachi II's IGS027A: game.cfg's `arm_internal` and `arm_external`) ----
    // The chip's map, as MAME's has it: its own ROM at 0 (16 KB), the cartridge's at 08000000 (read through a table of
    // 256 bytes the chip's ROM writes at 50000000: each word's two odd bytes are exclusive-ored with the table's byte
    // for the word), RAM at 10000000 (1 KB) and 18000000 (64 KB), the latches at 38000000, the RAM shared with the
    // 68000 at 48000000. Its clock is the board's 20 MHz: an ARM cycle is a 68000 cycle.
    //
    // How it is run beside the 68000 (docs/PGM.md, "The ARM beside the 68000"):
    // - It is brought up to the 68000's time at fixed steps of the frame (armSteps) and whenever the 68000 writes to
    //   the shared RAM or the latch, never when the 68000 reads them: what a read returns is then the same however
    //   the 68000's code is run (the rule the Z80 keeps too).
    // - A write to the 68000's latch is the ARM's fast interrupt, and the game then does nothing but read the ARM's
    //   latch until the answer is in it. So the ARM is run through its interrupt handler at once (ahead of the
    //   68000, until it is out of the handler again), and what it puts in its latch meanwhile is kept with the time
    //   it was put there: the 68000 reads the latch as it was at the time of the read (the time of a read is the
    //   same however the code is run, as the video chip's line counter relies on too). The 68000 so waits exactly
    //   as long as the ARM's handler takes.
    bool hasArm = false;
    Arm7 arm;
    std::vector<uint8_t> armRom;      // the chip's own ROM
    std::vector<uint8_t> armExternal; // the cartridge's, deciphered by the fixed rules (machine/pgm_crypt.h), before the table
    std::vector<uint8_t> armRam, armRamLow, armShared;
    unsigned armSteps = 64; // (a frame's fixed steps; the headless runner's --arm-steps: tools/check_arm.sh)
    // (for the tools: writes of the 68000's to the shared RAM or its latch that came while the ARM was ahead of it,
    // so that the ARM had them later than it would have: not part of the board's state)
    uint64_t armLateWrites = 0;
    // (for the tools: the host's time spent running the ARM, when asked for)
    bool armTimed = false;
    uint64_t armNanoseconds = 0;
    // (SPELUNK_TRACE_ARM in the headless runner: every word either CPU puts in its latch, with the frame, the 68000's
    // cycle in it and the two PCs, to stderr)
    bool armTrace = false;
    // Arm7Bus
    uint32_t ArmRead32(uint32_t address) override;
    uint16_t ArmRead16(uint32_t address) override;
    uint8_t ArmRead8(uint32_t address) override;
    void ArmWrite32(uint32_t address, uint32_t value) override;
    void ArmWrite16(uint32_t address, uint16_t value) override;
    void ArmWrite8(uint32_t address, uint8_t value) override;

    // After the set is loaded: starts the CPU at the program's reset vector
    void Reset();

    // What is held: each player's stick and buttons, and the coin, start and service switches
    uint32_t inputs[2] = {0, 0};
    uint32_t system = 0;
    uint32_t systemHeld = 0;
    uint32_t featureWords[4] = {};
    // Called when the game reads the players' ports
    void (*onInputRead)(void *context) = nullptr;
    void *onInputReadContext = nullptr;

    // One frame: 264 lines of 64 microseconds (59.18 Hz), from one vertical blank's start to the next. The sprite DMA
    // runs 3 lines before its end; at the end the picture is made, the sprites copied are latched for the next and the
    // blank's interrupt (level 6) is raised. A second interrupt (level 4) comes every 62 lines, in no step with it.
    void RunFrame();
    static constexpr uint64_t kCpuHz = 20000000;
    static constexpr uint64_t kLineCycles = 1280;
    static constexpr uint64_t kFrameCycles = kLineCycles * 264;      // 337,920
    static constexpr uint64_t kFrameNanoseconds = kFrameCycles * 50; // 16,896,000
    static constexpr unsigned kSoundSteps = 16;
    uint64_t frame = 0;
    // Frames in which the game gave the sprite DMA a list (its own speed)
    uint32_t pictures = 0;
    // The picture on show: 448 x 224, 32 bits a pixel, red in the low byte
    void Picture(uint32_t *out) const {
        std::memcpy(out, m_picture.data(), m_picture.size() * 4);
    }
    bool noPicture = false;
    // Whether the software model draws the picture (off when the GPU draws it, unless the two are being compared)
    bool rasterize = true;

    // The feature "Slowdown as the board" (docs/SLOWDOWN.md): the 68000 waits on the video chip as a board makes it
    // (AccessWait: every access to its memory 2 cycles, the chip's fetch before each line of the picture, its
    // microcycle after; a write to its registers and the palette 1). Measured against a board with the PGM core's
    // test ROMs (docs/PGM.md). Work RAM and the program are not waited for. Off: no waits, as the port always ran.
    void SetBoardTiming(bool on) {
        cpu.accessWait = on ? &Pgm::AccessWait : nullptr;
        cpu.accessWaitBoard = this;
    }
    // (the chip's own use of the video memory before each line of the picture, in the CPU's cycles from the line's
    // start, and every access's wait: AccessWait)
    static constexpr uint64_t kFetchBefore = 6, kFetchEnd = 362;
    static constexpr uint32_t kVideoMemoryWait = 2;

    // A hash of what the game can see (work RAM, the CPU's registers, the video chip, the Z80's RAM), for comparing runs
    uint64_t StateHash() const;
    void Serialize(Archive &ar);
    void Redraw();
    // (exact: the sound chips run on meanwhile; `which`: one of two moments kept at once: online play's rollback)
    void KeepNow(bool exact = false, int which = 0);
    void PutBack(int which = 0);
    // The input lag readout's looks (port/main.cpp), run a frame at a time between the game's own: the board as it is
    // now set aside (SetAside), and later the board and a moment set aside changing places (Trade), so that a frame
    // can be run on from the moment while the game's own state waits, and they change back. The picture on show goes
    // with it. `hold`: the sound chip stands still, as in run-ahead's frames (the moment's frames are not heard).
    struct Aside {
        std::vector<uint8_t> state;
        std::vector<uint32_t> picture;
    };
    void SetAside(Aside &to);
    void Trade(Aside &with, bool hold);

    // M68KBus
    uint8_t Read8(uint32_t address) override;
    uint16_t Read16(uint32_t address) override;
    void Write8(uint32_t address, uint8_t value) override;
    void Write16(uint32_t address, uint16_t value) override;
    uint16_t Fetch16(uint32_t address) override;
    // (the shared RAM and the latches are the ARM's too: a block that writes them and reads them back cannot be
    // checked on the side, where its writes are only noted and the ARM does not see them)
    bool IsMemory(uint32_t address) const override {
        return !hasArm || (address & 0xFE0000) != 0xD00000;
    }
    // Z80Bus
    uint8_t Read(uint16_t address) override {
        return z80Ram[address];
    }
    void Write(uint16_t address, uint8_t value) override {
        const uint8_t was = z80Ram[address];
        z80Ram[address] = value;
        Z80CodeWritten(z80, address, was, value);
    }
    uint8_t In(uint16_t port) override;
    void Out(uint16_t port, uint8_t value) override;

    // (for the tools: whether the 68000 has let the Z80 run, and what the sound side has done)
    bool Z80Running() const {
        return m_z80Running;
    }
    uint64_t soundWrites = 0, latchWrites = 0;

private:
    std::vector<uint32_t> m_picture;
    std::vector<uint8_t> m_kept[2], m_traded;
    uint64_t m_frameStart = 0, m_nextIrq4 = 0;
    bool m_irq4 = false, m_irq6 = false;
    // The sound side: the three latches between the CPUs, whether the Z80 is held, how far it has run
    uint8_t m_latch[3] = {};
    bool m_z80Running = false;
    // The IGS027A's answers (MAME's simulation of it for Cave's games)
    struct Protection {
        uint16_t value0 = 0, value1 = 0, key = 0;
        uint32_t response = 0;
        int32_t slot = 0;
        uint32_t slots[0x100] = {};
    } m_prot;
    // The V3021 clock: a bit at a time; kept at a fixed time from the board's reset, so runs are the same
    struct Clock {
        uint8_t ram[16] = {};
        uint8_t address = 0, data = 0, count = 0, io = 0;
        bool dataMode = false;
        int64_t secondsAtReset = 0; // (seconds since 2000-01-01 when the board was reset)
    } m_clock;
    uint32_t m_listHash = 0;
    // The ARM's side: the latch each CPU writes for the other, the table its ROM is read through, and the cartridge's
    // ROM as read through it (made again from the table when a state is loaded)
    uint32_t m_latch68k = 0, m_latchArm = 0;
    // What the ARM has put in its latch at times the 68000 has not come to yet (in their order), and the latch as it
    // was before the first of them
    struct LatchWrite {
        uint64_t at;
        uint32_t value;
    };
    static constexpr unsigned kLatchWrites = 16;
    LatchWrite m_latchLater[kLatchWrites] = {};
    uint32_t m_latchLaterCount = 0, m_latchSeen = 0;
    void ArmLatchSet(uint32_t value);
    void ArmThroughInterrupt();
    uint8_t m_xorTable[256] = {};
    std::vector<uint8_t> m_armCode;
    void RunArm(uint64_t cpuCycles);
    void ArmTableSet(unsigned index, uint8_t value);
    uint8_t *ArmByte(uint32_t address, bool write);

    void UpdateIrq(uint64_t at);
    void UpdateIrq() {
        UpdateIrq(cpu.cycles);
    }
    static uint32_t AccessWait(const void *board, uint32_t address, uint64_t cycles, bool write);
    void RunSound(uint64_t cpuCycles);
    void SoundToNow();
    void SoundCaughtUp();
    void SpriteDma();
    uint16_t ProtectionRead(uint32_t offset);
    void ProtectionWrite(uint32_t offset, uint16_t value);
    void ClockAccess(bool write, uint8_t bit);
    uint8_t ClockRead();
    void WriteByteOrWord(uint32_t address, uint16_t value, uint16_t mask);
    uint16_t Line() const;
};

} // namespace spelunk

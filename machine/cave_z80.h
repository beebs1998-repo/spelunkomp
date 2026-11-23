// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The sound side of the boards Cave's hardware had before the YMZ280B (1994 to 1996): a Z80 with a program ROM of its
// own (16 KB always there, the rest a 16 KB bank at a time), an FM chip whose timers are the Z80's interrupt, and one
// or two OKI MSM6295s (machine/okim6295.h) whose sample ROMs it pages in halves of 128 KB.
//
//   Mazinger Z (1994):  Z80 at 4 MHz, YM2203 at 4 MHz, one OKI at 1.056 MHz
//   Air Gallet (1996):  Z80 at 8 MHz, YM2151 at 4 MHz, two OKIs at 2.112 MHz   (Sailor Moon's board)
//
// The 68000 writes a 16-bit command into a latch, which raises the Z80's NMI; the Z80 answers with bytes the 68000
// reads back in the order they were written (32 are kept, as MAME keeps them: the two sides talk closely).
//
// Everything is timed in the Z80's cycles: the FM chip's timers and samples, the OKIs' samples (500 cycles apart on
// both boards) and the output, one channel at 44,100 a second. The board brings this side up to the 68000's time at
// fixed steps of the frame and whenever the 68000 writes to it, never when it reads (machine/cave68k.h).
//
// The FM chips are ymfm's (third_party/ymfm*: BSD-3-Clause, Aaron Giles). The ports, the clocks, the latches and the
// mix are as MAME's cave.cpp has them (BSD-3-Clause, Luca Elia): see THIRD_PARTY_NOTICES.md.
#pragma once

#include "okim6295.h"
#include "z80.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace spelunk {

class Archive;

class CaveZ80 : public Z80Bus {
public:
    enum class Kind { Mazinger, AirGallet };

    CaveZ80();
    ~CaveZ80() override;

    Z80 z80;
    // How the Z80 is run: its translated code where there is some (runtime/native_z80.h), or the interpreter alone
    Z80::Mode z80Mode = Z80::Mode::Interpreter;
    std::vector<uint8_t> rom; // the Z80's program
    OkiPair oki;              // (its ROMs set by the board)
    // The sound made: 16 bits, one channel, 44,100 a second
    std::vector<int16_t> out;
    static constexpr uint32_t sampleRate = 44100;
    // Run-ahead: everything runs as ever (the 68000 reads this side's answers), but no samples are made
    bool held = false;

    // After the ROMs are loaded: which board, and the Z80 started (`cpuCycles`: the 68000's time, when the board is
    // reset by its watchdog and not at power on)
    void Reset(Kind kind, uint64_t cpuCycles = 0);
    // The Z80's cycles in a second, and how many of the 68000's 16 MHz cycles one is
    uint64_t Hz() const {
        return m_kind == Kind::Mazinger ? 4000000 : 8000000;
    }
    unsigned CpuCyclesPerCycle() const {
        return m_kind == Kind::Mazinger ? 4 : 2;
    }
    // Brings the Z80 and the chips up to the 68000's time (its cycles since reset)
    void RunTo(uint64_t cpuCycles);

    // --- the 68000's side ---
    // A command written: the latch, and the Z80's NMI if the last one has been read
    void Command(uint16_t value);
    // The Z80's answers: whether one waits, and the oldest (`ahead` further on; none there: 0xFF). Take removes it
    bool AnswerWaits(unsigned ahead = 0) const {
        return ahead < m_latches.count;
    }
    uint8_t Answer(unsigned ahead = 0) const {
        return AnswerWaits(ahead) ? m_latches.answers[(m_latches.first + ahead) & 31] : 0xFF;
    }
    void TakeAnswer();

    void Serialize(Archive &ar);
    // (for a hash of the board: what this side holds that decides what it does next)
    template <typename Mix>
    void Hash(Mix &&mix) const {
        mix(m_ram.data(), m_ram.size());
        mix(&z80.pc, sizeof z80.pc);
        mix(z80.r, sizeof z80.r);
        mix(&z80.f, sizeof z80.f);
        mix(&z80.sp, sizeof z80.sp);
        mix(&z80.ix, sizeof z80.ix);
        mix(&z80.iy, sizeof z80.iy);
        mix(&z80.cycles, sizeof z80.cycles);
        mix(&m_latches, sizeof m_latches);
    }
    // (for the tools) the Z80's memory as it sees it now, 64 KB; and how many commands and answers there have been
    void Image(std::vector<uint8_t> &image);
    uint64_t commands = 0, answers = 0;
    // (and the Z80's writes to the FM chip and to the OKIs, with the loudest sample each has made: before the mix)
    uint64_t fmWrites = 0, okiWrites = 0;
    int32_t fmPeak = 0, okiPeak = 0;

    // Z80Bus
    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    uint8_t In(uint16_t port) override;
    void Out(uint16_t port, uint8_t value) override;

private:
    struct Chips;
    std::unique_ptr<Chips> m;
    Kind m_kind = Kind::Mazinger;
    std::vector<uint8_t> m_ram; // 8 KB (Mazinger Z: 2 KB at C000 and 2 KB at F800)
    struct Latches {
        uint16_t command = 0;
        uint8_t pending = 0; // (the command has not been read: the NMI line is up)
        uint8_t bank = 0;
        uint8_t answers[32] = {};
        uint8_t first = 0, count = 0;
        uint8_t pad[2] = {};
    } m_latches;
    int RamAt(uint16_t address) const;
};

} // namespace spelunk

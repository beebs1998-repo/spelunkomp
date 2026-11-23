// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The ICS2115 WaveFront synthesizer of IGS's PGM board: 32 voices of 8- or 16-bit or u-law samples from ROM, each
// with a volume envelope and a place between the speakers, loops, interrupts when a voice or its envelope comes to an
// end, and two timers (what the sound driver keeps its time by). The board's Z80 has it on its I/O ports.
//
// The registers, the envelopes, the mixing and the timers are as MAME's ics2115.cpp has them (BSD-3-Clause, Alex
// Marshall, nimitz, austere; after O. Galibert and ElSemi): see THIRD_PARTY_NOTICES.md. What MAME does on its own time
// (its timers, the sound stream) is done here at the chip's clock ticks, so that a run is the same every time.
#pragma once

#include <cstdint>
#include <vector>

namespace spelunk {

class Archive;

class Ics2115 {
public:
    static constexpr uint64_t kClock = 33868800; // (33.8688 MHz)
    // With all 32 voices on (the PGM games' setting) the chip makes a sample every 32 x 32 of its clock ticks
    static constexpr uint32_t sampleRate = kClock / (32 * 32); // 33,075 Hz

    // The sample ROM as the chip sees it (24 bits of address), set before Reset
    const uint8_t *rom = nullptr;
    uint32_t romSize = 0;
    void Reset();

    // The chip's four addresses (the board has already brought it up to the time: RunTo)
    uint8_t Read(unsigned offset);
    void Write(unsigned offset, uint8_t value);
    // The interrupt line (a timer, or a voice or envelope that asks for one when it ends)
    bool Irq() const {
        return m_irqOn;
    }
    // Brings the chip up to `ticks` of its clock: samples into `out` (16 bits, one channel), the timers' interrupts
    void RunTo(uint64_t ticks);
    uint64_t Now() const {
        return m_now;
    }
    // Run-ahead: while set, no samples are kept (the chip runs on: the driver waits on its timers)
    bool held = false;
    // Key-ons written to stderr (finding which sound is which)
    bool trace = false;
    std::vector<int16_t> out;
    void Serialize(Archive &ar);

    // What a voice is doing (for finding which tune is which): its sample's address, or -1 when it is not playing
    int Playing(int voice) const;

private:
    struct Voice {
        // the oscillator: address counters (20.9 fixed point), frequency (6.9)
        int32_t oscLeft = 0;
        uint32_t oscAcc = 0, oscStart = 0, oscEnd = 0;
        uint16_t fc = 0;
        uint8_t saddr = 0;
        // the volume envelope
        int32_t volLeft = 0, volAdd = 0, volStart = 0, volEnd = 0, volAcc = 0;
        uint16_t volRegAcc = 0;
        uint8_t volIncr = 0, pan = 0, volMode = 0;
        uint8_t oscCtrl = 0; // bit 0 done, 1 stop
        uint8_t oscConf = 0; // 0 u-law, 1 16 bits, 3 loop, 4 both ways, 5 interrupt, 6 backwards, 7 interrupt pending
        uint8_t volCtrl = 0; // 0 done, 1 stop, 2 rollover, 3 loop, 4 both ways, 5 interrupt, 6 backwards, 7 pending
        uint16_t regs[0x20] = {};
        bool Playing() const {
            return !(oscCtrl & 3);
        }
        bool UpdateEnvelope();
        bool UpdateOscillator();
    };
    struct Timer {
        uint8_t scale = 0, preset = 0;
        uint64_t period = 0; // (clock ticks; 0: not running)
        uint64_t next = 0;   // when it next fires
    };

    Voice m_voices[32];
    Timer m_timers[2];
    uint8_t m_activeOsc = 31, m_oscSelect = 0, m_regSelect = 0, m_irqEnabled = 0, m_irqPending = 0;
    bool m_irqOn = false;
    uint16_t m_regs[0x40] = {};
    uint64_t m_now = 0, m_nextSample = 0;

    int16_t m_ulaw[256];
    uint16_t m_volume[4096];
    uint16_t m_panlaw[256];

    uint8_t Sample(const Voice &v, uint32_t address) const {
        // (24 address lines: the bank's high bits fall off)
        const uint32_t at = ((static_cast<uint32_t>(v.saddr) << 20) | (address & 0xFFFFF)) & 0xFFFFFF;
        return at < romSize ? rom[at] : 0;
    }
    int32_t GetSample(const Voice &v) const;
    void MakeSample();
    uint16_t RegRead();
    void RegWrite(uint16_t data, uint16_t mask);
    void RecalcTimer(int timer);
    void RecalcIrq();
};

} // namespace spelunk

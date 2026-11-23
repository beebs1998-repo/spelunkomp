// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DonPachi's sound: two OKI MSM6295s (four voices each of 4-bit ADPCM from ROM, a phrase table at the ROM's start) and
// the NMK112 that pages their ROMs. The first chip (the music) runs at 4.22 MHz / 4, the second (the effects) at
// 4.22 MHz / 2, both with pin 7 high (a sample every 132 clocks). The NMK112 splits each chip's 256 KB into four
// 64 KB banks, each set to any 64 KB page of the chip's ROM; the second chip's phrase table is paged with them (a
// quarter of the table, 256 bytes, with each bank), the first's is not.
//
// The commands, the decoding, the volumes and the paging are as MAME's okim6295.cpp, okiadpcm.cpp and nmk112.cpp have
// them (BSD-3-Clause; Mirko Buffoni, Aaron Giles, Alex W. Jackson): see THIRD_PARTY_NOTICES.md.
#pragma once

#include <cstdint>
#include <vector>

namespace spelunk {

class Archive;

class OkiPair {
public:
    // Each chip's ROM as it sees it through the NMK112's pages, set before Reset
    const uint8_t *rom[2] = {};
    uint32_t romSize[2] = {};
    void Reset();

    // A command byte to a chip; its status (a bit for each voice playing, the high four set). `at`: the CPU's time of
    // the bus cycle (used by `slotOrigin` only)
    void Write(int chip, uint8_t value, uint64_t at = 0);
    uint8_t Read(int chip, uint64_t at = 0) const;
    // (measurement, headless68k --oki-slots: the chips as the FPGA core's (jt6295), which serves the four voices in
    // turn, a quarter of a sample each: a voice's status bit rises when a start's phrase has been looked up, and falls at
    // its turn after a stop or after its last nibble. Each chip's clock is made from 32 MHz by adding `kSlotStep[chip]`
    // / 65,536 a clock, its 32 MHz clock `slotOrigin` + 2 x the CPU's cycle. -1: off, the bit is the voice playing, as
    // MAME's chip has it)
    int64_t slotOrigin = -1;
    uint64_t slotStartDelay = 0; // (CPU cycles from a start command to its voice's bit: the phrase's addresses read)
    // The NMK112's eight registers: chip (bit 2), bank (bits 0-1)
    void SetBank(int which, uint8_t page);
    // The Z80 boards' paging instead (Mazinger Z, Air Gallet: machine/cave_z80.h): no NMK112, each chip's 256 KB two
    // halves of 128 KB, each set to any 128 KB of its ROM
    bool halves = false;
    void SetHalves(int chip, uint8_t low, uint8_t high);
    // One sample of a chip's own (a 132nd of its clock), for a board that mixes the chips itself: its four voices
    // added, each 12 bits
    int32_t Step(int chip);

    // Makes samples up to `cycles` (CPU cycles of `cpuHz`) into `out`: 16 bits, one channel, at `sampleRate`
    void RunTo(uint64_t cycles, uint64_t cpuHz);
    bool held = false; // (run-ahead: no samples, writes dropped)
    std::vector<int16_t> out;
    static constexpr uint32_t sampleRate = 44100;

    void Serialize(Archive &ar);

private:
    struct Voice {
        bool playing = false;
        uint32_t base = 0, sample = 0, count = 0;
        int32_t signal = 0, step = 0, volume = 0; // (volume in 32nds)
    };
    struct Chip {
        Voice voices[4];
        int32_t command = -1;
        int32_t last = 0; // (the last sample made)
        uint64_t made = 0;
    };
    Chip m_chips[2];
    uint64_t m_busyUntil[2][4] = {}; // (slotOrigin: when each voice's status bit falls; when it rises after a start, and
    uint64_t m_busyFrom[2][4] = {}, m_busyBefore[2][4] = {}; // when it fell before that)
    uint64_t Enables(int chip, uint64_t at) const;
    uint64_t CycleOf(int chip, uint64_t enables) const;
    uint64_t Turn(int chip, int voice, uint64_t at) const;
    uint8_t m_banks[8] = {};
    uint64_t m_made = 0; // (output samples made since reset)

    uint8_t Byte(int chip, uint32_t at) const;
    void Tick(int chip);
};

} // namespace spelunk

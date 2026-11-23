// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; the chips' event loop is cps2recomp's CPS-1 sound board's.
// The ports, the clocks, the latches and the mix are as MAME's cave.cpp has them (BSD-3-Clause, Luca Elia); the FM
// chips are ymfm's (BSD-3-Clause, Aaron Giles).
#include "cave_z80.h"

#include "../runtime/native_z80.h"
#include "../runtime/state.h"
#include "../third_party/ymfm_opm.h"
#include "../third_party/ymfm_opn.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace spelunk {

namespace {
    constexpr uint64_t kNever = std::numeric_limits<uint64_t>::max();
    // Both boards' OKIs make a sample every 500 of the Z80's cycles (1.056 MHz / 132 beside 4 MHz: 8,000 a second;
    // 2.112 MHz / 132 beside 8 MHz: 16,000)
    constexpr uint64_t kOkiCycles = 500;
    // The longest the Z80 runs before the chips are looked at (a timer it has just started is seen run out no later)
    constexpr uint64_t kSlice = 256;
} // namespace

struct CaveZ80::Chips : public ymfm::ymfm_interface {
    std::unique_ptr<ymfm::ym2203> opn; // (Mazinger Z)
    std::unique_ptr<ymfm::ym2151> opm; // (Air Gallet)
    uint64_t hz = 4000000;
    uint64_t ymScale = 1;   // the Z80's cycles in one of the FM chip's clocks
    uint64_t ymCycles = 24; // the Z80's cycles from one of the FM chip's samples to the next

    // What is kept in a save state
    struct Times {
        uint64_t now = 0; // the time the FM chip's timers and its busy flag are counted from (a port access, RunTo)
        uint64_t timerAt[2] = {kNever, kNever};
        uint64_t busyUntil = 0;
        uint64_t ymAt = 0;               // the FM chip's next sample
        uint64_t okiAt = 0, okiLast = 0; // the OKIs' next sample, and when they made their last
        uint64_t outCount = 0, outAt = 0; // the output's samples made, and the next
        int64_t fmSum = 0, ssgSum = 0;   // the FM chip's samples since the last output (its FM part, its three SSG voices)
        int32_t ymCount = 0;
        int32_t fmLast = 0, ssgLast = 0;
        int32_t okiPrev[2] = {}, okiCur[2] = {}; // each OKI's last two samples
        bool irq = false;
        uint8_t pad[7] = {};
    } t;

    // --- ymfm_interface ---
    void ymfm_set_timer(uint32_t tnum, int32_t duration) override {
        t.timerAt[tnum] = duration < 0 ? kNever : t.now + static_cast<uint64_t>(duration) * ymScale;
    }
    void ymfm_set_busy_end(uint32_t clocks) override {
        t.busyUntil = t.now + clocks * ymScale;
    }
    bool ymfm_is_busy() override {
        return t.now < t.busyUntil;
    }
    void ymfm_update_irq(bool asserted) override {
        t.irq = asserted;
    }

    uint64_t OutAt() const {
        return (t.outCount + 1) * hz / sampleRate;
    }
    uint64_t NextTimer() const {
        return std::min(t.timerAt[0], t.timerAt[1]);
    }
    void Run(OkiPair &oki, uint64_t now, bool mazinger, bool held, std::vector<int16_t> &out);
    int32_t fmPeak = 0, okiPeak = 0; // (for the tools)
};

// The chips: everything due up to the Z80's time now (timers run out, samples made)
void CaveZ80::Chips::Run(OkiPair &oki, uint64_t now, bool mazinger, bool held, std::vector<int16_t> &out) {
    for (;;) {
        const uint64_t at = std::min({t.timerAt[0], t.timerAt[1], t.ymAt, t.okiAt, t.outAt});
        if (at > now) {
            break;
        }
        if (t.timerAt[0] == at || t.timerAt[1] == at) {
            // (the chip restarts the timer: it counts from here, so its period stays exact)
            const uint32_t which = t.timerAt[0] == at ? 0 : 1;
            t.now = at;
            t.timerAt[which] = kNever;
            m_engine->engine_timer_expired(which);
        } else if (t.ymAt == at) {
            if (!held) {
                if (mazinger) {
                    ymfm::ym2203::output_data sample;
                    opn->generate(&sample);
                    t.fmLast = sample.data[0];
                    t.ssgLast = sample.data[1] + sample.data[2] + sample.data[3];
                } else {
                    ymfm::ym2151::output_data sample;
                    opm->generate(&sample);
                    t.fmLast = sample.data[0] + sample.data[1];
                }
                t.fmSum += t.fmLast;
                t.ssgSum += t.ssgLast;
                t.ymCount++;
            }
            t.ymAt += ymCycles;
        } else if (t.okiAt == at) {
            // (the OKIs run on while held too: the Z80 reads which of their voices are playing)
            for (int which = 0; which < (mazinger ? 1 : 2); which++) {
                t.okiPrev[which] = t.okiCur[which];
                t.okiCur[which] = oki.Step(which);
            }
            t.okiLast = at;
            t.okiAt += kOkiCycles;
        } else {
            // The output: the FM chip's samples since the last averaged; a line drawn between the OKIs' last two
            // (so they are a sample late)
            if (!held) {
                const int32_t fm = t.ymCount ? static_cast<int32_t>(t.fmSum / t.ymCount) : t.fmLast;
                const int32_t ssg = t.ymCount ? static_cast<int32_t>(t.ssgSum / t.ymCount) : t.ssgLast;
                const int64_t span = static_cast<int64_t>(t.okiAt - t.okiLast), into = static_cast<int64_t>(at - t.okiLast);
                int32_t okis = 0;
                for (int which = 0; which < 2; which++) {
                    okis += t.okiPrev[which] + static_cast<int32_t>((t.okiCur[which] - t.okiPrev[which]) * into / span);
                }
                // The mix (MAME's shares; an OKI's 12 bits are 16 times as much in 16). Mazinger Z: the YM2203's FM
                // 0.6, each of its SSG voices 0.2, the OKI 2. Air Gallet: each of the YM2151's two outputs 0.3,
                // each OKI 1
                const int32_t mix = mazinger ? (fm * 6 + ssg * 2 + okis * 320) / 10 : (fm * 3 + okis * 160) / 10;
                out.push_back(static_cast<int16_t>(std::clamp(mix, -32768, 32767)));
                fmPeak = std::max({fmPeak, std::abs(fm), std::abs(ssg)});
                okiPeak = std::max(okiPeak, std::abs(okis));
            }
            t.fmSum = t.ssgSum = 0;
            t.ymCount = 0;
            t.outCount++;
            t.outAt = OutAt();
        }
    }
}

CaveZ80::CaveZ80()
    : z80(*this)
    , m(std::make_unique<Chips>())
    , m_ram(0x2000, 0) {}

CaveZ80::~CaveZ80() = default;

void CaveZ80::Reset(Kind kind, uint64_t cpuCycles) {
    m_kind = kind;
    m->opn.reset();
    m->opm.reset();
    m->hz = Hz();
    if (kind == Kind::Mazinger) {
        // (the least of ymfm's output rates: a 24th of the clock, three to an FM sample at the chip's own divider)
        m->opn = std::make_unique<ymfm::ym2203>(*m);
        m->opn->set_fidelity(ymfm::OPN_FIDELITY_MIN);
        m->opn->reset();
        m->ymScale = 1;
        m->ymCycles = 24;
    } else {
        m->opm = std::make_unique<ymfm::ym2151>(*m);
        m->opm->reset();
        m->ymScale = 2;
        m->ymCycles = 64 * 2;
    }
    const uint64_t now = cpuCycles / CpuCyclesPerCycle();
    m->t = Chips::Times{};
    m->t.now = now;
    m->t.ymAt = now + m->ymCycles;
    m->t.okiLast = now;
    m->t.okiAt = now + kOkiCycles;
    m->t.outCount = now * sampleRate / m->hz;
    m->t.outAt = m->OutAt();
    oki.halves = true;
    oki.Reset();
    std::fill(m_ram.begin(), m_ram.end(), 0);
    m_latches = Latches{};
    z80.Reset();
    z80.cycles = now;
    z80.SetInterruptLine(false);
    std::vector<uint8_t> image;
    Image(image);
    z80.codeDiffers = Z80CodeDiffers(image.data());
    if (!cpuCycles) {
        out.clear();
        commands = answers = fmWrites = okiWrites = 0;
    }
}

void CaveZ80::Image(std::vector<uint8_t> &image) {
    image.resize(0x10000);
    for (unsigned a = 0; a < 0x10000; a++) {
        image[a] = Read(static_cast<uint16_t>(a));
    }
}

// Where an address is in the RAM (-1: not RAM). Mazinger Z's is 2 KB at C000 and 2 KB at F800; Air Gallet's 8 KB at
// C000, again at E000
int CaveZ80::RamAt(uint16_t address) const {
    if (m_kind == Kind::Mazinger) {
        return address >= 0xF800 ? 0x800 + (address & 0x7FF) : address >= 0xC000 && address < 0xC800 ? address & 0x7FF : -1;
    }
    return address >= 0xC000 ? address & 0x1FFF : -1;
}

uint8_t CaveZ80::Read(uint16_t address) {
    if (address < 0x4000) {
        return address < rom.size() ? rom[address] : 0xFF;
    }
    if (address < 0x8000) {
        return rom.empty() ? 0xFF : rom[((static_cast<size_t>(m_latches.bank) << 14) | (address & 0x3FFF)) % rom.size()];
    }
    const int at = RamAt(address);
    return at < 0 ? 0xFF : m_ram[static_cast<size_t>(at)];
}

void CaveZ80::Write(uint16_t address, uint8_t value) {
    const int at = RamAt(address);
    if (at >= 0) {
        m_ram[static_cast<size_t>(at)] = value;
    }
}

void CaveZ80::RunTo(uint64_t cpuCycles) {
    const uint64_t to = cpuCycles / CpuCyclesPerCycle();
    const bool mazinger = m_kind == Kind::Mazinger;
    while (z80.cycles < to) {
        const uint64_t slice = std::max(z80.cycles + 1, std::min({to, m->NextTimer(), z80.cycles + kSlice}));
        z80.Run(slice, z80Mode);
        m->Run(oki, z80.cycles, mazinger, held, out);
        z80.SetInterruptLine(m->t.irq);
    }
    fmPeak = m->fmPeak;
    okiPeak = m->okiPeak;
}

uint8_t CaveZ80::In(uint16_t port) {
    const bool mazinger = m_kind == Kind::Mazinger;
    auto sync = [&] {
        m->Run(oki, z80.cycles, mazinger, held, out);
        m->t.now = z80.cycles;
    };
    uint8_t value = 0xFF;
    switch (port & 0xFF) {
    case 0x20: // (Air Gallet: flags of the latches, never set)
        value = mazinger ? 0xFF : 0;
        break;
    case 0x30: // The command's low byte
        m_latches.pending = 0;
        value = static_cast<uint8_t>(m_latches.command);
        break;
    case 0x40: // (Air Gallet) its high byte
        if (!mazinger) {
            m_latches.pending = 0;
            value = static_cast<uint8_t>(m_latches.command >> 8);
        }
        break;
    case 0x50:
    case 0x51: // (Air Gallet) the YM2151: its status at the second port
        if (!mazinger) {
            sync();
            value = m->opm->read(port & 1);
        }
        break;
    case 0x52:
    case 0x53: // (Mazinger Z) the YM2203: its status, the register chosen
        if (mazinger) {
            sync();
            value = m->opn->read(port & 1);
        }
        break;
    case 0x60: // (Air Gallet) the OKIs: which voices are playing
    case 0x80:
        if (!mazinger) {
            sync();
            value = oki.Read((port & 0xFF) == 0x80 ? 1 : 0);
        }
        break;
    default: break;
    }
    z80.SetInterruptLine(m->t.irq);
    return value;
}

void CaveZ80::Out(uint16_t port, uint8_t value) {
    const bool mazinger = m_kind == Kind::Mazinger;
    auto sync = [&] {
        m->Run(oki, z80.cycles, mazinger, held, out);
        m->t.now = z80.cycles;
    };
    const unsigned at = port & 0xFF;
    switch (at) {
    case 0x00: // The program ROM's bank
        m_latches.bank = value & (mazinger ? 0x07 : 0x1F);
        break;
    case 0x10: // An answer for the 68000 (32 kept; one more is lost)
        if (m_latches.count < 32) {
            m_latches.answers[(m_latches.first + m_latches.count) & 31] = value;
            m_latches.count++;
        }
        answers++;
        break;
    case 0x50:
    case 0x51: // The FM chip: the register, its value
        sync();
        fmWrites++;
        if (mazinger) {
            m->opn->write(at & 1, value);
        } else {
            m->opm->write(at & 1, value);
        }
        break;
    case 0x60:
        if (!mazinger) {
            sync();
            okiWrites++;
            oki.Write(0, value);
        }
        break;
    case 0x70: // Mazinger Z: the OKI. Air Gallet: the first OKI's ROM halves (4 bits each)
        sync();
        if (mazinger) {
            okiWrites++;
            oki.Write(0, value);
        } else {
            oki.SetHalves(0, value & 0x0F, value >> 4);
        }
        break;
    case 0x74: // (Mazinger Z) the OKI's ROM halves (2 bits each)
        if (mazinger) {
            sync();
            oki.SetHalves(0, value & 3, (value >> 4) & 3);
        }
        break;
    case 0x80:
        if (!mazinger) {
            sync();
            okiWrites++;
            oki.Write(1, value);
        }
        break;
    case 0xC0:
        if (!mazinger) {
            sync();
            oki.SetHalves(1, value & 0x0F, value >> 4);
        }
        break;
    default: break;
    }
    z80.SetInterruptLine(m->t.irq);
}

void CaveZ80::Command(uint16_t value) {
    m_latches.command = value;
    if (!m_latches.pending) {
        z80.Nmi();
    }
    m_latches.pending = 1;
    commands++;
}

void CaveZ80::TakeAnswer() {
    if (m_latches.count) {
        m_latches.first = (m_latches.first + 1) & 31;
        m_latches.count--;
    }
}

void CaveZ80::Serialize(Archive &ar) {
    ar.Mark(0x435A3830);
    z80.Serialize(ar);
    ar.Vec(m_ram);
    ar.Pod(m_latches);
    std::vector<uint8_t> chip;
    if (ar.Saving()) {
        ymfm::ymfm_saved_state state(chip, true);
        if (m->opn) {
            m->opn->save_restore(state);
        } else if (m->opm) {
            m->opm->save_restore(state);
        }
    }
    ar.Vec(chip);
    if (!ar.Saving() && ar.Ok()) {
        ymfm::ymfm_saved_state state(chip, false);
        if (m->opn) {
            m->opn->save_restore(state);
        } else if (m->opm) {
            m->opm->save_restore(state);
        }
    }
    oki.Serialize(ar);
    ar.Pod(m->t);
}

} // namespace spelunk

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// As MAME's ics2115.cpp has the chip (BSD-3-Clause, Alex Marshall, nimitz, austere): see ics2115.h.
#include "ics2115.h"

#include "../runtime/state.h"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>

namespace spelunk {

namespace {
    enum : uint8_t {
        kDone = 1,
        kStop = 2,
        kUlaw = 1,
        kSixteen = 2,
        kLoop = 8,
        kBidir = 16,
        kIrq = 32,
        kInvert = 64,
        kPending = 128,
    };
} // namespace

void Ics2115::Reset() {
    // The tables: the volume (as measured on the chip: exponent i[11:8], mantissa i[7:0]), u-law (MIL-STD-188-113) and
    // the pan law
    for (int i = 0; i < 4096; i++) {
        const uint8_t exponent = static_cast<uint8_t>(i >> 8), mantissa = static_cast<uint8_t>(i);
        m_volume[i] = exponent == 0 ? static_cast<uint16_t>(mantissa >> 7) : static_cast<uint16_t>((((0x100 | mantissa) << (exponent - 1)) + 0xFF) >> 8);
    }
    uint16_t lut[8];
    const uint16_t lutInitial = 33 << 2;
    for (int i = 0; i < 8; i++) {
        lut[i] = static_cast<uint16_t>((lutInitial << i) - lutInitial);
    }
    for (unsigned i = 0; i < 256; i++) {
        const uint8_t exponent = (~i >> 4) & 7, mantissa = ~i & 15;
        const int16_t value = static_cast<int16_t>(lut[exponent] + (mantissa << (exponent + 3)));
        m_ulaw[i] = (i & 0x80) ? static_cast<int16_t>(-value) : value;
        m_panlaw[i] = static_cast<uint16_t>(16 - (std::bit_width(i) - 1));
    }
    m_panlaw[0] = 0xFFF;

    m_irqEnabled = m_irqPending = 0;
    m_activeOsc = 31;
    m_oscSelect = m_regSelect = 0;
    m_irqOn = false;
    std::memset(m_regs, 0, sizeof m_regs);
    for (Timer &t : m_timers) {
        t = Timer{};
    }
    for (Voice &v : m_voices) {
        v = Voice{};
        v.oscConf = 2;
        v.oscCtrl = 2;
        v.pan = 0x7F;
        v.volCtrl = 1;
    }
    m_now = m_nextSample = 0;
    out.clear();
}

// The envelope's next step (column 126 of patent 5809466); true: it asks for an interrupt
bool Ics2115::Voice::UpdateEnvelope() {
    // (as MAME has it: the test is always true)
    const bool bc = true;
    if (volCtrl & (kDone | kStop)) {
        return false;
    }
    if (volCtrl & kInvert) {
        volAcc -= volAdd;
        volLeft = volAcc - volStart;
    } else {
        volAcc += volAdd;
        volLeft = volEnd - volAcc;
    }
    if (volLeft > 0) {
        return false;
    }
    bool irq = false;
    if (volCtrl & kIrq) {
        volCtrl |= kPending;
        irq = true;
    }
    if (volCtrl & kLoop) {
        if (bc) {
            if (!(volCtrl & kBidir)) {
                volAcc = !(volCtrl & kInvert) ? volStart - (volEnd - (volAcc + volIncr)) : volEnd + ((volAcc - volIncr) - volStart);
            } else {
                volAcc = !(volCtrl & kInvert) ? volEnd + (volEnd - (volAcc + volIncr)) : volStart - ((volAcc - volIncr) - volStart);
            }
        }
    } else {
        volCtrl |= kDone;
    }
    return irq;
}

bool Ics2115::Voice::UpdateOscillator() {
    if (!Playing()) {
        return false;
    }
    if (oscConf & kInvert) {
        oscAcc -= static_cast<uint32_t>(fc) << 2;
        oscLeft = static_cast<int32_t>(oscAcc - oscStart);
    } else {
        oscAcc += static_cast<uint32_t>(fc) << 2;
        oscLeft = static_cast<int32_t>(oscEnd - oscAcc);
    }
    if (oscLeft > 0) {
        return false;
    }
    bool irq = false;
    if (oscConf & kIrq) {
        oscConf |= kPending;
        irq = true;
    }
    if (oscConf & kLoop) {
        if (oscConf & kBidir) {
            oscConf ^= kInvert;
        }
        if (oscConf & kInvert) {
            oscAcc = oscEnd + static_cast<uint32_t>(oscLeft);
            oscLeft = static_cast<int32_t>(oscAcc - oscStart);
        } else {
            oscAcc = oscStart - static_cast<uint32_t>(oscLeft);
            oscLeft = static_cast<int32_t>(oscEnd - oscAcc);
        }
    } else {
        oscCtrl |= kDone;
        oscAcc = (oscConf & kInvert) ? oscStart : oscEnd;
    }
    return irq;
}

int32_t Ics2115::GetSample(const Voice &v) const {
    const uint32_t current = v.oscAcc >> 12;
    uint32_t next;
    if (v.oscCtrl == 0 && (v.oscConf & kLoop) && !(v.oscConf & kBidir) && v.oscLeft < (static_cast<int32_t>(v.fc) << 2)) {
        next = v.oscStart >> 12;
    } else {
        next = current + 2;
    }
    int32_t s1, s2;
    if (v.oscConf & kUlaw) {
        s1 = m_ulaw[Sample(v, current)];
        s2 = m_ulaw[Sample(v, current + 1)];
    } else if (!(v.oscConf & kSixteen)) {
        s1 = static_cast<int8_t>(Sample(v, current)) * 256;
        s2 = static_cast<int8_t>(Sample(v, current + 1)) * 256;
    } else {
        s1 = Sample(v, current) | (static_cast<int8_t>(Sample(v, current + 1)) * 256);
        s2 = Sample(v, next) | (static_cast<int8_t>(Sample(v, next + 1)) * 256);
    }
    // (linear interpolation: US patent 6,246,774 B1, column 2 row 59)
    const int32_t diff = s2 - s1, fract = static_cast<int32_t>((v.oscAcc & 0xFF8) >> 3);
    return (s1 * 512 + diff * fract) >> 9;
}

// One sample of every voice on, mixed: the board's speaker has the two sides at a tenth each (MAME's level)
void Ics2115::MakeSample() {
    int32_t left = 0, right = 0;
    bool irq = false;
    for (int n = 0; n <= m_activeOsc; n++) {
        Voice &v = m_voices[n];
        const int fine = 1 << (3 * (v.volIncr >> 6));
        v.volAdd = (v.volIncr & 0x3F) << (10 - fine);
        if (!v.Playing()) {
            continue;
        }
        const int32_t volacc = (v.volAcc >> 14) & 0xFFF;
        const int32_t vlefti = volacc - m_panlaw[255 - v.pan], vrighti = volacc - m_panlaw[v.pan];
        const int32_t vleft = vlefti > 0 ? m_volume[vlefti] : 0, vright = vrighti > 0 ? m_volume[vrighti] : 0;
        const int32_t sample = GetSample(v);
        left += (sample * vleft) >> 15;
        right += (sample * vright) >> 15;
        irq |= v.UpdateOscillator();
        irq |= v.UpdateEnvelope();
    }
    if (!held) {
        out.push_back(static_cast<int16_t>(std::clamp((left + right) / 10, -32768, 32767)));
    }
    if (irq) {
        RecalcIrq();
    }
}

void Ics2115::RunTo(uint64_t ticks) {
    const uint64_t samplePeriod = 32ull * (m_activeOsc + 1u);
    while (true) {
        // the next of: a sample, either timer
        uint64_t when = m_nextSample;
        int which = -1;
        for (int t = 0; t < 2; t++) {
            if (m_timers[t].period && m_timers[t].next < when) {
                when = m_timers[t].next;
                which = t;
            }
        }
        if (when > ticks) {
            break;
        }
        m_now = when;
        if (which < 0) {
            MakeSample();
            m_nextSample += samplePeriod;
        } else {
            Timer &t = m_timers[which];
            t.next += t.period;
            if (!(m_irqPending & (1 << which))) {
                m_irqPending |= static_cast<uint8_t>(1 << which);
                RecalcIrq();
            }
        }
    }
    m_now = std::max(m_now, ticks);
}

uint16_t Ics2115::RegRead() {
    uint16_t ret = 0;
    Voice &v = m_voices[m_oscSelect];
    switch (m_regSelect) {
    case 0x00: ret = static_cast<uint16_t>(((v.oscCtrl == 0) ? (v.oscConf | 8) : (v.oscConf & ~8)) << 8); break;
    case 0x01: ret = v.fc; break;
    case 0x02: ret = static_cast<uint16_t>(v.oscStart >> 16); break;
    case 0x03: ret = static_cast<uint16_t>(v.oscStart & 0xFF00); break;
    case 0x04: ret = static_cast<uint16_t>(v.oscEnd >> 16); break;
    case 0x05: ret = static_cast<uint16_t>(v.oscEnd & 0xFF00); break;
    case 0x06: ret = v.volIncr; break;
    case 0x07: ret = static_cast<uint16_t>(v.volStart >> 18); break;
    case 0x08: ret = static_cast<uint16_t>(v.volEnd >> 18); break;
    case 0x09: ret = static_cast<uint16_t>(v.volAcc >> 10); break;
    case 0x0A: ret = static_cast<uint16_t>(v.oscAcc >> 16); break;
    case 0x0B: ret = static_cast<uint16_t>(v.oscAcc & 0xFFF8); break;
    case 0x0C: ret = static_cast<uint16_t>(v.pan << 8); break;
    case 0x0D: ret = static_cast<uint16_t>((0x01 | ((v.volCtrl & kIrq) ? 0x80 : 0)) << 8); break;
    case 0x0E: ret = m_activeOsc; break;
    case 0x0F: {
        // the first voice with an interrupt waiting (reading takes it)
        ret = 0xFF;
        for (int i = 0; i <= m_activeOsc; i++) {
            Voice &w = m_voices[i];
            if ((w.oscConf & kPending) || (w.volCtrl & kPending)) {
                ret = static_cast<uint16_t>(i | 0xE0);
                if (w.oscConf & kPending) {
                    w.oscConf &= ~kPending;
                    ret &= ~0x80;
                }
                if (w.volCtrl & kPending) {
                    w.volCtrl &= ~kPending;
                    ret &= ~0x40;
                }
                RecalcIrq();
                break;
            }
        }
        ret = static_cast<uint16_t>(ret << 8);
        break;
    }
    case 0x10: ret = static_cast<uint16_t>(v.oscCtrl << 8); break;
    case 0x11: ret = static_cast<uint16_t>(v.saddr << 8); break;
    case 0x40:
    case 0x41:
        ret = m_timers[m_regSelect & 1].preset;
        m_irqPending &= static_cast<uint8_t>(~(1 << (m_regSelect & 1)));
        RecalcIrq();
        break;
    case 0x43: ret = m_irqPending & 3; break;
    case 0x4A: ret = m_irqPending; break;
    case 0x4B: ret = 0x80; break;
    case 0x4C: ret = 1; break; // (the chip's revision)
    default: ret = 0; break;
    }
    return ret;
}

void Ics2115::RegWrite(uint16_t data, uint16_t mask) {
    Voice &v = m_voices[m_oscSelect];
    auto combine = [&](uint16_t &reg) { reg = static_cast<uint16_t>((reg & ~mask) | (data & mask)); };
    if (m_regSelect < 0x20) {
        combine(v.regs[m_regSelect]);
    } else if (m_regSelect >= 0x40 && m_regSelect < 0x80) {
        combine(m_regs[m_regSelect - 0x40]);
    }
    const bool high = mask & 0xFF00, low = mask & 0x00FF;
    switch (m_regSelect) {
    case 0x00:
        if (high) {
            v.oscConf = static_cast<uint8_t>((v.oscConf & 0x80) | ((data >> 8) & 0x7F));
            RecalcIrq();
        }
        break;
    case 0x01:
        if (high) {
            v.fc = static_cast<uint16_t>((v.fc & 0x00FE) | (data & 0xFF00));
        }
        if (low) {
            v.fc = static_cast<uint16_t>((v.fc & 0xFF00) | (data & 0x00FE));
        }
        break;
    case 0x02:
        if (high) {
            v.oscStart = (v.oscStart & 0x00FFFFFF) | (static_cast<uint32_t>(data & 0xFF00) << 16);
        }
        if (low) {
            v.oscStart = (v.oscStart & 0xFF00FFFF) | (static_cast<uint32_t>(data & 0x00FF) << 16);
        }
        break;
    case 0x03:
        if (high) {
            v.oscStart = (v.oscStart & 0xFFFF00FF) | (data & 0xFF00);
        }
        break;
    case 0x04:
        if (high) {
            v.oscEnd = (v.oscEnd & 0x00FFFFFF) | (static_cast<uint32_t>(data & 0xFF00) << 16);
        }
        if (low) {
            v.oscEnd = (v.oscEnd & 0xFF00FFFF) | (static_cast<uint32_t>(data & 0x00FF) << 16);
        }
        break;
    case 0x05:
        if (high) {
            v.oscEnd = (v.oscEnd & 0xFFFF00FF) | (data & 0xFF00);
        }
        break;
    case 0x06:
        if (high) {
            v.volIncr = static_cast<uint8_t>(data >> 8);
        }
        break;
    case 0x07:
        if (low) {
            v.volStart = (data & 0xFF) << 18;
        }
        break;
    case 0x08:
        if (low) {
            v.volEnd = (data & 0xFF) << 18;
        }
        break;
    case 0x09:
        if (high) {
            v.volRegAcc = static_cast<uint16_t>((v.volRegAcc & 0x00FF) | (data & 0xFF00));
        }
        if (low) {
            v.volRegAcc = static_cast<uint16_t>((v.volRegAcc & 0xFF00) | (data & 0x00FF));
        }
        v.volAcc = v.volRegAcc << 10;
        break;
    case 0x0A:
        if (high) {
            v.oscAcc = (v.oscAcc & 0x00FFFFFF) | (static_cast<uint32_t>(data & 0xFF00) << 16);
        }
        if (low) {
            v.oscAcc = (v.oscAcc & 0xFF00FFFF) | (static_cast<uint32_t>(data & 0x00FF) << 16);
        }
        break;
    case 0x0B:
        if (high) {
            v.oscAcc = (v.oscAcc & 0xFFFF00FF) | (data & 0xFF00);
        }
        if (low) {
            v.oscAcc = (v.oscAcc & 0xFFFFFF00) | (data & 0x00F8);
        }
        break;
    case 0x0C:
        if (high) {
            v.pan = static_cast<uint8_t>(data >> 8);
        }
        break;
    case 0x0D:
        if (high) {
            v.volCtrl = static_cast<uint8_t>((v.volCtrl & 0x80) | ((data >> 8) & 0x7F));
            RecalcIrq();
        }
        break;
    case 0x0E:
        if (high) {
            m_activeOsc = static_cast<uint8_t>((data >> 8) & 0x1F);
        }
        break;
    case 0x10:
        if (high) {
            if (trace && !((data >> 8) & 3)) {
                fprintf(stderr, "ics: voice %2d on: sample %02X:%06X-%06X conf %02X fc %04X vol %04X->%04X incr %02X ctl %02X pan %02X\n", m_oscSelect,
                        v.saddr, v.oscStart >> 12, v.oscEnd >> 12, v.oscConf, v.fc, v.volAcc >> 10, v.volEnd >> 10, v.volIncr, v.volCtrl, v.pan);
            }
            v.oscCtrl = static_cast<uint8_t>((data >> 8) & 3);
            // (a stop keys the envelope off too)
            if (v.oscCtrl & kStop) {
                v.volCtrl |= kStop;
            }
        }
        break;
    case 0x11:
        if (high) {
            v.saddr = static_cast<uint8_t>(data >> 8);
        }
        break;
    case 0x12:
        if (high) {
            v.volMode = static_cast<uint8_t>(data >> 8);
        }
        break;
    case 0x40:
    case 0x41:
        if (low) {
            m_timers[m_regSelect & 1].preset = static_cast<uint8_t>(data);
            RecalcTimer(m_regSelect & 1);
        }
        break;
    case 0x42:
    case 0x43:
        if (low) {
            m_timers[m_regSelect & 1].scale = static_cast<uint8_t>(data);
            RecalcTimer(m_regSelect & 1);
        }
        break;
    case 0x4A:
        if (low) {
            m_irqEnabled = static_cast<uint8_t>(data);
            RecalcIrq();
        }
        break;
    case 0x4F:
        if (low) {
            m_oscSelect = static_cast<uint8_t>((data & 0xFF) % (1 + m_activeOsc));
        }
        break;
    default: break;
    }
}

uint8_t Ics2115::Read(unsigned offset) {
    switch (offset & 3) {
    case 0: {
        uint8_t ret = 0;
        if (m_irqOn) {
            ret |= 0x80;
            if (m_irqEnabled && (m_irqPending & 3)) {
                ret |= 1;
            }
            for (int i = 0; i <= m_activeOsc; i++) {
                if (m_voices[i].oscConf & kPending) {
                    ret |= 2;
                    break;
                }
            }
        }
        return ret;
    }
    case 1: return m_regSelect;
    case 2: return static_cast<uint8_t>(RegRead());
    default: return static_cast<uint8_t>(RegRead() >> 8);
    }
}

void Ics2115::Write(unsigned offset, uint8_t value) {
    switch (offset & 3) {
    case 1: m_regSelect = value; break;
    case 2: RegWrite(value, 0x00FF); break;
    case 3: RegWrite(static_cast<uint16_t>(value << 8), 0xFF00); break;
    default: break;
    }
}

void Ics2115::RecalcIrq() {
    bool irq = (m_irqPending & m_irqEnabled) != 0;
    for (int i = 0; !irq && i < 32; i++) {
        const Voice &v = m_voices[i];
        irq |= (v.oscConf & kIrq) && (v.oscConf & kPending);
        irq |= (v.volCtrl & kIrq) && (v.volCtrl & kPending);
    }
    m_irqOn = irq;
}

void Ics2115::RecalcTimer(int timer) {
    Timer &t = m_timers[timer];
    uint64_t period = static_cast<uint64_t>((t.scale & 0x1F) + 1) * (t.preset + 1u);
    period <<= 4 + (t.scale >> 5);
    if (t.period != period) {
        t.period = period;
        t.next = m_now + period;
    }
}

int Ics2115::Playing(int voice) const {
    const Voice &v = m_voices[voice & 31];
    return v.Playing() ? static_cast<int>((static_cast<uint32_t>(v.saddr) << 20) | (v.oscStart >> 12)) : -1;
}

void Ics2115::Serialize(Archive &ar) {
    ar.Mark(0x49435332);
    for (Voice &v : m_voices) {
        ar.Pod(v);
    }
    ar.Pod(m_timers);
    ar.Pod(m_activeOsc);
    ar.Pod(m_oscSelect);
    ar.Pod(m_regSelect);
    ar.Pod(m_irqEnabled);
    ar.Pod(m_irqPending);
    ar.Pod(m_irqOn);
    ar.Pod(m_regs);
    ar.Pod(m_now);
    ar.Pod(m_nextSample);
}

} // namespace spelunk

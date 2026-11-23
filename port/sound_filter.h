// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp: https://github.com/tmcleroy/cps2recomp
//
// The menu's Sound filter (sound_filter): the board's sound softened before it is played. What the boards' sound
// chips hand over (the CV1000's YMZ770, the 68000 board's OKI M6295s and YMZ280B, the PGM's ICS2115) can be harsh in
// the treble: a hiss and grit that phone speakers hide, and headphones and good speakers do not. On the board the
// sound went on through a DAC and an amplifier that took some of that away; this is a low-pass in its place, by
// ear, not a model of those parts. Only what is heard: the game, recordings and online play are as they were (the
// frontend filters a frame's sound as it hands it to SDL, port/main.cpp's Sound, after everything else has it).
// (cps2recomp's, for one channel: these boards' sound is mono)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace port {

class SoundFilter {
public:
    // The choices, the first being off: how far down the treble is taken (Hz, at the 12 dB an octave of a
    // second-order Butterworth low-pass)
    static constexpr double kCutoff[] = {0, 8000, 5000};
    static constexpr int kChoices = 3;

    // One channel, filtered in place, at `rate` samples a second; `choice` the feature's value
    void Apply(int16_t *samples, size_t count, int choice, uint32_t rate) {
        choice = std::clamp(choice, 0, kChoices - 1);
        if (choice != m_choice || rate != m_rate) {
            Set(choice, rate);
        }
        if (!m_choice || !m_rate) {
            return;
        }
        for (size_t i = 0; i < count; i++) {
            // (direct form II transposed)
            const double in = samples[i];
            const double out = m_b0 * in + m_z1;
            m_z1 = m_b1 * in - m_a1 * out + m_z2;
            m_z2 = m_b2 * in - m_a2 * out;
            samples[i] = static_cast<int16_t>(std::clamp(std::lround(out), -32768L, 32767L));
        }
    }

private:
    void Set(int choice, uint32_t rate) {
        m_choice = choice;
        m_rate = rate;
        m_z1 = m_z2 = 0;
        if (!choice || !rate) {
            return;
        }
        // (the RBJ cookbook's low-pass, Q = 1/sqrt 2; the YMZ770's phrases can be as slow as 16 kHz, where the
        // cutoff is kept under the rate's half)
        const double w = 2 * 3.14159265358979323846 * std::min(kCutoff[choice], rate * 0.45) / rate;
        const double alpha = std::sin(w) / (2 * std::sqrt(0.5));
        const double a0 = 1 + alpha;
        m_b0 = m_b2 = (1 - std::cos(w)) / 2 / a0;
        m_b1 = (1 - std::cos(w)) / a0;
        m_a1 = -2 * std::cos(w) / a0;
        m_a2 = (1 - alpha) / a0;
    }

    int m_choice = 0;
    uint32_t m_rate = 0;
    double m_b0 = 1, m_b1 = 0, m_b2 = 0, m_a1 = 0, m_a2 = 0;
    double m_z1 = 0, m_z2 = 0;
};

} // namespace port

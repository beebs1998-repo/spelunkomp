// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The training tools' memory of a small board (Cave's 68000 boards: a third of a megabyte all told): a moment of
// play is the whole board written out (Board::Serialize), and the moments of the last minute or so are kept as the
// 4 KB pages of it that changed from one to the next. (The CV1000, with its 16 MB of RAM and its VRAM on the GPU,
// has runtime/timeline.h.)
#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace spelunk {

class Moments {
public:
    size_t limitMoments = 900;
    size_t limitBytes = 256u << 20;

    void Clear() {
        m_begun = false;
        m_moments.clear();
        m_bytes = 0;
        m_frames = 0;
    }
    // A frame was run since the last moment
    void Note() {
        m_frames++;
    }
    // A moment kept now: `state` is the board as it is
    void Keep(const std::vector<uint8_t> &state) {
        if (!m_begun || state.size() != m_last.size()) {
            Clear();
            m_last = state;
            m_begun = true;
            return;
        }
        Moment m;
        for (size_t at = 0; at < state.size(); at += kPage) {
            const size_t size = std::min(kPage, state.size() - at);
            if (std::memcmp(&m_last[at], &state[at], size) != 0) {
                m.pages.push_back(static_cast<uint32_t>(at / kPage));
                m.before.insert(m.before.end(), m_last.begin() + static_cast<ptrdiff_t>(at), m_last.begin() + static_cast<ptrdiff_t>(at + size));
                std::memcpy(&m_last[at], &state[at], size);
            }
        }
        m_frames = 0;
        m_bytes += m.before.size();
        m_moments.push_back(std::move(m));
        while (m_moments.size() > limitMoments || m_bytes > limitBytes) {
            // (the oldest goes: the timeline begins at the moment after it now)
            m_bytes -= m_moments.front().before.size();
            m_moments.pop_front();
        }
    }
    // Back to the last moment kept; from there, to the one before it. The board as it was then, or nullptr when
    // there is none further back
    const std::vector<uint8_t> *StepBack() {
        if (!m_begun || (!m_frames && m_moments.empty())) {
            return nullptr;
        }
        if (!m_frames) {
            const Moment &m = m_moments.back();
            size_t from = 0;
            for (uint32_t page : m.pages) {
                const size_t at = static_cast<size_t>(page) * kPage, size = std::min(kPage, m_last.size() - at);
                std::memcpy(&m_last[at], &m.before[from], size);
                from += size;
            }
            m_bytes -= m.before.size();
            m_moments.pop_back();
        }
        m_frames = 0;
        return &m_last;
    }
    bool Begun() const {
        return m_begun;
    }
    uint32_t Frames() const {
        return m_frames;
    }
    size_t Count() const {
        return m_moments.size();
    }
    size_t Bytes() const {
        return m_bytes + m_last.size();
    }

private:
    static constexpr size_t kPage = 0x1000;
    struct Moment {
        std::vector<uint32_t> pages; // the pages that differ from the moment before
        std::vector<uint8_t> before; // (as they were then)
    };
    bool m_begun = false;
    uint32_t m_frames = 0;       // (frames run since the last moment)
    std::vector<uint8_t> m_last; // the board at the last moment
    std::deque<Moment> m_moments;
    size_t m_bytes = 0;
};

} // namespace spelunk

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "timeline.h"

#include <algorithm>
#include <cstring>

namespace spelunk {

namespace {
    constexpr int kW = Blitter::kVramWidth;

    void BlockFrom(const std::vector<uint16_t> &vram, uint32_t block, uint16_t *out) {
        const VramArea a = Written::Area(block);
        for (int row = 0; row < a.h; row++) {
            std::memcpy(out + row * a.w, &vram[static_cast<size_t>(a.y + row) * kW + a.x], static_cast<size_t>(a.w) * 2);
        }
    }
    void BlockTo(std::vector<uint16_t> &vram, uint32_t block, const uint16_t *in) {
        const VramArea a = Written::Area(block);
        for (int row = 0; row < a.h; row++) {
            std::memcpy(&vram[static_cast<size_t>(a.y + row) * kW + a.x], in + row * a.w, static_cast<size_t>(a.w) * 2);
        }
    }
    // The blocks given sent to VRAM as `from` (all of VRAM) has them
    void Send(VramAccess &vram, const std::vector<uint32_t> &blocks, const std::vector<uint16_t> &from, std::vector<VramArea> &areas,
              std::vector<uint16_t> &pixels) {
        if (blocks.empty()) {
            return;
        }
        areas.clear();
        pixels.resize(blocks.size() * Written::kPixels);
        for (size_t i = 0; i < blocks.size(); i++) {
            areas.push_back(Written::Area(blocks[i]));
            BlockFrom(from, blocks[i], &pixels[i * Written::kPixels]);
        }
        vram.write(areas, pixels.data());
    }
} // namespace

void Written::Clear() {
    for (uint32_t b : m_list) {
        m_is[b] = 0;
    }
    m_list.clear();
}

void Written::All() {
    Clear();
    for (uint32_t b = 0; b < kBlocks; b++) {
        Add(b);
    }
}

void Written::Add(int x0, int y0, int x1, int y1) {
    x0 = std::max(x0, 0) / kBlock;
    y0 = std::max(y0, 0) / kBlock;
    x1 = std::min(x1, Blitter::kVramWidth - 1) / kBlock;
    y1 = std::min(y1, Blitter::kVramHeight - 1) / kBlock;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            Add(static_cast<uint32_t>(y * kAcross + x));
        }
    }
}

void Written::Note(const NativeList &list) {
    for (const NativeDraw &d : list.draws) {
        Add(d.x0, d.y0, d.x1, d.y1);
    }
    for (const NativeUpload &u : list.uploads) {
        if (u.width > 0 && u.height > 0) {
            Add(u.x, u.y, u.x + u.width - 1, u.y + u.height - 1);
        }
    }
}

void Point::Set(Cv1000 &board, VramAccess &vram) {
    m_ram.assign(board.ram.begin(), board.ram.begin() + static_cast<ptrdiff_t>(board.RamSize()));
    board.KeepMoment(m_small);
    vram.readAll(m_vram);
    m_written.Clear();
    frame = board.frame;
    m_set = true;
}

void Point::Back(Cv1000 &board, VramAccess &vram) {
    if (!m_set) {
        return;
    }
    board.SetMoment(m_small, m_ram.data());
    std::vector<VramArea> areas;
    m_sent = m_written.List();
    Send(vram, m_sent, m_vram, areas, m_pixels);
    m_written.Clear();
}

void Timeline::Clear() {
    m_begun = false;
    m_moments.clear();
    m_bytes = 0;
    m_frames = 0;
    m_written.Clear();
}

void Timeline::Keep(Cv1000 &board, VramAccess &vram) {
    const size_t ramSize = board.RamSize();
    if (!m_begun) {
        m_ram.assign(board.ram.begin(), board.ram.begin() + static_cast<ptrdiff_t>(ramSize));
        board.KeepMoment(m_first);
        vram.readAll(m_vram);
        m_written.Clear();
        m_frames = 0;
        m_begun = true;
        return;
    }
    Moment m;
    board.KeepMoment(m.small);
    for (size_t page = 0; page < ramSize >> 12; page++) {
        uint8_t *then = &m_ram[page << 12];
        const uint8_t *now = &board.ram[page << 12];
        if (std::memcmp(then, now, 0x1000) != 0) {
            m.pages.push_back(static_cast<uint32_t>(page));
            m.pagesBefore.insert(m.pagesBefore.end(), then, then + 0x1000);
            std::memcpy(then, now, 0x1000);
        }
    }
    m.blocks = m_written.List();
    if (!m.blocks.empty()) {
        m_areas.clear();
        for (uint32_t b : m.blocks) {
            m_areas.push_back(Written::Area(b));
        }
        m_pixels.resize(m.blocks.size() * Written::kPixels);
        vram.read(m_areas, m_pixels.data());
        m.blocksBefore.resize(m_pixels.size());
        for (size_t i = 0; i < m.blocks.size(); i++) {
            BlockFrom(m_vram, m.blocks[i], &m.blocksBefore[i * Written::kPixels]);
            BlockTo(m_vram, m.blocks[i], &m_pixels[i * Written::kPixels]);
        }
    }
    m_written.Clear();
    m_frames = 0;
    m_bytes += m.Bytes();
    m_moments.push_back(std::move(m));
    while (m_moments.size() > 1 && (m_moments.size() > limitMoments || m_bytes > limitBytes)) {
        // (the oldest goes: the timeline begins at it now)
        m_bytes -= m_moments.front().Bytes();
        m_first = std::move(m_moments.front().small);
        m_moments.pop_front();
    }
}

bool Timeline::StepBack(Cv1000 &board, VramAccess &vram) {
    if (!m_begun || (!m_frames && m_moments.empty())) {
        return false;
    }
    if (!m_frames) {
        // (at the last moment already: to the one before it)
        Moment &m = m_moments.back();
        for (size_t i = 0; i < m.pages.size(); i++) {
            std::memcpy(&m_ram[static_cast<size_t>(m.pages[i]) << 12], &m.pagesBefore[i << 12], 0x1000);
        }
        for (size_t i = 0; i < m.blocks.size(); i++) {
            BlockTo(m_vram, m.blocks[i], &m.blocksBefore[i * Written::kPixels]);
            m_written.Add(m.blocks[i]);
        }
        m_bytes -= m.Bytes();
        m_moments.pop_back();
    }
    board.SetMoment(m_moments.empty() ? m_first : m_moments.back().small, m_ram.data());
    Send(vram, m_written.List(), m_vram, m_areas, m_pixels);
    m_written.Clear();
    m_frames = 0;
    return true;
}

void Timeline::Jumped(Cv1000 &board, const Point &point) {
    if (!m_begun) {
        return;
    }
    // (VRAM here differs from the point's only in blocks written since the point was set, and those were just sent)
    for (uint32_t b : point.SentBlocks()) {
        const VramArea a = Written::Area(b);
        for (int row = 0; row < a.h; row++) {
            const size_t at = static_cast<size_t>(a.y + row) * kW + a.x;
            std::memcpy(&m_vram[at], &point.Vram()[at], static_cast<size_t>(a.w) * 2);
        }
    }
    std::memcpy(m_ram.data(), point.Ram().data(), m_ram.size());
    board.KeepMoment(m_first);
    m_moments.clear();
    m_bytes = 0;
    m_written.Clear();
    m_frames = 0;
}

} // namespace spelunk

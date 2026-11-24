// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "timeline.h"

#include <algorithm>
#include <cstdint>
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
    m_readsKept = 0;
    m_frames = 0;
    m_written.Clear();
}

void Timeline::DropOldest() {
    // (the timeline begins at it now)
    const Moment &m = m_moments.front();
    m_first.assign(&m_store[m.at], &m_store[m.at] + m.small);
    m_moments.pop_front();
}

size_t Timeline::Place(size_t bytes) {
    if (bytes > m_storeSize) {
        return SIZE_MAX;
    }
    for (;;) {
        if (m_moments.empty()) {
            return 0;
        }
        const size_t oldest = m_moments.front().at, end = m_moments.back().at + m_moments.back().size;
        if (m_moments.back().at >= oldest) {
            // (the newest after the oldest: room after it, else at the store's start, before the oldest)
            if (m_storeSize - end >= bytes) {
                return end;
            }
            if (oldest >= bytes) {
                return 0;
            }
        } else if (oldest - end >= bytes) {
            // (gone round: room between the newest and the oldest)
            return end;
        }
        DropOldest();
    }
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
        // (the store: what limitBytes leaves beside RAM and VRAM as kept here, the board's moment, and a part for what
        // a moment's VRAM is read through, kept here and by VRAM's reads)
        constexpr size_t kReads = 16u << 20;
        const size_t beside = m_ram.capacity() + m_vram.capacity() * 2 + m_first.capacity() + kReads;
        const size_t size = limitBytes > beside ? limitBytes - beside : 0;
        if (size != m_storeSize) {
            m_store.reset(size ? new uint8_t[size] : nullptr);
            m_storeSize = size;
            m_storeUsed = 0;
        }
        return;
    }
    board.KeepMoment(m_small);
    m_pages.clear();
    for (size_t page = 0; page < ramSize >> 12; page++) {
        if (std::memcmp(&m_ram[page << 12], &board.ram[page << 12], 0x1000) != 0) {
            m_pages.push_back(static_cast<uint32_t>(page));
        }
    }
    const std::vector<uint32_t> &blocks = m_written.List();
    Moment m;
    m.small = static_cast<uint32_t>(m_small.size());
    m.pages = static_cast<uint32_t>(m_pages.size());
    m.blocks = static_cast<uint32_t>(blocks.size());
    m.size = Aligned(m.BlocksBeforeAt() - m.at + static_cast<size_t>(m.blocks) * Written::kPixels * 2);
    while (m_moments.size() >= limitMoments) {
        DropOldest();
    }
    m.at = Place(m.size);
    if (m.at == SIZE_MAX) {
        // (a moment larger than the whole store: the timeline begins again from here)
        Clear();
        Keep(board, vram);
        return;
    }
    m_storeUsed = std::max(m_storeUsed, m.at + m.size);
    uint8_t *const out = &m_store[0];
    std::memcpy(out + m.at, m_small.data(), m_small.size());
    std::memcpy(out + m.PagesAt(), m_pages.data(), m_pages.size() * 4);
    // (the pages as they were)
    for (size_t i = 0; i < m_pages.size(); i++) {
        uint8_t *then = &m_ram[static_cast<size_t>(m_pages[i]) << 12];
        std::memcpy(out + m.BeforeAt() + (i << 12), then, 0x1000);
        std::memcpy(then, &board.ram[static_cast<size_t>(m_pages[i]) << 12], 0x1000);
    }
    if (!blocks.empty()) {
        std::memcpy(out + m.BlocksAt(), blocks.data(), blocks.size() * 4);
        m_areas.clear();
        for (uint32_t b : blocks) {
            m_areas.push_back(Written::Area(b));
        }
        m_pixels.resize(blocks.size() * Written::kPixels);
        vram.read(m_areas, m_pixels.data());
        uint16_t *before = reinterpret_cast<uint16_t *>(out + m.BlocksBeforeAt());
        for (size_t i = 0; i < blocks.size(); i++) {
            BlockFrom(m_vram, blocks[i], before + i * Written::kPixels);
            BlockTo(m_vram, blocks[i], &m_pixels[i * Written::kPixels]);
        }
    }
    m_written.Clear();
    m_frames = 0;
    m_readsKept = vram.kept ? vram.kept() : 0;
    m_moments.push_back(m);
}

size_t Timeline::Bytes() const {
    return m_storeUsed + m_ram.capacity() + m_vram.capacity() * 2 + m_first.capacity() + m_small.capacity() + m_pages.capacity() * 4 +
           m_pixels.capacity() * 2 + m_areas.capacity() * sizeof(VramArea) + m_readsKept;
}

bool Timeline::StepBack(Cv1000 &board, VramAccess &vram) {
    if (!m_begun || (!m_frames && m_moments.empty())) {
        return false;
    }
    if (!m_frames) {
        // (at the last moment already: to the one before it)
        const Moment &m = m_moments.back();
        const uint8_t *in = &m_store[0];
        for (size_t i = 0; i < m.pages; i++) {
            uint32_t page;
            std::memcpy(&page, in + m.PagesAt() + i * 4, 4);
            std::memcpy(&m_ram[static_cast<size_t>(page) << 12], in + m.BeforeAt() + (i << 12), 0x1000);
        }
        const uint16_t *before = reinterpret_cast<const uint16_t *>(in + m.BlocksBeforeAt());
        for (size_t i = 0; i < m.blocks; i++) {
            uint32_t block;
            std::memcpy(&block, in + m.BlocksAt() + i * 4, 4);
            BlockTo(m_vram, block, before + i * Written::kPixels);
            m_written.Add(block);
        }
        m_moments.pop_back();
    }
    if (m_moments.empty()) {
        board.SetMoment(m_first, m_ram.data());
    } else {
        const Moment &m = m_moments.back();
        m_small.assign(&m_store[m.at], &m_store[m.at] + m.small);
        board.SetMoment(m_small, m_ram.data());
    }
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
    m_written.Clear();
    m_frames = 0;
}

} // namespace spelunk

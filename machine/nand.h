// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// U2: the 128 MB NAND flash (Samsung K9F1G08U0M) that holds a CV1000 game's graphics, and on some games its
// settings and scores. 65536 pages of 2048 data bytes and 64 spare bytes, 64 pages to a block. The player's file is
// the chip's whole contents, spare bytes included; it is only read (what the game programs or erases is kept in
// memory and saved with the player's data).
#pragma once

#include "../runtime/state.h"

#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

namespace spelunk {

class Nand {
public:
    static constexpr uint32_t kPageData = 2048, kPageTotal = 2048 + 64, kPages = 64 * 1024, kBlockPages = 64;
    static constexpr size_t kSize = static_cast<size_t>(kPageTotal) * kPages;

    std::vector<uint8_t> data;     // the chip's contents (kSize bytes)
    std::set<uint32_t> written;    // blocks the game has programmed or erased (saved with the player's data)

    void Command(uint8_t c) {
        switch (c) {
        case 0xFF:
            m_mode = Mode::Idle;
            m_status = 0xC0;
            break;
        case 0x00:
            m_mode = Mode::Read;
            m_addressCount = 0;
            break;
        case 0x30:
            if (m_mode == Mode::Read && m_addressCount >= 4) {
                m_at = static_cast<size_t>(m_page) * kPageTotal + m_column;
                m_mode = Mode::Reading;
                reads++;
            }
            break;
        case 0x05: // (another column of the page being read)
            m_mode = Mode::Column;
            m_addressCount = 0;
            break;
        case 0xE0:
            if (m_mode == Mode::Column) {
                m_at = static_cast<size_t>(m_page) * kPageTotal + m_column;
                m_mode = Mode::Reading;
            }
            break;
        case 0x80:
            m_mode = Mode::Program;
            m_addressCount = 0;
            m_programmed = 0;
            std::memset(m_pageRegister, 0xFF, sizeof m_pageRegister);
            break;
        case 0x10:
            if (m_mode == Mode::Program) {
                if (m_page < kPages) {
                    uint8_t *page = &data[static_cast<size_t>(m_page) * kPageTotal];
                    for (uint32_t i = 0; i < kPageTotal; i++) {
                        page[i] &= m_pageRegister[i];
                    }
                    written.insert(m_page / kBlockPages);
                }
                m_status = 0xC0;
            }
            m_mode = Mode::Idle;
            break;
        case 0x60:
            m_mode = Mode::Erase;
            m_addressCount = 0;
            m_page = 0;
            break;
        case 0xD0:
            if (m_mode == Mode::Erase && m_page < kPages) {
                const uint32_t first = m_page & ~(kBlockPages - 1);
                std::memset(&data[static_cast<size_t>(first) * kPageTotal], 0xFF, static_cast<size_t>(kBlockPages) * kPageTotal);
                written.insert(first / kBlockPages);
                m_status = 0xC0;
            }
            m_mode = Mode::Idle;
            break;
        case 0x70:
            m_mode = Mode::Status;
            break;
        case 0x90:
            m_mode = Mode::Id;
            m_idAt = 0;
            break;
        default:
            m_mode = Mode::Idle;
            break;
        }
    }
    void Address(uint8_t a) {
        switch (m_mode) {
        case Mode::Read:
        case Mode::Program:
            // (two column bytes, then two page bytes)
            if (m_addressCount == 0) {
                m_column = a;
                m_page = 0;
            } else if (m_addressCount == 1) {
                m_column |= (a & 0x0F) << 8;
            } else if (m_addressCount < 4) {
                m_page |= static_cast<uint32_t>(a) << (8 * (m_addressCount - 2));
            }
            m_addressCount++;
            break;
        case Mode::Column:
            if (m_addressCount == 0) {
                m_column = a;
            } else if (m_addressCount == 1) {
                m_column |= (a & 0x0F) << 8;
            }
            m_addressCount++;
            break;
        case Mode::Erase:
            if (m_addressCount < 2) {
                m_page |= static_cast<uint32_t>(a) << (8 * m_addressCount);
            }
            m_addressCount++;
            break;
        default:
            break;
        }
    }
    uint8_t Read() {
        switch (m_mode) {
        case Mode::Reading:
            return m_at < data.size() ? data[m_at++] : 0xFF;
        case Mode::Status:
            return m_status;
        case Mode::Id: {
            static const uint8_t kId[4] = {0xEC, 0xF1, 0x00, 0x15};
            return kId[m_idAt++ & 3];
        }
        default:
            return 0xFF;
        }
    }
    void Write(uint8_t v) {
        if (m_mode == Mode::Program) {
            const uint32_t at = m_column + m_programmed++;
            if (at < kPageTotal) {
                m_pageRegister[at] = v;
            }
        }
    }
    uint64_t reads = 0; // page reads started (the checks: how much loading a frame did)

    void Serialize(Archive &ar) {
        ar.Mark(0x4E414E44);
        ar.Pod(m_mode);
        ar.Pod(m_status);
        ar.Pod(m_addressCount);
        ar.Pod(m_column);
        ar.Pod(m_page);
        ar.Pod(m_at);
        ar.Pod(m_idAt);
        ar.Pod(m_programmed);
        ar.Pod(m_pageRegister);
        // (the blocks the game has written, with their contents)
        std::vector<uint32_t> blocks(written.begin(), written.end());
        ar.Vec(blocks);
        if (!ar.Saving()) {
            written = std::set<uint32_t>(blocks.begin(), blocks.end());
        }
        for (uint32_t b : blocks) {
            if (b < kPages / kBlockPages) {
                ar.Bytes(&data[static_cast<size_t>(b) * kBlockPages * kPageTotal], static_cast<size_t>(kBlockPages) * kPageTotal);
            }
        }
    }

private:
    enum class Mode : uint8_t { Idle, Read, Reading, Column, Program, Erase, Status, Id };
    Mode m_mode = Mode::Idle;
    uint8_t m_status = 0xC0; // (ready, not write-protected)
    uint32_t m_addressCount = 0, m_column = 0, m_page = 0, m_idAt = 0, m_programmed = 0;
    uint64_t m_at = 0;
    uint8_t m_pageRegister[kPageTotal] = {};
};

} // namespace spelunk

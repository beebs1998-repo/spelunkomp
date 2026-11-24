// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What the training tools keep of the game as it is played, small enough to keep a great many and quick enough to
// keep and go back to between two frames:
//
//   Point      one place in play kept whole (the practice point), gone back to as often as wanted
//   Timeline   a moment every few frames of the last minute or so, stepped back through (rewind)
//
// Main RAM is kept by its 4 KB pages and VRAM by blocks of 64 x 64 pixels: the lists the game ran say which blocks
// were written (machine/blitter.h's NativeList), so only those are brought from the GPU or sent back to it. Neither
// touches the game: with both in use and never gone back to, it plays as without them.
#pragma once

#include "../machine/cv1000.h"

#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace spelunk {

// VRAM where it is (the GPU's texture, or the software blitter's array)
struct VramAccess {
    std::function<void(const std::vector<VramArea> &, uint16_t *)> read;
    std::function<void(const std::vector<VramArea> &, const uint16_t *)> write;
    std::function<void(std::vector<uint16_t> &)> readAll;
    // What the reads keep beside it (the GPU's transfer buffer they come back through, as large as the largest read;
    // none for the software blitter's array): counted in the timeline's bytes
    std::function<size_t()> kept;
};

// The blocks of VRAM written
class Written {
public:
    static constexpr int kBlock = 64, kAcross = Blitter::kVramWidth / kBlock, kDown = Blitter::kVramHeight / kBlock;
    static constexpr int kBlocks = kAcross * kDown, kPixels = kBlock * kBlock;
    Written()
        : m_is(kBlocks, 0) {}
    void Clear();
    void All();
    void Add(uint32_t block) {
        if (!m_is[block]) {
            m_is[block] = 1;
            m_list.push_back(block);
        }
    }
    void Add(int x0, int y0, int x1, int y1); // (pixels, inclusive)
    void Note(const NativeList &list);
    const std::vector<uint32_t> &List() const {
        return m_list;
    }
    static VramArea Area(uint32_t block) {
        return {static_cast<int>(block % kAcross) * kBlock, static_cast<int>(block / kAcross) * kBlock, kBlock, kBlock};
    }

private:
    std::vector<uint8_t> m_is;
    std::vector<uint32_t> m_list;
};

class Point {
public:
    bool IsSet() const {
        return m_set;
    }
    void Clear() {
        m_set = false;
    }
    void Set(Cv1000 &board, VramAccess &vram);
    // What the game wrote since (every frame's lists while a point is set)
    void Note(const NativeList &list) {
        if (m_set) {
            m_written.Note(list);
        }
    }
    // VRAM is no longer what the notes say (a state loaded, the board reset): all of it is sent at the next Back
    void Lost() {
        m_written.All();
    }
    void Back(Cv1000 &board, VramAccess &vram);
    uint64_t frame = 0; // (the board's frame count at the point)

    // (for the Timeline, which goes on from the point after a Back)
    const std::vector<uint8_t> &Ram() const {
        return m_ram;
    }
    const std::vector<uint16_t> &Vram() const {
        return m_vram;
    }
    const std::vector<uint32_t> &SentBlocks() const {
        return m_sent;
    }

private:
    bool m_set = false;
    std::vector<uint8_t> m_ram, m_small;
    std::vector<uint16_t> m_vram, m_pixels;
    std::vector<uint32_t> m_sent;
    Written m_written;
};

class Timeline {
public:
    size_t limitMoments = 900;
    size_t limitBytes = 384u << 20;

    void Clear();
    void Note(const NativeList &list) {
        m_written.Note(list);
        m_frames++;
    }
    // A moment kept now (the first after a Clear begins the timeline: all of VRAM is brought once)
    void Keep(Cv1000 &board, VramAccess &vram);
    // Back to the last moment kept; from there, to the one before it. False when there is none further back
    bool StepBack(Cv1000 &board, VramAccess &vram);
    // The board was put at a Point: the timeline begins again there
    void Jumped(Cv1000 &board, const Point &point);
    bool Begun() const {
        return m_begun;
    }
    uint32_t Frames() const {
        return m_frames;
    }
    size_t Moments() const {
        return m_moments.size();
    }
    // What the timeline holds, all told: the store its moments are kept in, as much of it as has been used (the
    // memory the system has given it), the RAM and VRAM it keeps beside them, and what VRAM's reads keep for it
    // (limitBytes is of this)
    size_t Bytes() const;

private:
    // The moments are kept one after another in one store, made at the timeline's beginning as large as limitBytes
    // leaves beside the rest, and used round and round: the oldest goes to make room, as the newest goes back. So the
    // memory they take is the store's and no more: no allocation for each moment, whose freed places the allocator
    // could not always use again (about 10% more than the moments' own bytes, uncounted, before)
    struct Moment {
        size_t at = 0, size = 0; // (its place in the store: the board but RAM and VRAM, then the pages of RAM that differ
                                 // from the moment before and those pages as they were, then the blocks of VRAM written
                                 // since the moment before and those blocks as they were)
        uint32_t small = 0, pages = 0, blocks = 0;
        size_t PagesAt() const {
            return at + Aligned(small);
        }
        size_t BeforeAt() const {
            return PagesAt() + pages * 4;
        }
        size_t BlocksAt() const {
            return BeforeAt() + static_cast<size_t>(pages) * 0x1000;
        }
        size_t BlocksBeforeAt() const {
            return BlocksAt() + blocks * 4;
        }
    };
    static size_t Aligned(size_t bytes) {
        return (bytes + 7) & ~static_cast<size_t>(7);
    }
    // A place for `bytes` in the store, the oldest moments let go until there is one; SIZE_MAX if none can be
    size_t Place(size_t bytes);
    void DropOldest();
    bool m_begun = false;
    uint32_t m_frames = 0;           // (frames run since the last moment)
    std::vector<uint8_t> m_ram;      // RAM at the last moment
    std::vector<uint16_t> m_vram;    // VRAM at the last moment
    std::vector<uint8_t> m_first;    // the board at the timeline's beginning (when no moment is left)
    std::deque<Moment> m_moments;
    std::unique_ptr<uint8_t[]> m_store; // (not cleared when made: the system gives its memory only as it is written)
    size_t m_storeSize = 0, m_storeUsed = 0; // (its size, and how far into it has ever been written)
    std::vector<uint8_t> m_small;      // (a moment's board as it is kept, and as it is given back)
    std::vector<uint32_t> m_pages;
    Written m_written;
    std::vector<VramArea> m_areas;
    std::vector<uint16_t> m_pixels;
    size_t m_readsKept = 0; // (VramAccess::kept, as the last moment left it)
};

} // namespace spelunk

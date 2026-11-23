// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The native renderer (docs/NATIVE.md): VRAM is a texture on the GPU, the game's lists (machine/blitter.h's
// NativeList) are run there by a shader that does the board's own arithmetic, and the screen is drawn from it. The
// software blitter draws nothing while this is in use; it stays as the reference the check compares with.
#pragma once

#include "../machine/blitter.h"
#include "gpu_base.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace spelunk {

class Gpu : public GpuBase {
public:
    ~Gpu();
    bool Init(SDL_Window *window);
    void Shutdown();

    // All of VRAM sent to the GPU (the start, a reset, a save state loaded), or brought back (a save state made)
    void SetVram(const std::vector<uint16_t> &vram);
    void GetVram(std::vector<uint16_t> &vram);

    // Runs a frame's lists (in the game's order)
    void Run(const NativeList &list);

    // Parts of VRAM brought back or sent, each `w` x `h` pixels, their rows one after another in `pixels` in the
    // areas' order (the moments the training tools keep: runtime/timeline.h)
    void ReadAreas(const std::vector<VramArea> &areas, uint16_t *pixels);
    void WriteAreas(const std::vector<VramArea> &areas, const uint16_t *pixels);

    // Run-ahead: from BeginJournal the areas written are kept first as they were; Rollback puts them back. Two journals
    // can be kept at once, each from its own moment (online play's rollback: port/netplay.h); EndJournal ends one
    // without putting anything back
    static constexpr int kJournals = 2;
    void BeginJournal(int which = 0);
    void Rollback(int which = 0);
    void EndJournal(int which);

    // Shows the screen (the window of VRAM at the scroll position, turned `turn` quarter turns clockwise) in the
    // rectangle given, in the window's real pixels, with the overlay (GpuBase::SetOverlay) over it
    void PresentScreen(uint32_t scrollX, uint32_t scrollY, int turn, const SDL_FRect &to);

    // The screen's 320 x 240 pixels as the software blitter's Picture gives them, read back (screenshots, checks, the
    // picture the menu is drawn over)
    void ReadScreen(uint32_t scrollX, uint32_t scrollY, uint32_t *out);

    // The screen as PresentScreen draws it, into pixels here instead of the window (checks: the window's own path,
    // with no window shown): `width` x `height` pixels, 32 bits each, red in the low byte
    void ScreenToPixels(uint32_t scrollX, uint32_t scrollY, int turn, int width, int height, std::vector<uint32_t> &out);

    // What the last Run did (the checks and the readouts)
    uint32_t passes = 0, drawsRun = 0, uploadsRun = 0, longestTile = 0;

private:
    struct Rect {
        int x0, y0, x1, y1;
    };
    SDL_GPUTexture *m_vram = nullptr, *m_work = nullptr, *m_backup = nullptr, *m_backup2 = nullptr;
    SDL_GPUTexture *m_draws = nullptr, *m_tiles = nullptr, *m_lists = nullptr;
    SDL_GPUGraphicsPipeline *m_listPipeline = nullptr;
    bool m_journal[kJournals] = {};
    std::vector<Rect> m_kept[kJournals];
    std::vector<int32_t> m_tileRuns, m_listData, m_drawData;
    std::vector<uint32_t> m_counts;

    void Keep(SDL_GPUCommandBuffer *cmd, const Rect &area);
    void RunDraws(SDL_GPUCommandBuffer *cmd, const NativeDraw *draws, uint32_t count);
};

} // namespace spelunk

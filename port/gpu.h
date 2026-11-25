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

    // The input lag readout's looks (port/hw_cv1000.h): VRAM copied whole into a texture of its own (one of two:
    // SetVramAside), and VRAM and that one changing places (TradeVram: no copy); DropVramAside lets both go
    static constexpr int kAsides = 2;
    void SetVramAside(int which);
    void TradeVram(int which);
    void DropVramAside();

    // Shows the screen (the window of VRAM at the scroll position, turned `turn` quarter turns clockwise) in the
    // rectangle given, in the window's real pixels, with the overlay (GpuBase::SetOverlay) over it
    void PresentScreen(uint32_t scrollX, uint32_t scrollY, int turn, const SDL_FRect &to);

    // The screen's 320 x 240 pixels as the software blitter's Picture gives them, read back (screenshots, checks, the
    // picture the menu is drawn over)
    void ReadScreen(uint32_t scrollX, uint32_t scrollY, uint32_t *out);
    // The same without waiting (the input lag readout's looks): started, and ended a frame on (GpuBase::StartRead)
    void StartScreenRead(int which, uint32_t scrollX, uint32_t scrollY);
    bool EndScreenRead(int which, uint32_t *out);

    // The screen as PresentScreen draws it, into pixels here instead of the window (checks: the window's own path,
    // with no window shown): `width` x `height` pixels, 32 bits each, red in the low byte
    void ScreenToPixels(uint32_t scrollX, uint32_t scrollY, int turn, int width, int height, std::vector<uint32_t> &out);

    // What the last Run did (the checks and the readouts)
    uint32_t passes = 0, drawsRun = 0, uploadsRun = 0, longestTile = 0;

private:
    struct Rect {
        int x0, y0, x1, y1;
    };
    // VRAM is one of two textures (m_vram), the other (m_work) what the next pass draws into, then VRAM in turn: the
    // two are the same but in m_stale, the areas the other one has not had yet (the last pass's, and what was written
    // into VRAM since), which the next pass brings first; m_staleAll: all of it (one copy). Everything that reads VRAM
    // reads m_vram, whichever texture it is now
    SDL_GPUTexture *m_vram = nullptr, *m_work = nullptr, *m_backup = nullptr, *m_backup2 = nullptr;
    std::vector<Rect> m_stale;
    bool m_staleAll = true;
    void Stale(const Rect &area);
    // A frame's passes, planned before any is recorded: their draws, lists and tiles one after another in the three
    // data textures (sent in one go: Flush), and the game's own uploads among them in order
    struct Step {
        bool upload;
        Rect area;
        int tilesW;
        uint32_t tileAt, pixelsAt; // (the pass's first tile; an upload's pixels in m_pixels)
    };
    std::vector<Step> m_steps;
    std::vector<NativeDraw> m_planDraws;
    std::vector<int32_t> m_planLists, m_planTiles;
    std::vector<uint16_t> m_pixels;
    void PlanDraws(SDL_GPUCommandBuffer *cmd, const NativeDraw *draws, uint32_t count);
    void Flush(SDL_GPUCommandBuffer *cmd);
    // (each aside with a work texture of its own and what that is behind by, traded with VRAM's: a look's frames,
    // VRAM and an aside by turns each frame, then copy no more than their passes' areas)
    SDL_GPUTexture *m_aside[kAsides] = {}, *m_asideWork[kAsides] = {};
    std::vector<Rect> m_asideStale[kAsides];
    bool m_asideStaleAll[kAsides] = {};
    int m_readW[kReads] = {}, m_readH[kReads] = {};
    std::vector<uint16_t> m_readPart;
    SDL_GPUTexture *m_draws = nullptr, *m_tiles = nullptr, *m_lists = nullptr;
    SDL_GPUGraphicsPipeline *m_listPipeline = nullptr;
    bool m_journal[kJournals] = {};
    std::vector<Rect> m_kept[kJournals];
    std::vector<uint32_t> m_counts;

    void Keep(SDL_GPUCommandBuffer *cmd, const Rect &area);
};

} // namespace spelunk

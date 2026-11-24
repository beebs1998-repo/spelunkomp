// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The native renderer of Cave's 68000 boards (docs/NATIVE.md): the frame the video hands on (machine/cave_video.h
// NativeFrame68k: the sprites read from the chip's list at its latch, the layers, their registers and the palette as
// the picture is made) drawn by the GPU. The sprites as quads in the list's order into a sprite buffer of the chip's
// kind (port/shaders/sprites68k.*), then for each screen pixel the layers, the priorities and the palette
// (compose68k.frag). The software model (machine/cave_video.cpp) stays as the reference the check compares with.
#pragma once

#include "../machine/cave_video.h"
#include "gpu_base.h"

namespace spelunk {

class Gpu68k : public GpuBase {
public:
    ~Gpu68k();
    // The video's tiles and sprites (from the set: they never change) go to the GPU here
    bool Init(SDL_Window *window, const CaveVideo &video);
    void Shutdown();

    // The frame made into the picture (what of it is new: the sprites since the last latch, the rest since the last
    // picture)
    void Render(const CaveVideo &video, const NativeFrame68k &frame);

    // Shows the picture turned `turn` quarter turns clockwise in the rectangle given, in the window's real pixels, with
    // the overlay over it; or into pixels here, `width` x `height` (the checks' shot of the window's own path)
    void PresentScreen(int turn, const SDL_FRect &to);
    void ScreenToPixels(int turn, int width, int height, std::vector<uint32_t> &out);
    // The picture's pixels (320 x 240; Mazinger Z's board 384 across), 32 bits each, red in the low byte (screenshots, the checks, the menu's background)
    void ReadScreen(uint32_t *out);
    // The same without waiting (the input lag readout's looks): started, and ended a frame on (GpuBase::StartRead)
    void StartScreenRead(int which);
    bool EndScreenRead(int which, uint32_t *out);

    // What the last Render drew (the checks)
    uint32_t spritesDrawn = 0;

private:
    SDL_GPUTexture *m_spritePixels = nullptr, *m_tilePixels = nullptr, *m_spriteData = nullptr, *m_spriteBuffer = nullptr;
    SDL_GPUTexture *m_layers = nullptr, *m_palette = nullptr, *m_picture = nullptr;
    SDL_GPUGraphicsPipeline *m_spritePipeline = nullptr, *m_composePipeline = nullptr;
    int32_t m_tileBase[CaveVideo::kLayers] = {}, m_tileCount[CaveVideo::kLayers] = {};
    uint32_t m_spriteMask = 0;
    std::vector<int32_t> m_spriteWords;

    // A byte-a-pixel texture 4096 wide holding `bytes`, sent once
    SDL_GPUTexture *MakePixels(const std::vector<uint8_t> &bytes);
};

} // namespace spelunk

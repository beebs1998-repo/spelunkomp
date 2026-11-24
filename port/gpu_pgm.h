// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The native renderer of IGS's PGM board (docs/PGM.md): the video chip's state at the vertical blank (its memory, its
// registers, the sprites on show) drawn by the GPU. The ROMs go to the GPU once, with a count of the clear mask bits
// before every word of the sprites' B ROM, so that each sprite pixel finds its place in the packed A ROM directly;
// the sprites as quads in the list's order into a sprite buffer (port/shaders/sprites_pgm.*), then for each screen
// pixel the background, the sprite and the text (compose_pgm.frag). The software model (machine/igs023.cpp) stays as
// the reference the check compares with.
#pragma once

#include "../machine/igs023.h"
#include "gpu_base.h"

namespace spelunk {

class GpuPgm : public GpuBase {
public:
    ~GpuPgm();
    // The video's ROMs (from the set: they never change) go to the GPU here
    bool Init(SDL_Window *window, const Igs023 &video);
    void Shutdown();

    // The picture of the chip as it is now (its memory and registers, the sprites on show)
    void Render(const Igs023 &video);

    void PresentScreen(int turn, const SDL_FRect &to);
    void ScreenToPixels(int turn, int width, int height, std::vector<uint32_t> &out);
    // The picture's 448 x 224 pixels, 32 bits each, red in the low byte
    void ReadScreen(uint32_t *out);
    // The same without waiting (the input lag readout's looks): started, and ended a frame on (GpuBase::StartRead)
    void StartScreenRead(int which);
    bool EndScreenRead(int which, uint32_t *out);

    uint32_t spritesDrawn = 0;

private:
    SDL_GPUTexture *m_tiles = nullptr, *m_pixels = nullptr, *m_masks = nullptr, *m_counts = nullptr, *m_spriteData = nullptr;
    SDL_GPUTexture *m_spriteBuffer = nullptr, *m_vram = nullptr, *m_picture = nullptr;
    SDL_GPUGraphicsPipeline *m_spritePipeline = nullptr, *m_composePipeline = nullptr;
    int32_t m_rom[4] = {};
    int32_t m_bgTiles = 1, m_textTiles = 1;
    std::vector<int32_t> m_spriteTexels;

    // `bytes` as a texture 4096 texels wide of `format` (`texel` bytes each), sent once
    SDL_GPUTexture *MakeRom(const void *bytes, size_t size, SDL_GPUTextureFormat format, uint32_t texel);
};

} // namespace spelunk

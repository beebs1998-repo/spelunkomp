// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "gpu.h"

#include "shaders.inc"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace spelunk {

namespace {
    constexpr int kW = Blitter::kVramWidth, kH = Blitter::kVramHeight;
    constexpr int kDataSize = 2048; // the draws' and the lists' textures are this wide and tall
    constexpr int kTileRows = 512; // the tiles' texture: kDataSize wide, this tall (a frame's passes' tiles one after another)

} // namespace

Gpu::~Gpu() {
    Shutdown();
}

void Gpu::Shutdown() {
    if (!m_device) {
        return;
    }
    SDL_WaitForGPUIdle(m_device);
    DropVramAside();
    for (SDL_GPUTexture *t : {m_vram, m_work, m_backup, m_backup2, m_draws, m_tiles, m_lists}) {
        if (t) {
            SDL_ReleaseGPUTexture(m_device, t);
        }
    }
    for (SDL_GPUGraphicsPipeline *p : {m_listPipeline}) {
        if (p) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, p);
        }
    }
    m_vram = m_work = m_backup = m_backup2 = m_draws = m_tiles = m_lists = nullptr;
    m_listPipeline = nullptr;
    Close();
}

bool Gpu::Init(SDL_Window *window) {
    if (!Open(window)) {
        return false;
    }
    const SDL_GPUTextureUsageFlags sampled = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    // (VRAM and the work texture take turns: both drawn into, both read)
    m_vram = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, sampled | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    m_work = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, sampled | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    m_draws = MakeTexture(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT, kDataSize, kDataSize, sampled);
    m_lists = MakeTexture(SDL_GPU_TEXTUREFORMAT_R32_INT, kDataSize, kDataSize, sampled);
    m_tiles = MakeTexture(SDL_GPU_TEXTUREFORMAT_R32G32_INT, kDataSize, kTileRows, sampled);
    SDL_GPUShader *list = MakeShader(k_list_frag_spirv, sizeof k_list_frag_spirv, k_list_frag_msl, true, 4, 1);
    SDL_GPUShader *screen = MakeShader(k_screen_frag_spirv, sizeof k_screen_frag_spirv, k_screen_frag_msl, true, 1, 1);
    bool ok = m_vram && m_work && m_draws && m_lists && m_tiles && list && screen;
    if (ok) {
        m_listPipeline = MakePipeline(m_fullVertex, list, SDL_GPU_TEXTUREFORMAT_R16_UINT);
        // (the base keeps the screen's shader: the window's pipeline, made again for another window)
        const bool screenOk = SetScreenShader(screen);
        screen = nullptr;
        ok = m_listPipeline && screenOk;
    }
    for (SDL_GPUShader *s : {list, screen}) {
        if (s) {
            SDL_ReleaseGPUShader(m_device, s);
        }
    }
    if (!ok) {
        fprintf(stderr, "gpu: could not be set up: %s\n", SDL_GetError());
        Shutdown();
        return false;
    }
    return true;
}

namespace {
    // Sends `rows` rows of `width` texels (each `texel` bytes) from the transfer buffer to a texture
    void UploadFrom(SDL_GPUCopyPass *copy, SDL_GPUTransferBuffer *from, SDL_GPUTexture *to, int x, int y, int width, int rows, uint32_t offset = 0) {
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = from;
        source.offset = offset;
        source.pixels_per_row = static_cast<Uint32>(width);
        source.rows_per_layer = static_cast<Uint32>(rows);
        SDL_GPUTextureRegion region{};
        region.texture = to;
        region.x = static_cast<Uint32>(x);
        region.y = static_cast<Uint32>(y);
        region.w = static_cast<Uint32>(width);
        region.h = static_cast<Uint32>(rows);
        region.d = 1;
        // (never "cycled": that would throw the rest of the texture away, and VRAM keeps what is not written)
        SDL_UploadToGPUTexture(copy, &source, &region, false);
    }
    void CopyArea(SDL_GPUCopyPass *copy, SDL_GPUTexture *from, SDL_GPUTexture *to, int x, int y, int w, int h) {
        SDL_GPUTextureLocation a{}, b{};
        a.texture = from;
        b.texture = to;
        a.x = b.x = static_cast<Uint32>(x);
        a.y = b.y = static_cast<Uint32>(y);
        SDL_CopyGPUTextureToTexture(copy, &a, &b, static_cast<Uint32>(w), static_cast<Uint32>(h), 1, false);
    }
} // namespace

void Gpu::SetVram(const std::vector<uint16_t> &vram) {
    // (a transfer buffer of its own, let go afterwards: it is 64 MB, and the one for the frames' data stays small)
    const uint32_t bytes = static_cast<uint32_t>(vram.size() * 2);
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = bytes;
    SDL_GPUTransferBuffer *whole = SDL_CreateGPUTransferBuffer(m_device, &info);
    std::memcpy(SDL_MapGPUTransferBuffer(m_device, whole, false), vram.data(), bytes);
    SDL_UnmapGPUTransferBuffer(m_device, whole);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    UploadFrom(copy, whole, m_vram, 0, 0, kW, kH);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(m_device, whole);
    m_staleAll = true; // (all of VRAM new: the work texture behind in all of it)
    for (int j = 0; j < kJournals; j++) {
        m_kept[j].clear();
        m_journal[j] = false;
    }
}

void Gpu::ReadAreas(const std::vector<VramArea> &areas, uint16_t *pixels) {
    uint32_t bytes = 0;
    for (const VramArea &a : areas) {
        bytes += static_cast<uint32_t>(a.w) * a.h * 2;
    }
    if (!bytes) {
        return;
    }
    if (bytes > m_downloadSize) {
        if (m_download) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_download);
        }
        m_downloadSize = bytes;
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        info.size = bytes;
        m_download = SDL_CreateGPUTransferBuffer(m_device, &info);
    }
    // (all of them in one go: one wait for the GPU)
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    uint32_t at = 0;
    for (const VramArea &a : areas) {
        SDL_GPUTextureRegion region{};
        region.texture = m_vram;
        region.x = static_cast<Uint32>(a.x);
        region.y = static_cast<Uint32>(a.y);
        region.w = static_cast<Uint32>(a.w);
        region.h = static_cast<Uint32>(a.h);
        region.d = 1;
        SDL_GPUTextureTransferInfo to{};
        to.transfer_buffer = m_download;
        to.offset = at;
        to.pixels_per_row = static_cast<Uint32>(a.w);
        to.rows_per_layer = static_cast<Uint32>(a.h);
        SDL_DownloadFromGPUTexture(copy, &region, &to);
        at += static_cast<uint32_t>(a.w) * a.h * 2;
    }
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(m_device, true, &fence, 1);
    SDL_ReleaseGPUFence(m_device, fence);
    std::memcpy(pixels, SDL_MapGPUTransferBuffer(m_device, m_download, false), bytes);
    SDL_UnmapGPUTransferBuffer(m_device, m_download);
}

void Gpu::WriteAreas(const std::vector<VramArea> &areas, const uint16_t *pixels) {
    uint32_t bytes = 0;
    for (const VramArea &a : areas) {
        bytes += static_cast<uint32_t>(a.w) * a.h * 2;
    }
    if (!bytes) {
        return;
    }
    std::memcpy(MapUpload(bytes), pixels, bytes);
    SDL_UnmapGPUTransferBuffer(m_device, m_upload);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    uint32_t at = 0;
    for (const VramArea &a : areas) {
        UploadRows(copy, m_vram, a.x, a.y, a.w, a.h, at);
        at += static_cast<uint32_t>(a.w) * a.h * 2;
        Stale({a.x, a.y, a.x + a.w - 1, a.y + a.h - 1});
    }
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
}

void Gpu::GetVram(std::vector<uint16_t> &vram) {
    vram.resize(static_cast<size_t>(kW) * kH);
    Download(m_vram, 0, 0, kW, kH, vram.data(), static_cast<uint32_t>(vram.size() * 2));
    // (64 MB that would otherwise stay: with rewind on, beside the timeline's own copy of VRAM, uncounted)
    ShrinkDownload(0);
}

void Gpu::ReadScreen(uint32_t scrollX, uint32_t scrollY, uint32_t *out) {
    // (the window's rows where they lie in VRAM; one that runs off an edge wraps, a column at a time)
    const int w = Blitter::kScreenWidth, h = Blitter::kScreenHeight;
    const int x = static_cast<int>(scrollX & (kW - 1)), y = static_cast<int>(scrollY & (kH - 1));
    std::vector<uint16_t> pixels(static_cast<size_t>(w) * h, 0);
    const int gotW = std::min(w, kW - x), gotH = std::min(h, kH - y);
    std::vector<uint16_t> part(static_cast<size_t>(gotW) * gotH);
    Download(m_vram, x, y, gotW, gotH, part.data(), static_cast<uint32_t>(part.size() * 2));
    for (int row = 0; row < gotH; row++) {
        std::memcpy(&pixels[static_cast<size_t>(row) * w], &part[static_cast<size_t>(row) * gotW], static_cast<size_t>(gotW) * 2);
    }
    for (size_t i = 0; i < pixels.size(); i++) {
        const uint16_t p = pixels[i];
        const uint32_t r = (p >> 10) & 0x1F, g = (p >> 5) & 0x1F, b = p & 0x1F;
        out[i] = 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (r << 3 | r >> 2);
    }
}

void Gpu::StartScreenRead(int which, uint32_t scrollX, uint32_t scrollY) {
    // (as ReadScreen reads it)
    const int x = static_cast<int>(scrollX & (kW - 1)), y = static_cast<int>(scrollY & (kH - 1));
    m_readW[which] = std::min(static_cast<int>(Blitter::kScreenWidth), kW - x);
    m_readH[which] = std::min(static_cast<int>(Blitter::kScreenHeight), kH - y);
    StartRead(which, m_vram, x, y, m_readW[which], m_readH[which], static_cast<uint32_t>(m_readW[which] * m_readH[which] * 2));
}

bool Gpu::EndScreenRead(int which, uint32_t *out) {
    const int w = Blitter::kScreenWidth, h = Blitter::kScreenHeight, gotW = m_readW[which], gotH = m_readH[which];
    m_readPart.resize(static_cast<size_t>(gotW) * gotH);
    if (!EndRead(which, m_readPart.data(), static_cast<uint32_t>(m_readPart.size() * 2))) {
        return false;
    }
    std::vector<uint16_t> pixels(static_cast<size_t>(w) * h, 0);
    for (int row = 0; row < gotH; row++) {
        std::memcpy(&pixels[static_cast<size_t>(row) * w], &m_readPart[static_cast<size_t>(row) * gotW], static_cast<size_t>(gotW) * 2);
    }
    for (size_t i = 0; i < pixels.size(); i++) {
        const uint16_t p = pixels[i];
        const uint32_t r = (p >> 10) & 0x1F, g = (p >> 5) & 0x1F, b = p & 0x1F;
        out[i] = 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (r << 3 | r >> 2);
    }
    return true;
}

// Run-ahead: an area about to be written is copied to the backup texture first (each journal's own). Only the part
// of it not kept already is copied: the first copy of a pixel holds what was there when the journal began, a second
// would not
void Gpu::Keep(SDL_GPUCommandBuffer *cmd, const Rect &area) {
    for (int j = 0; j < kJournals; j++) {
        if (!m_journal[j]) {
            continue;
        }
        // (what of the area is not inside an area already kept: split by the kept areas, simply)
        std::vector<Rect> left{area};
        for (const Rect &k : m_kept[j]) {
            std::vector<Rect> next;
            for (const Rect &r : left) {
                if (r.x1 < k.x0 || r.x0 > k.x1 || r.y1 < k.y0 || r.y0 > k.y1) {
                    next.push_back(r);
                    continue;
                }
                if (r.y0 < k.y0) {
                    next.push_back({r.x0, r.y0, r.x1, k.y0 - 1});
                }
                if (r.y1 > k.y1) {
                    next.push_back({r.x0, k.y1 + 1, r.x1, r.y1});
                }
                const int y0 = std::max(r.y0, k.y0), y1 = std::min(r.y1, k.y1);
                if (r.x0 < k.x0) {
                    next.push_back({r.x0, y0, k.x0 - 1, y1});
                }
                if (r.x1 > k.x1) {
                    next.push_back({k.x1 + 1, y0, r.x1, y1});
                }
            }
            left.swap(next);
        }
        if (left.empty()) {
            continue;
        }
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        for (const Rect &r : left) {
            CopyArea(copy, m_vram, j ? m_backup2 : m_backup, r.x0, r.y0, r.x1 - r.x0 + 1, r.y1 - r.y0 + 1);
            m_kept[j].push_back(r);
        }
        SDL_EndGPUCopyPass(copy);
    }
}

void Gpu::BeginJournal(int which) {
    SDL_GPUTexture *&backup = which ? m_backup2 : m_backup;
    if (!backup) {
        backup = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    }
    m_kept[which].clear();
    m_journal[which] = true;
}

void Gpu::EndJournal(int which) {
    m_kept[which].clear();
    m_journal[which] = false;
}

void Gpu::Rollback(int which) {
    if (!m_kept[which].empty()) {
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        for (const Rect &r : m_kept[which]) {
            CopyArea(copy, which ? m_backup2 : m_backup, m_vram, r.x0, r.y0, r.x1 - r.x0 + 1, r.y1 - r.y0 + 1);
            Stale(r);
        }
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(cmd);
    }
    EndJournal(which);
}

void Gpu::SetVramAside(int which) {
    const SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET; // (as m_vram's)
    if (!m_aside[which]) {
        m_aside[which] = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, usage);
        m_asideWork[which] = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, usage);
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    CopyArea(copy, m_vram, m_aside[which], 0, 0, kW, kH);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    m_asideStale[which].clear();
    m_asideStaleAll[which] = true; // (its work texture brought whole at its first pass)
}

void Gpu::TradeVram(int which) {
    std::swap(m_vram, m_aside[which]);
    std::swap(m_work, m_asideWork[which]);
    std::swap(m_stale, m_asideStale[which]);
    std::swap(m_staleAll, m_asideStaleAll[which]);
}

void Gpu::DropVramAside() {
    if (!m_device) {
        return;
    }
    for (int i = 0; i < kAsides; i++) {
        for (SDL_GPUTexture **t : {&m_aside[i], &m_asideWork[i]}) {
            if (*t) {
                SDL_ReleaseGPUTexture(m_device, *t);
                *t = nullptr;
            }
        }
    }
}

// The data textures' room for a batch of passes (SPELUNK_GPU_SMALL_BATCHES: a few passes' worth, so a frame takes
// many batches: the check of Flush's way between them)
namespace {
    struct Room {
        size_t draws, lists, tiles;
    };
    const Room &BatchRoom() {
        static const Room room = getenv("SPELUNK_GPU_SMALL_BATCHES")
                                     ? Room{64, 4096, 512}
                                     : Room{static_cast<size_t>(kDataSize) * kDataSize / 2, static_cast<size_t>(kDataSize) * kDataSize,
                                            static_cast<size_t>(kDataSize) * kTileRows};
        return room;
    }
} // namespace

void Gpu::Stale(const Rect &area) {
    if (m_staleAll) {
        return;
    }
    // (a few areas, else the one around them all)
    if (m_stale.size() >= 8) {
        Rect all = area;
        for (const Rect &r : m_stale) {
            all = {std::min(all.x0, r.x0), std::min(all.y0, r.y0), std::max(all.x1, r.x1), std::max(all.y1, r.y1)};
        }
        m_stale.assign(1, all);
        return;
    }
    m_stale.push_back(area);
}

// A run of draws none of which reads what another of them writes: binned into tiles over the area they cover, the
// pass that draws them planned (Flush records it)
void Gpu::PlanDraws(SDL_GPUCommandBuffer *cmd, const NativeDraw *draws, uint32_t count) {
    Rect area{kW, kH, -1, -1};
    for (uint32_t i = 0; i < count; i++) {
        area.x0 = std::min(area.x0, draws[i].x0);
        area.y0 = std::min(area.y0, draws[i].y0);
        area.x1 = std::max(area.x1, draws[i].x1);
        area.y1 = std::max(area.y1, draws[i].y1);
    }
    area.x0 = std::max(area.x0, 0) & ~15; // (tiles start on the area's corner: whole tiles keep the sums simple)
    area.y0 = std::max(area.y0, 0) & ~15;
    area.x1 = std::min(area.x1, kW - 1);
    area.y1 = std::min(area.y1, kH - 1);
    if (area.x1 < area.x0 || area.y1 < area.y0) {
        return;
    }
    const int tilesW = (area.x1 - area.x0) / 16 + 1, tilesH = (area.y1 - area.y0) / 16 + 1;
    const size_t tiles = static_cast<size_t>(tilesW) * tilesH;
    // Each tile's draws, in order: counted, then placed
    m_counts.assign(tiles, 0);
    auto each = [&](const NativeDraw &d, auto &&fn) {
        const int tx0 = (std::max(d.x0, area.x0) - area.x0) / 16, tx1 = (std::min(d.x1, area.x1) - area.x0) / 16;
        const int ty0 = (std::max(d.y0, area.y0) - area.y0) / 16, ty1 = (std::min(d.y1, area.y1) - area.y0) / 16;
        for (int ty = ty0; ty <= ty1; ty++) {
            for (int tx = tx0; tx <= tx1; tx++) {
                fn(static_cast<size_t>(ty) * tilesW + tx);
            }
        }
    };
    for (uint32_t i = 0; i < count; i++) {
        each(draws[i], [&](size_t tile) { m_counts[tile]++; });
    }
    size_t total = 0;
    for (uint32_t c : m_counts) {
        longestTile = std::max(longestTile, c);
        total += c;
    }
    const Room &room = BatchRoom();
    const Room most{static_cast<size_t>(kDataSize) * kDataSize / 2, static_cast<size_t>(kDataSize) * kDataSize,
                    static_cast<size_t>(kDataSize) * kTileRows};
    if (count > most.draws || total > most.lists || tiles > most.tiles) {
        fprintf(stderr, "gpu: a pass of %u draws is more than the renderer holds\n", count);
        return;
    }
    // (no room left in this batch: the passes so far recorded, and a batch begun anew)
    if (m_planDraws.size() + count > room.draws || m_planLists.size() + total > room.lists || m_planTiles.size() / 2 + tiles > room.tiles) {
        Flush(cmd);
    }
    const size_t drawAt = m_planDraws.size(), listAt = m_planLists.size(), tileAt = m_planTiles.size() / 2;
    m_planDraws.insert(m_planDraws.end(), draws, draws + count);
    m_planTiles.resize((tileAt + tiles) * 2);
    size_t run = listAt;
    for (size_t t = 0; t < tiles; t++) {
        m_planTiles[(tileAt + t) * 2] = static_cast<int32_t>(run);
        m_planTiles[(tileAt + t) * 2 + 1] = static_cast<int32_t>(m_counts[t]);
        run += m_counts[t];
        m_counts[t] = 0;
    }
    m_planLists.resize(listAt + total);
    for (uint32_t i = 0; i < count; i++) {
        each(draws[i], [&](size_t tile) {
            m_planLists[static_cast<size_t>(m_planTiles[(tileAt + tile) * 2]) + m_counts[tile]++] = static_cast<int32_t>(drawAt + i);
        });
    }
    m_steps.push_back({false, area, tilesW, static_cast<uint32_t>(tileAt), 0});
    passes++;
    drawsRun += count;
}

// The passes planned so far: their data sent in one copy pass, then each run, each into the texture that is not VRAM,
// which then is
void Gpu::Flush(SDL_GPUCommandBuffer *cmd) {
    if (m_steps.empty()) {
        return;
    }
    // (whole rows of each data texture, but of a first and only row just what is used; each part 512 bytes on, as
    // MapUpload's)
    auto part = [](size_t texels, size_t texel, int &width, int &rows) {
        rows = static_cast<int>((texels + kDataSize - 1) / kDataSize);
        width = rows == 1 ? static_cast<int>(texels) : kDataSize;
        return (static_cast<uint32_t>(width) * rows * texel + 511) / 512 * 512;
    };
    int drawW = 0, drawRows = 0, listW = 0, listRows = 0, tileW = 0, tileRows = 0;
    const uint32_t drawBytes = part(m_planDraws.size() * 2, 16, drawW, drawRows);
    const uint32_t listBytes = part(m_planLists.size(), 4, listW, listRows);
    const uint32_t tileBytes = part(m_planTiles.size() / 2, 8, tileW, tileRows);
    const uint32_t pixelBytes = static_cast<uint32_t>(m_pixels.size() * 2);
    uint8_t *data = static_cast<uint8_t *>(MapUpload(drawBytes + listBytes + tileBytes + pixelBytes));
    std::memcpy(data, m_planDraws.data(), m_planDraws.size() * sizeof(NativeDraw));
    std::memcpy(data + drawBytes, m_planLists.data(), m_planLists.size() * 4);
    std::memcpy(data + drawBytes + listBytes, m_planTiles.data(), m_planTiles.size() * 4);
    std::memcpy(data + drawBytes + listBytes + tileBytes, m_pixels.data(), pixelBytes);
    SDL_UnmapGPUTransferBuffer(m_device, m_upload);
    const uint32_t pixelsAt = drawBytes + listBytes + tileBytes;
    if (drawRows || listRows || tileRows) {
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        if (drawRows) {
            UploadRows(copy, m_draws, 0, 0, drawW, drawRows, 0);
        }
        if (listRows) {
            UploadRows(copy, m_lists, 0, 0, listW, listRows, drawBytes);
        }
        if (tileRows) {
            UploadRows(copy, m_tiles, 0, 0, tileW, tileRows, drawBytes + listBytes);
        }
        SDL_EndGPUCopyPass(copy);
    }
    for (const Step &step : m_steps) {
        const Rect &a = step.area;
        const int w = a.x1 - a.x0 + 1, h = a.y1 - a.y0 + 1;
        if (step.upload) {
            Keep(cmd, a);
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            UploadRows(copy, m_vram, a.x0, a.y0, w, h, pixelsAt + step.pixelsAt * 2);
            SDL_EndGPUCopyPass(copy);
            Stale(a);
            continue;
        }
        Keep(cmd, a);
        if (m_staleAll) {
            // (the other texture behind VRAM in more than a few areas: all of it brought at once)
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            CopyArea(copy, m_vram, m_work, 0, 0, kW, kH);
            SDL_EndGPUCopyPass(copy);
            m_staleAll = false;
            m_stale.clear();
        }
        SDL_assert(m_work != m_vram);
        SDL_GPUColorTargetInfo target{};
        target.texture = m_work;
        target.load_op = SDL_GPU_LOADOP_LOAD; // (it becomes VRAM: all of it kept, not only the area)
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, m_listPipeline);
        // (VRAM read, the other written: never the texture drawn into, SDL_assert above)
        const SDL_GPUTextureSamplerBinding bindings[4] = {{m_vram, m_nearest}, {m_draws, m_nearest}, {m_tiles, m_nearest}, {m_lists, m_nearest}};
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 4);
        auto draw = [&](const Rect &r, int32_t tileAt, int32_t tilesW) {
            const int rw = r.x1 - r.x0 + 1, rh = r.y1 - r.y0 + 1;
            const SDL_GPUViewport viewport{static_cast<float>(r.x0), static_cast<float>(r.y0), static_cast<float>(rw), static_cast<float>(rh), 0, 1};
            SDL_SetGPUViewport(pass, &viewport);
            const SDL_Rect scissor{r.x0, r.y0, rw, rh};
            SDL_SetGPUScissor(pass, &scissor);
            const int32_t params[4] = {r.x0, r.y0, tileAt, tilesW};
            SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof params);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        };
        // (first what the other texture has not had yet, VRAM as it is; then the pass's draws over it)
        for (const Rect &r : m_stale) {
            draw(r, 0, 0);
        }
        draw(a, static_cast<int32_t>(step.tileAt), step.tilesW);
        SDL_EndGPURenderPass(pass);
        std::swap(m_vram, m_work);
        m_stale.assign(1, a); // (now the one behind: by this pass's area)
    }
    m_steps.clear();
    m_planDraws.clear();
    m_planLists.clear();
    m_planTiles.clear();
    m_pixels.clear();
}

void Gpu::Run(const NativeList &list) {
    passes = drawsRun = uploadsRun = longestTile = 0;
    if (list.steps.empty()) {
        return;
    }
    // (one command buffer for the frame, its passes planned and then recorded in one go or a few: Flush)
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    for (const NativeList::Step &step : list.steps) {
        if (step.upload) {
            const NativeUpload &u = list.uploads[step.first];
            if (u.width > 0 && u.height > 0) {
                m_steps.push_back({true, {u.x, u.y, u.x + u.width - 1, u.y + u.height - 1}, 0, 0, static_cast<uint32_t>(m_pixels.size())});
                m_pixels.insert(m_pixels.end(), &list.pixels[u.pixels], &list.pixels[u.pixels] + static_cast<size_t>(u.width) * u.height);
                uploadsRun++;
            }
        } else {
            // A pass ends where a draw reads from the area the pass has written so far (it must see those pixels)
            const NativeDraw *draws = &list.draws[step.first];
            uint32_t from = 0;
            Rect written{kW, kH, -1, -1};
            for (uint32_t i = 0; i < step.count; i++) {
                const NativeDraw &d = draws[i];
                const bool fx = d.flags & NativeDraw::kFlipX, fy = d.flags & NativeDraw::kFlipY;
                const int sa = fx ? d.sx - d.x1 : d.sx + d.x0, sb = fx ? d.sx - d.x0 : d.sx + d.x1;
                int ta = fy ? d.sy - d.y1 : d.sy + d.y0, tb = fy ? d.sy - d.y0 : d.sy + d.y1;
                if ((ta & ~0xFFF) != (tb & ~0xFFF)) {
                    ta = 0; // (the source wraps round VRAM's height: any row)
                    tb = kH - 1;
                } else {
                    ta &= 0xFFF;
                    tb &= 0xFFF;
                }
                const bool reads = written.x1 >= 0 && !(sb < written.x0 || sa > written.x1 || tb < written.y0 || ta > written.y1);
                if (reads && i > from) {
                    PlanDraws(cmd, draws + from, i - from);
                    from = i;
                    written = {kW, kH, -1, -1};
                }
                written.x0 = std::min(written.x0, d.x0);
                written.y0 = std::min(written.y0, d.y0);
                written.x1 = std::max(written.x1, d.x1);
                written.y1 = std::max(written.y1, d.y1);
            }
            PlanDraws(cmd, draws + from, step.count - from);
        }
    }
    Flush(cmd);
    SubmitWork(cmd);
}

void Gpu::PresentScreen(uint32_t scrollX, uint32_t scrollY, int turn, const SDL_FRect &to) {
    const int32_t params[4] = {static_cast<int32_t>(scrollX & (kW - 1)), static_cast<int32_t>(scrollY & (kH - 1)), turn, 0};
    Present(m_screenPipeline, m_vram, params, to, true);
}

void Gpu::ScreenToPixels(uint32_t scrollX, uint32_t scrollY, int turn, int width, int height, std::vector<uint32_t> &out) {
    const int32_t params[4] = {static_cast<int32_t>(scrollX & (kW - 1)), static_cast<int32_t>(scrollY & (kH - 1)), turn, 0};
    PresentToPixels(m_screenPipeline, m_vram, params, width, height, out);
}

} // namespace spelunk

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
//
// Every frame of the screen as a small grey picture, one after another in one file (measurement: lining the port's
// frames up against a video of a board, tools/boardvideo). The headless runners write it when SPELUNK_FRAME_DUMP is
// set to FILE[:DIV]: the picture as the player sees it (stood up), each DIV x DIV block of pixels averaged (2 by
// default) to one byte, 0.299 R + 0.587 G + 0.114 B. The file starts "SPLKFRM1", then the width and height as two
// little-endian 32-bit numbers; then the frames, width x height bytes each, the first being the frame after the
// first the board runs.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

class FrameDump {
public:
    // From SPELUNK_FRAME_DUMP (nothing written if it is not set)
    FrameDump() {
        const char *v = getenv("SPELUNK_FRAME_DUMP");
        if (!v || !*v) {
            return;
        }
        std::string path = v;
        if (const size_t colon = path.rfind(':'); colon != std::string::npos && colon + 1 < path.size() &&
                                                  path.find_first_not_of("0123456789", colon + 1) == std::string::npos) {
            div_ = std::max(1, atoi(path.c_str() + colon + 1));
            path.resize(colon);
        }
        out_ = fopen(path.c_str(), "wb");
    }
    ~FrameDump() {
        if (out_) {
            fclose(out_);
        }
    }
    bool on() const { return out_ != nullptr; }
    // pixels: XBGR8888 (bytes R, G, B, X), w x h, already stood up
    void Write(const uint32_t *pixels, int w, int h) {
        if (!out_) {
            return;
        }
        const int tw = w / div_, th = h / div_;
        if (!wroteHeader_) {
            const uint32_t size[2] = {static_cast<uint32_t>(tw), static_cast<uint32_t>(th)};
            fwrite("SPLKFRM1", 1, 8, out_);
            fwrite(size, 4, 2, out_);
            wroteHeader_ = true;
        }
        row_.assign(static_cast<size_t>(tw) * th, 0);
        for (int ty = 0; ty < th; ty++) {
            for (int tx = 0; tx < tw; tx++) {
                uint32_t sum = 0;
                for (int y = ty * div_; y < (ty + 1) * div_; y++) {
                    for (int x = tx * div_; x < (tx + 1) * div_; x++) {
                        const uint32_t p = pixels[static_cast<size_t>(y) * w + x];
                        sum += (299 * (p & 0xFF) + 587 * ((p >> 8) & 0xFF) + 114 * ((p >> 16) & 0xFF)) / 1000;
                    }
                }
                row_[static_cast<size_t>(ty) * tw + tx] = static_cast<uint8_t>(sum / (div_ * div_));
            }
        }
        fwrite(row_.data(), 1, row_.size(), out_);
    }

private:
    FILE *out_ = nullptr;
    int div_ = 2;
    bool wroteHeader_ = false;
    std::vector<uint8_t> row_;
};

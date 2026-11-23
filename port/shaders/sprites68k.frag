#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A sprite's pixel on this screen pixel, as the sprite chip writes its buffer (machine/cave_video.cpp DrawSprite: the
// same steps): the sprite's pixels in 256ths of a screen pixel apart (its zoom), each written over what is there
// unless it is 0, the last written staying. The buffer holds the pixel (bits 0-7), its colour and its priority.
layout(set = 2, binding = 0) uniform usampler2D pixels; // the sprites' pixels, a byte each, 4096 a row
layout(set = 3, binding = 0) uniform Params {
    ivec4 mask; // x: the pixels' count less one (a power of two)
} u;
layout(location = 0) flat in ivec4 v_place; // x y (256ths of a pixel) width height
layout(location = 1) flat in ivec4 v_look;  // code attributes zoomX zoomY
layout(location = 0) out uint o_pixel;

// Which of a sprite's `size` pixels from `start` (in 256ths) land on screen pixel `at`: [first, last]
ivec2 Hits(int start, int zoom, int size, int at) {
    if (zoom <= 0) {
        return start >= 0 && (start >> 8) == at ? ivec2(0, size - 1) : ivec2(1, 0);
    }
    const int lo = at * 256 - start, hi = lo + 255;
    if (hi < 0) {
        return ivec2(1, 0);
    }
    return ivec2(lo <= 0 ? 0 : (lo + zoom - 1) / zoom, min(hi / zoom, size - 1));
}

void main() {
    const ivec2 at = ivec2(gl_FragCoord.xy);
    const int width = v_place.z, height = v_place.w;
    const ivec2 xs = Hits(v_place.x, v_look.z, width, at.x), ys = Hits(v_place.y, v_look.w, height, at.y);
    const bool flipX = (v_look.y & 0x10000) != 0, flipY = (v_look.y & 0x20000) != 0;
    // (the chip writes the source row by row from the first, flipped or not: of the pixels that land here, the one
    // written last is the last row's, then the last column's; with a flip those are the first drawn)
    for (int i = 0; i <= ys.y - ys.x; i++) {
        const int ky = flipY ? ys.x + i : ys.y - i;
        const int row = v_look.x + (flipY ? height - 1 - ky : ky) * width;
        for (int j = 0; j <= xs.y - xs.x; j++) {
            const int kx = flipX ? xs.x + j : xs.y - j;
            const int i2 = (row + (flipX ? width - 1 - kx : kx)) & u.mask.x;
            const uint pixel = texelFetch(pixels, ivec2(i2 & 4095, i2 >> 12), 0).r;
            if (pixel != 0u) {
                o_pixel = pixel | uint(v_look.y & 0xFF00);
                return;
            }
        }
    }
    discard;
}

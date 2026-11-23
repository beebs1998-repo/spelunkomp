#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A sprite's pixel on this screen pixel, as the IGS023 draws it (machine/igs023.cpp DrawSprite). The ROMs hold a mask
// bit for every pixel (B: 16 to a word, a row of the sprite after another) and the pixels themselves only where the
// mask is clear (A: three of 5 bits to a word, in the same order). So the pixel of row r, column c is the A data's
// k-th, k the clear bits of the mask before it: from a count made once of the clear bits before every mask word
// (`counts`), one subtraction and the bits of its own word. Zoom repeats or leaves out whole rows and columns by the
// bits of 32-bit tables; flips take the rows and columns from the far end. The buffer holds the colour's index (bits
// 0-9), the priority (bit 10: behind the background) and bit 15 (a sprite is here).
layout(set = 2, binding = 0) uniform isampler2D sprites; // the sprites (port/gpu_pgm.cpp SpriteTexels)
layout(set = 2, binding = 1) uniform usampler2D masks;   // the B ROM's words, 4096 a row
layout(set = 2, binding = 2) uniform usampler2D counts;  // the clear bits in the B ROM before each of its words
layout(set = 2, binding = 3) uniform usampler2D pixels;  // the A ROM's words, 4096 a row
layout(set = 3, binding = 0) uniform Params {
    ivec4 rom; // x: the A words less one, y: the B words less one, z: the B words' count as a power of two, w: all
               // the clear bits in the B ROM
} u;
layout(location = 0) flat in int v_sprite;
layout(location = 0) out uint o_pixel;

uint Fetch(usampler2D t, uint i) {
    return texelFetch(t, ivec2(int(i & 4095u), int(i >> 12)), 0).r;
}
// The clear bits in the B words before word n (n past the end: the ROM again from its start, as its address wraps)
uint Before(uint n) {
    return (n >> uint(u.rom.z)) * uint(u.rom.w) + Fetch(counts, n & uint(u.rom.y));
}
// The row (column) of the sprite the n-th line drawn shows: a table bit set doubles it (grow) or leaves it out
int Source(int n, uint zoom, bool grow) {
    if (zoom == 0u) {
        return n;
    }
    int period = 0;
    for (int i = 0; i < 32; i++) {
        period += ((zoom >> uint(i)) & 1u) != 0u ? (grow ? 2 : 0) : 1;
    }
    int rest = n % period, s = (n / period) * 32;
    for (int i = 0; i < 32; i++) {
        const int w = ((zoom >> uint(i)) & 1u) != 0u ? (grow ? 2 : 0) : 1;
        if (rest < w) {
            return s + i;
        }
        rest -= w;
    }
    return s + 31;
}

void main() {
    const ivec2 at = ivec2(gl_FragCoord.xy);
    const ivec4 place = texelFetch(sprites, ivec2(v_sprite * 4, 0), 0);     // x y last-x last-y
    const ivec4 data = texelFetch(sprites, ivec2(v_sprite * 4 + 1, 0), 0);  // first mask word, first pixel word, look, width
    const ivec4 zoom = texelFetch(sprites, ivec2(v_sprite * 4 + 2, 0), 0);  // across, down
    const int look = data.z;
    const bool flipX = (look & 0x100) != 0, flipY = (look & 0x200) != 0, growX = (look & 0x800) != 0, growY = (look & 0x1000) != 0;
    const int ox = at.x - place.x, oy = at.y - place.y;
    const int c = Source(flipX ? place.z - ox : ox, uint(zoom.x), growX);
    const int r = Source(flipY ? place.w - oy : oy, uint(zoom.y), growY);
    const uint first = uint(data.x);
    const uint word = first + uint(r * data.w + (c >> 4));
    const uint mask = Fetch(masks, word & uint(u.rom.y));
    const uint bit = uint(c & 15);
    if (((mask >> bit) & 1u) != 0u) {
        discard;
    }
    const uint k = Before(word) - Before(first) + uint(bitCount(~mask & ((1u << bit) - 1u) & 0xFFFFu));
    const uint pixel = (Fetch(pixels, (uint(data.y) + k / 3u) & uint(u.rom.x)) >> ((k % 3u) * 5u)) & 31u;
    o_pixel = pixel + uint(look & 31) * 32u + ((look & 0x400) != 0 ? 0x400u : 0u) + 0x8000u;
}

#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The picture of IGS's PGM board (port/gpu_pgm.cpp): for each screen pixel the background's (its scroll, each line's
// own scroll across), over it the sprite buffer's pixel unless that sprite is behind the background and the
// background has a pixel here, over both the text; the colour from the palette. machine/igs023.cpp Draw is the same
// steps in C++, and the check compares the two.
layout(set = 2, binding = 0) uniform usampler2D sprites; // the sprite buffer (448 x 224)
layout(set = 2, binding = 1) uniform usampler2D vram;    // 256 words a row: background, text, line scroll, palette
layout(set = 2, binding = 2) uniform usampler2D tiles;   // the tile ROM's bytes, 4096 a row
layout(set = 3, binding = 0) uniform Params {
    ivec4 scroll; // the background's across and down, the text's across and down
    ivec4 misc;   // the control register, the background's tiles, the text's tiles
} u;
layout(location = 0) out vec4 o_colour;

const int kBg = 0, kText = 8 * 256, kLines = 24 * 256, kPalette = 32 * 256;

int Word(int at) {
    return int(texelFetch(vram, ivec2(at & 255, at >> 8), 0).r);
}
int Byte(int at) {
    return int(texelFetch(tiles, ivec2(at & 4095, at >> 12), 0).r);
}
vec4 Colour(int index) {
    const int c = Word(kPalette + index);
    const ivec3 rgb = ivec3((c >> 10) & 31, (c >> 5) & 31, c & 31);
    return vec4(vec3((rgb << 3) | (rgb >> 2)) / 255.0, 1.0);
}

void main() {
    const ivec2 at = ivec2(gl_FragCoord.xy);
    const int ctrl = u.misc.x;
    int index = 0x3FF;
    bool background = false;
    if ((ctrl & 0x1000) == 0) {
        const int ty = (at.y + u.scroll.y) & 511;
        const int tx = (at.x + u.scroll.x + Word(kLines + at.y)) & 2047;
        const int cell = ((ty >> 5) * 64 + (tx >> 5)) * 2;
        const int code = Word(kBg + cell) % u.misc.y, attr = Word(kBg + cell + 1);
        int px = tx & 31, py = ty & 31;
        if ((attr & 0x40) != 0) {
            px = 31 - px;
        }
        if ((attr & 0x80) != 0) {
            py = 31 - py;
        }
        const int bit = code * 5120 + py * 160 + px * 5;
        const int pixel = ((Byte(bit >> 3) | (Byte((bit >> 3) + 1) << 8)) >> (bit & 7)) & 31;
        if (pixel != 31) {
            index = 0x400 + ((attr & 0x3E) >> 1) * 32 + pixel;
            background = true;
        }
    }
    const int sprite = int(texelFetch(sprites, at, 0).r);
    if ((sprite & 0x8000) != 0 && ((sprite & 0x400) == 0 || !background)) {
        index = sprite & 0x3FF;
    }
    if ((ctrl & 0x800) == 0) {
        const int ty = (at.y + u.scroll.w) & 255, tx = (at.x + u.scroll.z) & 511;
        const int cell = ((ty >> 3) * 64 + (tx >> 3)) * 2;
        const int code = Word(kText + cell) % u.misc.z, attr = Word(kText + cell + 1);
        int px = tx & 7, py = ty & 7;
        if ((attr & 0x40) != 0) {
            px = 7 - px;
        }
        if ((attr & 0x80) != 0) {
            py = 7 - py;
        }
        const int pixel = (Byte(code * 32 + py * 4 + (px >> 1)) >> ((px & 1) * 4)) & 15;
        if (pixel != 15) {
            index = 0x800 + ((attr & 0x3E) >> 1) * 16 + pixel;
        }
    }
    o_colour = Colour(index);
}

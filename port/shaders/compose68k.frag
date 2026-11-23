#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The picture of Cave's 68000 boards (port/gpu68k.cpp): for each screen pixel the tile layers' pixels (each layer's
// scroll, its scroll and row for each line, flips), the frontmost by the tile's priority, then its layer's, then the
// layer's number; the sprite buffer's pixel over it unless the tile's priority is above the sprite's; the colour
// from the palette. machine/cave_video.cpp Draw is the same steps in C++, and the check compares the two.
layout(set = 2, binding = 0) uniform usampler2D sprites; // the sprite buffer (the screen's size: 320 x 240, or 384 across)
layout(set = 2, binding = 1) uniform usampler2D layers;  // each layer's memory as words, 128 a row, 128 rows a layer
layout(set = 2, binding = 2) uniform usampler2D tiles;   // the layers' tiles, a byte a pixel, 64 a tile, 4096 a row
layout(set = 2, binding = 3) uniform usampler2D palette; // 32,768 colours, 256 a row: 5 bits each of green, red, blue
layout(set = 3, binding = 0) uniform Params {
    ivec4 misc;    // the layers, the background's pen, the sprites' colour step, the chips' pixel the screen starts at
    ivec4 on[4];   // shown, 16 x 16 tiles, flipped across, flipped down
    ivec4 lines[4]; // a scroll for each line, a row for each line, the scroll across, the scroll down
    ivec4 look[4]; // priority, the colours' base, the step from one colour to the next, the tiles' count
    ivec4 where[4]; // the first of the layer's tile pixels in `tiles`, which of a tile's colour bits count, 1: its 8 x 8 tiles below 0x10000 are those 0x40000 on
} u;
layout(location = 0) out vec4 o_colour;

uint Word(int layer, int word) {
    return texelFetch(layers, ivec2(word & 127, layer * 128 + ((word >> 7) & 127)), 0).r;
}
vec4 Colour(int pen) {
    const int c = int(texelFetch(palette, ivec2(pen & 255, pen >> 8), 0).r);
    const ivec3 rgb = ivec3((c >> 5) & 31, (c >> 10) & 31, c & 31);
    return vec4(vec3((rgb << 3) | (rgb >> 2)) / 255.0, 1.0);
}

void main() {
    const ivec2 at = ivec2(gl_FragCoord.xy);
    const int width = textureSize(sprites, 0).x;
    int best = -1;
    int pen = u.misc.y & 0x7FFF;
    for (int n = 0; n < u.misc.x; n++) {
        if (u.on[n].x == 0) {
            continue;
        }
        const bool big = u.on[n].y != 0, flipX = u.on[n].z != 0, flipY = u.on[n].w != 0;
        const int scrollX = u.lines[n].z, scrollY = u.lines[n].w;
        // (a layer turned over is the same layer seen from the screen's other end)
        const int line = flipY ? 239 - at.y : at.y;
        const int row = (scrollY + (flipY ? 1 : -1) + line) & 0x1FF;
        const int mapY = u.lines[n].y != 0 ? int(Word(n, 0x800 + row * 2 + 1)) & 0x1FF : (line + scrollY) & 0x1FF;
        const int lineX = scrollX + (u.lines[n].x != 0 ? int(Word(n, 0x800 + row * 2)) : 0);
        const int px = ((flipX ? width - 1 - at.x : at.x + u.misc.w) + lineX) & 0x1FF, py = mapY;
        uint tile, code;
        if (big) {
            const int w = ((py >> 4) * 32 + (px >> 4)) * 2;
            tile = (Word(n, w) << 16) | Word(n, w + 1);
            code = ((tile & 0xFFFFu) * 4u) ^ uint((px >> 3) & 1) ^ uint(((py >> 3) & 1) * 2);
        } else {
            const int w = 0x2000 + ((py >> 3) * 64 + (px >> 3)) * 2;
            tile = (Word(n, w) << 16) | Word(n, w + 1);
            code = tile & 0x3FFFFu;
            if (u.where[n].z != 0 && code < 0x10000u) {
                code += 0x40000u;
            }
        }
        const int i = u.where[n].x + int(code % uint(u.look[n].w)) * 64 + (py & 7) * 8 + (px & 7);
        const uint pixel = texelFetch(tiles, ivec2(i & 4095, i >> 12), 0).r;
        if (pixel == 0u) {
            continue;
        }
        const int rank = int(tile >> 30) << 4 | u.look[n].x << 2 | n;
        if (rank > best) {
            best = rank;
            pen = (u.look[n].y + int((tile >> 24) & uint(u.where[n].y)) * u.look[n].z + int(pixel)) & 0x7FFF;
        }
    }
    const int sprite = int(texelFetch(sprites, at, 0).r);
    if (sprite != 0) {
        const int spritePriority = sprite >> 14, tilePriority = best < 0 ? 0 : (best >> 4) + 1;
        if (tilePriority <= spritePriority) {
            pen = (((sprite >> 8) & 0x3F) * u.misc.z + (sprite & 0xFF)) & 0x7FFF;
        }
    }
    o_colour = Colour(pen);
}

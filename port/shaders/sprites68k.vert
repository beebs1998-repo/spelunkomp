#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The sprites of Cave's 68000 boards (port/gpu68k.cpp), one quad each over the screen pixels it can reach, in the
// list's order: where two overlap the later one is drawn last, as the sprite chip drew it into its buffer.
layout(set = 0, binding = 0) uniform isampler2D sprites; // each sprite in two texels: x y (256ths) width height | code attributes zoomX zoomY
layout(set = 1, binding = 0) uniform Params {
    ivec4 screen; // x: the screen's width (320; Mazinger Z's board 384)
} u;
layout(location = 0) flat out ivec4 v_place;
layout(location = 1) flat out ivec4 v_look;

// The last screen pixel a sprite of `size` pixels from `start` (256ths of a pixel) reaches with its zoom
int Reach(int start, int size, int zoom) {
    return (start + (size - 1) * max(zoom, 0)) >> 8;
}

void main() {
    const int n = gl_VertexIndex / 6, corner = gl_VertexIndex % 6;
    const ivec4 place = texelFetch(sprites, ivec2((n * 2) & 2047, (n * 2) >> 11), 0);
    const ivec4 look = texelFetch(sprites, ivec2((n * 2 + 1) & 2047, (n * 2 + 1) >> 11), 0);
    v_place = place;
    v_look = look;
    const int x0 = max(place.x >> 8, 0), y0 = max(place.y >> 8, 0);
    const int x1 = min(Reach(place.x, place.z, look.z), u.screen.x - 1) + 1, y1 = min(Reach(place.y, place.w, look.w), 239) + 1;
    // (the corners of two triangles; none at all where the sprite misses the screen)
    const bool right = corner == 1 || corner == 3 || corner == 4, down = corner == 2 || corner == 4 || corner == 5;
    vec2 p = vec2(right ? x1 : x0, down ? y1 : y0);
    if (x1 <= x0 || y1 <= y0) {
        p = vec2(0.0);
    }
    gl_Position = vec4(p.x / (float(u.screen.x) * 0.5) - 1.0, 1.0 - p.y / 120.0, 0.0, 1.0);
}

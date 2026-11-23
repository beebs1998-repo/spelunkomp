#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The sprites of IGS's PGM board (port/gpu_pgm.cpp), one quad each over the screen pixels it covers, in the list's
// order: where two overlap the later one is drawn last. (The chip draws the list from its end and keeps the first
// pixel it draws: the same pixel wins.)
layout(set = 0, binding = 0) uniform isampler2D sprites; // each sprite in four texels (port/gpu_pgm.cpp SpriteTexels)
layout(location = 0) flat out int v_sprite;

void main() {
    const int n = gl_VertexIndex / 6, corner = gl_VertexIndex % 6;
    const ivec4 place = texelFetch(sprites, ivec2(n * 4, 0), 0); // x y last-x last-y (from x, y)
    v_sprite = n;
    const int x0 = max(place.x, 0), y0 = max(place.y, 0);
    const int x1 = min(place.x + place.z, 447) + 1, y1 = min(place.y + place.w, 223) + 1;
    // (the corners of two triangles; none at all where the sprite misses the screen)
    const bool right = corner == 1 || corner == 3 || corner == 4, down = corner == 2 || corner == 4 || corner == 5;
    vec2 p = vec2(right ? x1 : x0, down ? y1 : y0);
    if (x1 <= x0 || y1 <= y0 || place.z < 0 || place.w < 0) {
        p = vec2(0.0);
    }
    gl_Position = vec4(p.x / 224.0 - 1.0, 1.0 - p.y / 112.0, 0.0, 1.0);
}

#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The screen: the 320 x 240 window of VRAM at the scroll position, turned to stand as the player wants it and
// stretched over the viewport (the port sets that to a whole multiple of the picture, or to fill the window)
layout(set = 2, binding = 0) uniform usampler2D vram;
layout(set = 3, binding = 0) uniform Params {
    ivec4 window; // scroll x, y; the turn in quarter turns clockwise
} u;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main() {
    const int turn = u.window.z;
    const ivec2 size = (turn & 1) != 0 ? ivec2(240, 320) : ivec2(320, 240);
    const ivec2 t = clamp(ivec2(v_uv * vec2(size)), ivec2(0), size - 1);
    ivec2 p = t;
    if (turn == 1) {
        p = ivec2(t.y, 239 - t.x);
    } else if (turn == 2) {
        p = ivec2(319 - t.x, 239 - t.y);
    } else if (turn == 3) {
        p = ivec2(319 - t.y, t.x);
    }
    const int pen = int(texelFetch(vram, (u.window.xy + p) & ivec2(0x1FFF, 0x0FFF), 0).r);
    const ivec3 c = ivec3((pen >> 10) & 31, (pen >> 5) & 31, pen & 31);
    o_colour = vec4(vec3((c << 3) | (c >> 2)) / 255.0, 1.0);
}

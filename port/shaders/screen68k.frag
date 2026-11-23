#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The screen of Cave's 68000 boards: the picture the GPU made (320 x 240; Mazinger Z's board 384 across), turned to stand as the player wants it and
// stretched over the viewport (the port sets that to a whole multiple of the picture, or to fill the window)
layout(set = 2, binding = 0) uniform sampler2D picture;
layout(set = 3, binding = 0) uniform Params {
    ivec4 turn; // x: the turn in quarter turns clockwise
} u;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main() {
    const int turn = u.turn.x;
    const ivec2 full = textureSize(picture, 0);
    const ivec2 size = (turn & 1) != 0 ? full.yx : full;
    const ivec2 t = clamp(ivec2(v_uv * vec2(size)), ivec2(0), size - 1);
    ivec2 p = t;
    if (turn == 1) {
        p = ivec2(t.y, full.y - 1 - t.x);
    } else if (turn == 2) {
        p = ivec2(full.x - 1 - t.x, full.y - 1 - t.y);
    } else if (turn == 3) {
        p = ivec2(full.x - 1 - t.y, t.x);
    }
    o_colour = vec4(texelFetch(picture, p, 0).rgb, 1.0);
}

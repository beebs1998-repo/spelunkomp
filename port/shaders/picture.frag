#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A picture made on the CPU (the in-game menu over the frozen game) over the viewport
layout(set = 2, binding = 0) uniform sampler2D picture;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main() {
    o_colour = vec4(texture(picture, v_uv).rgb, 1.0);
}

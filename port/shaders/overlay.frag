#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A picture made on the CPU laid over what is there (the training readouts over the game): its alpha decides
layout(set = 2, binding = 0) uniform sampler2D picture;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_colour;

void main() {
    o_colour = texture(picture, v_uv);
}

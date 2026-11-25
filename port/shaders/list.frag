#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The CV1000 blitter's draws, run on the GPU as the board runs them: for each pixel of VRAM the draws that cover it,
// in the game's order, each blended into what is there with the board's own 5-bit arithmetic (machine/blitter.cpp is
// the same sums in C++, and the check compares the two pixel for pixel).
//
// The draws are binned by the CPU into tiles of 16 x 16 pixels over the area they cover: `tiles` gives each tile's
// run in `lists`, which names its draws in order; `draws` holds each draw in two texels. A frame's passes are in the
// three textures one after another (port/gpu.cpp, Gpu::Run): a pass's tiles start at origin.z, origin.w to a row.
// The picture goes to the other of two VRAM textures, which then becomes VRAM; origin.w 0 is a pass of no draws, which
// brings the pixels of an area that the other texture does not have yet (VRAM as it is, copied).
layout(set = 2, binding = 0) uniform usampler2D vram;  // VRAM: bit 15 opaque, then red, green, blue (5 bits each)
layout(set = 2, binding = 1) uniform isampler2D draws; // x0 y0 x1 y1 | sx sy flags colours
layout(set = 2, binding = 2) uniform isampler2D tiles; // first, count
layout(set = 2, binding = 3) uniform isampler2D lists;
layout(set = 3, binding = 0) uniform Params {
    ivec4 origin; // the area's top left in VRAM, its first tile, its tiles across (0: VRAM as it is)
} u;
layout(location = 0) out uint o_pixel;

int mul(int x, int y) {
    return min((x * y) / 31, 31);
}
// (31 - x) * y / 31
int rev(int x, int y) {
    return mul(x ^ 31, y);
}
int sourceTerm(int mode, int s, int d, int alpha) {
    switch (mode) {
    case 0: return mul(alpha, s);
    case 1: return mul(s, s);
    case 2: return mul(d, s);
    case 4: return rev(alpha, s);
    case 5: return rev(s, s);
    case 6: return rev(d, s);
    default: return s;
    }
}
int destTerm(int mode, int s, int d, int alpha) {
    switch (mode) {
    case 0: return mul(d, alpha);
    case 1: return mul(s, d);
    case 2: return mul(d, d);
    case 4: return rev(alpha, d);
    case 5: return rev(s, d);
    case 6: return rev(d, d);
    default: return d;
    }
}

void main() {
    const ivec2 p = ivec2(gl_FragCoord.xy);
    int dst = int(texelFetch(vram, p, 0).r);
    if (u.origin.w == 0) {
        o_pixel = uint(dst);
        return;
    }
    const ivec2 tile = (p - u.origin.xy) >> 4;
    const int t = u.origin.z + tile.y * u.origin.w + tile.x;
    const ivec2 run = texelFetch(tiles, ivec2(t & 2047, t >> 11), 0).rg;
    for (int i = 0; i < run.y; i++) {
        const int at = run.x + i;
        const int draw = texelFetch(lists, ivec2(at & 2047, at >> 11), 0).r * 2;
        const ivec4 a = texelFetch(draws, ivec2(draw & 2047, draw >> 11), 0);
        if (p.x < a.x || p.y < a.y || p.x > a.z || p.y > a.w) {
            continue;
        }
        const ivec4 b = texelFetch(draws, ivec2((draw + 1) & 2047, draw >> 11), 0);
        const int flags = b.z;
        const int sx = (flags & 0x200) != 0 ? b.x - p.x : b.x + p.x;
        const int sy = ((flags & 0x400) != 0 ? b.y - p.y : b.y + p.y) & 0xFFF;
        const int pen = int(texelFetch(vram, ivec2(sx, sy), 0).r);
        if ((flags & 0x100) != 0 && (pen & 0x8000) == 0) {
            continue;
        }
        const int mode = flags & 0x7F;
        if (mode == 64 && (flags & 0x80) == 0) {
            dst = pen;
            continue;
        }
        int r = (pen >> 10) & 31, g = (pen >> 5) & 31, bl = pen & 31;
        if ((flags & 0x80) != 0) {
            r = mul(r, (b.w >> 10) & 63);
            g = mul(g, (b.w >> 16) & 63);
            bl = mul(bl, (b.w >> 22) & 63);
        }
        if (mode != 64) {
            const int sa = b.w & 31, da = (b.w >> 5) & 31, sm = mode & 7, dm = (mode >> 3) & 7;
            const int dr = (dst >> 10) & 31, dg = (dst >> 5) & 31, db = dst & 31;
            r = min(sourceTerm(sm, r, dr, sa) + destTerm(dm, r, dr, da), 31);
            g = min(sourceTerm(sm, g, dg, sa) + destTerm(dm, g, dg, da), 31);
            bl = min(sourceTerm(sm, bl, db, sa) + destTerm(dm, bl, db, da), 31);
        }
        dst = (pen & 0x8000) | (r << 10) | (g << 5) | bl;
    }
    o_pixel = uint(dst);
}

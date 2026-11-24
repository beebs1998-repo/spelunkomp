// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The Shader feature's choices by name, and its Your own, which the game (port/main.cpp) and the launcher
// (launcher/src/description.cpp: the player's presets under it, recomp-common/own_shaders.h) both name. A choice is
// kept in settings.ini by its name ([features] shader_id, recomp-common/settings.h), so the list's order is free; the
// number beside it (shader) is its place in the order the releases before the names had (0.1.0-beta.2, 0.2.0-beta.1:
// CRT Newpixie added at the end, after Your own), which those read.
#pragma once

#include <cstddef>

namespace spelunk {

// In the order of the menu's choices (runtime/features.cpp), and of kShaderPresets after Off (port/main.cpp)
constexpr const char *kShaderIds[] = {"off",          "crt",           "crt-lottes",       "crt-royale",    "crt-guest",
                                      "crt-light",    "crt-newpixie",  "scanlines-light",  "scanlines-strong",
                                      "scanlines-ossc", "scanlines-dark", "xbrz",          "scalefx",       "your-own"};
constexpr size_t kShaderCount = sizeof kShaderIds / sizeof kShaderIds[0];
// What shader = N meant before the names
constexpr const char *kShaderNumbered[] = {"off",          "crt",            "crt-lottes",       "crt-royale",
                                           "crt-guest",    "crt-light",      "scanlines-light",  "scanlines-strong",
                                           "scanlines-ossc", "scanlines-dark", "xbrz",           "scalefx",
                                           "your-own",     "crt-newpixie"};

constexpr int kYourOwnShader = static_cast<int>(kShaderCount) - 1;
static_assert(kShaderIds[kYourOwnShader][0] == 'y', "Your own is the last choice");

} // namespace spelunk

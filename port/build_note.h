// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
//
// A program built with none of its game's code translated (work/<id>/gen had none: CMake defines
// SPELUNK_INTERPRETED_BUILD) runs the game on the board's interpreters, far slower. It says so as it starts, in its
// log, and to the launcher, which shows the note on the game's row (--list-features: recomp-common/settings.h,
// FeatureListNote). CI's builds are these; tools/check_gen.sh keeps them out of the app and the releases.
#pragma once

#include "../recomp-common/settings.h"

#include <string>
#include <vector>

namespace port {

#ifdef SPELUNK_INTERPRETED_BUILD
inline constexpr bool kInterpretedBuild = true;
#else
inline constexpr bool kInterpretedBuild = false;
#endif
inline constexpr const char *kInterpretedLog = "interpreted build: no translated code";
inline constexpr const char *kInterpretedNote = "Interpreted build: no translated code, so it plays slower";

// What --list-features prints: the features, and the note of a build with no translated code
inline std::string FeatureListFor(const std::vector<recomp::FeatureInfo> &features) {
    std::string text = recomp::FeatureListText(features);
    if (kInterpretedBuild) {
        text += recomp::FeatureListNote(kInterpretedNote);
    }
    return text;
}

} // namespace port

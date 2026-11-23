// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; its shape is cps2recomp's.
//
// Which key and which gamepad button does what, for each player: the launcher's Controllers page writes the player's
// choices to settings.ini and the game reads them (the format and its reading are the recomp ports' own:
// recomp-common/launcher/src/bindings.h). Here are the board's controls as actions, and the usual layout.
//
// Usual layout. Keyboard, player 1: arrows; Z X C V the four buttons; Enter Start; 5 a coin. Player 2 has no keys
// until some are chosen. A gamepad: buttons 1-4 on South, East, West and North; Start; Back a coin. The left stick
// always works as the stick. The training tools: R (the left trigger) held rewinds, Backspace (the left shoulder)
// goes back to the practice point, \ (the right shoulder) sets it.
// (Kept to C++17: the launcher builds with it.)
#pragma once

#include <memory>

#include <map>

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../recomp-common/launcher/src/bindings.h"

namespace port {

// Each action's bits: the board's player bits (machine/cv1000.h's kIn) in the low 8, Start and the coin above them
inline constexpr uint32_t kStartBit = 1u << 8, kCoinBit = 1u << 9;
// The training tools' (they do something only with their features on: runtime/features.cpp)
inline constexpr uint32_t kRewindBit = 1u << 10, kPointBackBit = 1u << 11, kPointSetBit = 1u << 12, kFastBit = 1u << 13, kReplayBit = 1u << 14;
inline constexpr uint32_t kToolBits = kRewindBit | kPointBackBit | kPointSetBit | kFastBit | kReplayBit;
struct BoundAction {
    const char *name;
    const char *title;
    uint32_t bits;
};
inline constexpr BoundAction kBoundActions[] = {
    {"up", "Up", 0x01},        {"down", "Down", 0x02},     {"left", "Left", 0x04},     {"right", "Right", 0x08},
    {"b1", "Button 1", 0x10},  {"b2", "Button 2", 0x20},   {"b3", "Button 3", 0x40},   {"b4", "Button 4", 0x80},
    {"start", "Start", kStartBit}, {"coin", "Coin", kCoinBit},
    {"rewind", "Rewind (hold)", kRewindBit},
    {"point_back", "Back to the practice point", kPointBackBit},
    {"point_set", "Set the practice point", kPointSetBit},
    {"fast_forward", "Fast forward (hold)", kFastBit},
    {"point_replay", "Replay the last attempt", kReplayBit},
};
inline constexpr int kBoundActionCount = static_cast<int>(sizeof(kBoundActions) / sizeof(kBoundActions[0]));
inline constexpr int kFirstButton = 4, kButtons = 4;
struct BoundTable {
    recomp::BindAction actions[kBoundActionCount];
    uint32_t bits[kBoundActionCount];
    std::vector<std::string> titles;
    recomp::BindTable table;
};

// `buttons`: the game's own names for its buttons ("Shot|Bomb|..."), as game.cfg's `buttons` says
inline const BoundTable &MakeBoundTable(int players, const std::string &buttons) {
    // (one table per set of names, kept: the collection's launcher has a game of each)
    static std::map<std::string, std::unique_ptr<BoundTable>> made;
    std::unique_ptr<BoundTable> &slot = made[std::to_string(players) + ":" + buttons];
    if (slot) {
        return *slot;
    }
    slot = std::make_unique<BoundTable>();
    BoundTable &t = *slot;
    static constexpr SDL_Scancode kKeys[kBoundActionCount] = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_Z,
        SDL_SCANCODE_X,  SDL_SCANCODE_C,    SDL_SCANCODE_V,    SDL_SCANCODE_RETURN, SDL_SCANCODE_5,
        SDL_SCANCODE_R,  SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_TAB, SDL_SCANCODE_RIGHTBRACKET,
    };
    static const recomp::PadInput kPad[kBoundActionCount] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, -1},    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, -1}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, -1},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, -1}, {SDL_GAMEPAD_BUTTON_SOUTH, -1},     {SDL_GAMEPAD_BUTTON_EAST, -1},
        {SDL_GAMEPAD_BUTTON_WEST, -1},       {SDL_GAMEPAD_BUTTON_NORTH, -1},     {SDL_GAMEPAD_BUTTON_START, -1},
        {SDL_GAMEPAD_BUTTON_BACK, -1},
        {-1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, -1}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, -1},
        {-1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK, -1},
    };
    std::vector<std::string> names;
    for (size_t from = 0; !buttons.empty();) {
        const size_t bar = buttons.find('|', from);
        names.push_back(buttons.substr(from, bar == std::string::npos ? std::string::npos : bar - from));
        if (bar == std::string::npos) {
            break;
        }
        from = bar + 1;
    }
    t.titles.reserve(kBoundActionCount);
    for (int i = 0; i < kBoundActionCount; i++) {
        const int button = i - kFirstButton;
        t.titles.push_back(button >= 0 && button < kButtons && button < static_cast<int>(names.size()) ? names[button] : kBoundActions[i].title);
    }
    for (int i = 0; i < kBoundActionCount; i++) {
        recomp::BindAction &a = t.actions[i];
        a.name = kBoundActions[i].name;
        a.title = t.titles[i].c_str();
        for (int p = 0; p < recomp::kBindMaxPlayers; p++) {
            a.key[p] = p == 0 ? kKeys[i] : SDL_SCANCODE_UNKNOWN;
            a.pad[p] = kPad[i];
        }
        t.bits[i] = kBoundActions[i].bits;
    }
    t.table = {t.actions, kBoundActionCount, players};
    return t;
}

// A player's controls now: the bits of every bound key and gamepad input held, and the left stick as the stick
inline uint32_t BoundBits(const BoundTable &t, const recomp::Bindings &b, int player, const bool *keys, SDL_Gamepad *pad) {
    uint32_t bits = 0;
    for (int i = 0; i < t.table.count; i++) {
        const SDL_Scancode key = b.key[player][i];
        if (key != SDL_SCANCODE_UNKNOWN && keys[key]) {
            bits |= t.bits[i];
        }
        const recomp::PadInput in = b.pad[player][i];
        if (pad && ((in.button >= 0 && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(in.button))) ||
                    (in.axis >= 0 && SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(in.axis)) > 16000))) {
            bits |= t.bits[i];
        }
    }
    if (pad) {
        const int x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX), y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
        bits |= (x < -16000 ? 0x04u : 0) | (x > 16000 ? 0x08u : 0) | (y < -16000 ? 0x01u : 0) | (y > 16000 ? 0x02u : 0);
    }
    return bits;
}

} // namespace port

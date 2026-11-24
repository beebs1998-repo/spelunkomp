// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
// What the game's window has of online play (port/netplay.h; docs/ONLINE.md): the command line's --host, --join
// and --net-* options, the wait for the other player, and over the match the two players' names, the ping, the
// chat and Esc's "Leave the match?". port/main.cpp runs the match's frames through Netplay.
#pragma once

#include "hud.h"
#include "netplay.h"

#include "../recomp-common/menu_sdl.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace port {

// The game as online play asks for it (NetGame), from port/main.cpp's own parts
struct FnGame : NetGame {
    std::string id, title;
    uint32_t build = 0;
    double frameSeconds = 1 / 60.0;
    int mostAhead = Netplay::kMostAhead;
    std::function<void(std::vector<uint8_t> &)> keep;
    std::function<bool(const std::vector<uint8_t> &)> putBack;
    std::function<void()> coming;
    std::function<void(int)> keepAt, back, drop;
    std::function<void(const uint16_t *, bool)> step;
    std::function<uint32_t()> hash;
    std::vector<int> *features = nullptr;

    std::string GameId() const override { return id; }
    std::string Title() const override { return title; }
    uint32_t Build() const override { return build; }
    void KeepState(std::vector<uint8_t> &out) override { keep(out); }
    bool PutBackState(const std::vector<uint8_t> &in) override { return putBack(in); }
    void MatchComing() override { coming(); }
    int Feature(size_t index) const override { return index < features->size() ? (*features)[index] : 0; }
    void SetFeatureForNow(size_t index, int value) override {
        if (index < features->size()) {
            (*features)[index] = value;
        }
    }
    void Keep(int which) override { keepAt(which); }
    void BackTo(int which) override { back(which); }
    void Drop(int which) override { drop(which); }
    void Step(const uint16_t players[2], bool shown) override { step(players, shown); }
    uint32_t StateHash() override { return hash(); }
    double FrameSeconds() const override { return frameSeconds; }
    int MostAhead() const override { return mostAhead; }
};

struct OnlineOptions {
    bool wanted = false; // (--host or --join)
    Netplay::Settings net;
    std::string hashes;  // (--net-hashes FILE: each confirmed frame's hash, every second's: the sync tests)
    std::string say;     // (--net-say [FRAME:]TEXT: said in the chat at that frame of the match, for tests)
    uint64_t sayAt = 120;
    std::string log;     // (--net-log FILE: every confirmed frame's controls and hash, for finding where two games part)

    // An option of online play's at argv[i] (and its value, after it): taken, and true
    bool Take(int argc, char **argv, int &i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--host") {
            wanted = net.hosting = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                net.port = atoi(next());
            }
        } else if (a == "--join") {
            wanted = true;
            net.address = next();
        } else if (a == "--net-list") {
            net.listed = true;
        } else if (a == "--net-name") {
            net.name = next();
        } else if (a == "--net-invite") {
            // (two waiting room ids: 12 hex digits each)
            const std::string v = next();
            const size_t colon = v.find(':');
            const auto id = [](const std::string &t) { return t.size() == 12 && t.find_first_not_of("0123456789abcdef") == std::string::npos; };
            if (colon != std::string::npos && id(v.substr(0, colon)) && id(v.substr(colon + 1))) {
                net.invite = v.substr(0, colon);
                net.from = v.substr(colon + 1);
            }
        } else if (a == "--net-invite-name") {
            net.inviteName = next();
        } else if (a == "--net-service") {
            net.service = next();
        } else if (a == "--net-port") {
            net.port = atoi(next()); // (the UDP port hosting uses, --host's or the menu's Host a match)
        } else if (a == "--net-delay") {
            net.delay = atoi(next());
        } else if (a == "--net-fake") {
            // LAG[:JITTER[:LOSS]]: each packet sent LAG ms late, give or take JITTER, and LOSS in a hundred lost (tests)
            const std::string v = next();
            const size_t colon = v.find(':'), second = colon == std::string::npos ? colon : v.find(':', colon + 1);
            net.fakeLag = atoi(v.c_str());
            net.fakeJitter = colon == std::string::npos ? 0 : atoi(v.c_str() + colon + 1);
            net.fakeLoss = second == std::string::npos ? 0 : atoi(v.c_str() + second + 1);
        } else if (a == "--net-hashes") {
            hashes = next();
        } else if (a == "--net-log") {
            log = next();
        } else if (a == "--net-say") {
            say = next();
            const size_t colon = say.find(':');
            if (colon != std::string::npos && colon > 0 && say.find_first_not_of("0123456789") == colon) {
                sayAt = strtoull(say.c_str(), nullptr, 10);
                say.erase(0, colon + 1);
            }
        } else {
            return false;
        }
        return true;
    }
};

class OnlineScreen {
public:
    static constexpr uint64_t kChatTicks = 6 * 60; // (how long a line of the chat stays)

    bool leaving = false; // (Esc in a match: "Leave the match?" over the game, which goes on)

    // A key or button in a match: true if online play took it (the chat being typed, Esc's question). `leave` is set
    // when the player confirms leaving.
    bool Event(const SDL_Event &e, Netplay &net, SDL_Window *window, uint64_t tick, bool &leave) {
        if (e.type == SDL_EVENT_TEXT_INPUT && typing) {
            line += e.text.text;
            if (line.size() > Netplay::kChatMost) {
                line.resize(Netplay::kChatMost);
            }
            return true;
        }
        const bool key = e.type == SDL_EVENT_KEY_DOWN, button = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        if (!key && !button) {
            return typing && (e.type == SDL_EVENT_KEY_UP);
        }
        const int code = key ? e.key.scancode : -1;
        const int pad = button ? e.gbutton.button : -1;
        if (typing) {
            if (code == SDL_SCANCODE_RETURN || code == SDL_SCANCODE_KP_ENTER) {
                Said(net, line, tick);
                line.clear();
                Stop(window);
            } else if (code == SDL_SCANCODE_ESCAPE || pad == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
                line.clear();
                Stop(window);
            } else if (code == SDL_SCANCODE_BACKSPACE && !line.empty()) {
                line.pop_back();
            }
            return key; // (the keyboard is the chat's while typing; a gamepad still plays)
        }
        if (leaving) {
            if (code == SDL_SCANCODE_RETURN || code == SDL_SCANCODE_KP_ENTER || pad == SDL_GAMEPAD_BUTTON_SOUTH) {
                leave = true;
                leaving = false;
                return true;
            }
            if (code == SDL_SCANCODE_ESCAPE || code == SDL_SCANCODE_BACKSPACE || pad == SDL_GAMEPAD_BUTTON_EAST ||
                pad == SDL_GAMEPAD_BUTTON_START) {
                leaving = false;
                return true;
            }
            return false;
        }
        // (a gamepad's: the in-game menu's buttons, as the player chose them on the launcher's Controllers page; Start and
        // Select together are port/main.cpp's, with the menu's)
        if ((key && !e.key.repeat && (code == SDL_SCANCODE_ESCAPE || code == SDL_SCANCODE_F1)) ||
            (button && recomp::sdl::MenuButton(static_cast<SDL_GamepadButton>(pad)))) {
            leaving = true;
            return true;
        }
        if ((key && !e.key.repeat && code == SDL_SCANCODE_T) || pad == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            typing = true;
            line.clear();
            SDL_StartTextInput(window);
            return true;
        }
        return false;
    }
    bool Typing() const {
        return typing;
    }
    // What has come in the chat, taken into the lines shown
    void Hear(Netplay &net, uint64_t tick) {
        const std::string name = net.Name(1 - net.Local());
        for (const std::string &heard : net.TakeHeard()) {
            chat.push_back({(name.empty() ? "Player " + std::to_string(2 - net.Local()) : name) + ": " + heard, tick + kChatTicks, false});
        }
        while (chat.size() > 4) {
            chat.erase(chat.begin());
        }
    }
    // A line said (the chat, or --net-say)
    void Said(Netplay &net, const std::string &text, uint64_t tick) {
        if (text.empty()) {
            return;
        }
        net.Say(text);
        const std::string name = net.Name(net.Local());
        chat.push_back({(name.empty() ? "You" : name) + ": " + text.substr(0, Netplay::kChatMost), tick + kChatTicks, true});
        if (!hintShown) {
            hintShown = true;
        }
    }

    // Over the match: each player's name at the bottom, at their side; the ping (and the input delay) between them;
    // "PRESS START TO JOIN" until this side's player has; the chat at the left, above the names
    void Match(Hud &hud, Netplay &net, bool localIn, bool showPing, bool showDelay, bool hideNames, uint64_t tick, uint64_t matchFrame) {
        if (!hideNames) {
            for (int side = 0; side < 2; side++) {
                std::string name = net.Name(side);
                name = "P" + std::to_string(side + 1) + " " + (name.empty() ? (side == net.Local() ? "YOU" : "") : name);
                hud.Text(side ? 0.98f : 0.02f, 0.955f, side ? 1 : -1, name, side ? 0xFF40A0FFu : 0xFFFFA040u);
            }
        }
        char text[64] = "";
        if (net.Silent() > 1000) {
            hud.Text(0.5f, 0.45f, 0, "WAITING FOR THE OTHER PLAYER", 0xFF40FFFFu);
        }
        if (showPing && showDelay) {
            snprintf(text, sizeof text, "PING %d MS  DELAY %d", net.Ping(), net.Delay());
        } else if (showPing) {
            snprintf(text, sizeof text, "PING %d MS", net.Ping());
        } else if (showDelay) {
            snprintf(text, sizeof text, "DELAY %d", net.Delay());
        }
        hud.Text(0.5f, 0.955f, 0, text);
        if (!localIn) {
            hud.Text(0.5f, 0.55f, 0, "PRESS START TO JOIN", (tick / 30) % 2 ? 0xFFFFFFFFu : 0xFF40FFFFu);
        }
        if (leaving) {
            hud.Text(0.5f, 0.40f, 0, "LEAVE THE MATCH?", 0xFF40FFFFu);
            hud.Text(0.5f, 0.46f, 0, "ENTER: LEAVE    ESC: STAY");
        }
        // (the chat: its last lines for a few seconds each, and how to chat as the match begins, or how to send)
        float y = 0.90f;
        if (typing) {
            hud.Text(0.02f, y, -1, "SAY: " + line + ((tick / 20) % 2 ? "_" : " "), 0xFFFFFFFFu);
            y -= 0.045f;
            hud.Text(0.02f, y, -1, "ENTER SENDS   ESC PUTS IT AWAY", 0xFFB0B0B0u);
            y -= 0.045f;
        } else if (matchFrame < 600) {
            hud.Text(0.02f, y, -1, "T: CHAT", 0xFFB0B0B0u);
            y -= 0.045f;
        }
        for (auto it = chat.rbegin(); it != chat.rend(); ++it) {
            if (it->until > tick) {
                hud.Text(0.02f, y, -1, it->text, it->mine ? 0xFFD0D0D0u : 0xFF80FFFFu);
                y -= 0.045f;
            }
        }
    }
    // While no match is on: what is being waited for, the match's code to give, or why the match ended
    void Wait(Hud &hud, Netplay *net, const std::string &ended, uint64_t tick) {
        static const char *const kDots[] = {"", ".", "..", "..."};
        if (!ended.empty()) {
            hud.Text(0.5f, 0.40f, 0, ended, 0xFF40FFFFu);
            hud.Text(0.5f, 0.50f, 0, "ESC: QUIT");
            return;
        }
        if (!net) {
            return;
        }
        hud.Text(0.5f, 0.40f, 0, net->waiting + kDots[(tick / 30) % 4]);
        if (!net->code.empty()) {
            hud.Text(0.5f, 0.48f, 0, "MATCH CODE " + net->code, 0xFF78E660u);
            if (net->Listed()) {
                hud.Text(0.5f, 0.54f, 0, "IN THE LOBBY");
            }
            if (!net->Invited().empty()) {
                hud.Text(0.5f, 0.60f, 0,
                         net->Invitation() == "declined" ? net->Invited() + " declined"
                         : net->Invitation() == "away"   ? net->Invited() + " couldn't be invited"
                                                         : "Invited " + net->Invited());
            }
        }
        hud.Text(0.5f, 0.70f, 0, "ESC: GIVE UP", 0xFFB0B0B0u);
    }

private:
    struct ChatLine {
        std::string text;
        uint64_t until;
        bool mine;
    };
    std::vector<ChatLine> chat;
    bool typing = false, hintShown = false;
    std::string line;
    void Stop(SDL_Window *window) {
        typing = false;
        SDL_StopTextInput(window);
    }
};

} // namespace port

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's port/touch.cpp.
//
// The controls on a touch screen (touch.h).
#include "touch.h"

#include "bindings.h"
#include "../recomp-common/menu.h"
#include "../recomp-common/text.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace port {

namespace {

constexpr int kLeast = 4;          // the frames a press lasts at least (a quick tap's down and up can come in one frame)
constexpr uint64_t kIdleMs = 5000; // no finger for this long: not drawn
constexpr uint32_t kUp = 0x01, kDown = 0x02, kLeft = 0x04, kRight = 0x08; // (bindings.h's stick bits)

// A colour, its alpha premultiplied (red in the low byte)
uint32_t Premultiplied(int r, int g, int b, float alpha) {
    const auto by = [alpha](int c) { return static_cast<uint32_t>(std::lround(c * alpha)); };
    return by(r) | by(g) << 8 | by(b) << 16 | static_cast<uint32_t>(std::lround(255 * alpha)) << 24;
}

// One colour over another, both premultiplied
uint32_t Over(uint32_t top, uint32_t under) {
    const uint32_t keep = 255 - (top >> 24);
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const uint32_t c = ((top >> shift) & 0xFF) + ((under >> shift) & 0xFF) * keep / 255;
        out |= std::min<uint32_t>(c, 255) << shift;
    }
    return out;
}

struct Picture {
    std::vector<uint32_t> &pixels;
    int width, height;

    // A disc, its edge smoothed; `ring`: only that much of it inside the edge (0: filled)
    void Disc(float cx, float cy, float radius, float ring, int r, int g, int b, float alpha) {
        const int x0 = std::max(0, static_cast<int>(cx - radius - 1)), x1 = std::min(width, static_cast<int>(cx + radius + 2));
        const int y0 = std::max(0, static_cast<int>(cy - radius - 1)), y1 = std::min(height, static_cast<int>(cy + radius + 2));
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                const float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
                float cover = std::clamp(radius - d + 0.5f, 0.0f, 1.0f);
                if (ring > 0) {
                    cover *= std::clamp(d - (radius - ring) + 0.5f, 0.0f, 1.0f);
                }
                if (cover > 0) {
                    uint32_t &at = pixels[static_cast<size_t>(y) * width + x];
                    at = Over(Premultiplied(r, g, b, alpha * cover), at);
                }
            }
        }
    }
    // Text in the menu's small font, its middle at (cx, cy)
    void Text(float cx, float cy, float dot, const std::string &text, float alpha) {
        using namespace recomp::text;
        const float w = TextWidth(text.c_str(), dot) - dot, h = 7 * dot;
        float x = cx - w / 2;
        const float y = cy - h / 2;
        const uint32_t colour = Premultiplied(255, 255, 255, alpha);
        for (unsigned char ch : text) {
            const uint8_t *glyph = kFont[(ch < 32 || ch > 126 ? '?' : ch) - 32];
            for (int col = 0; col < 5; col++) {
                for (int row = 0; row < 8; row++) {
                    if (glyph[col] & (1 << row)) {
                        for (int py = static_cast<int>(y + row * dot); py < static_cast<int>(y + (row + 1) * dot); py++) {
                            for (int px = static_cast<int>(x + col * dot); px < static_cast<int>(x + (col + 1) * dot); px++) {
                                if (px >= 0 && py >= 0 && px < width && py < height) {
                                    uint32_t &at = pixels[static_cast<size_t>(py) * width + px];
                                    at = Over(colour, at);
                                }
                            }
                        }
                    }
                }
            }
            x += kCharWidth * dot;
        }
    }
};

constexpr SDL_TouchID kScriptedTouch = 0x5C819700D0000001ull; // (checks' fingers: Scripted)

// A finger on a screen the picture is on (a phone's, a touch screen's), not a laptop's touchpad
bool OnScreen(const SDL_TouchFingerEvent &finger) {
    return finger.touchID == kScriptedTouch || SDL_GetTouchDeviceType(finger.touchID) == SDL_TOUCH_DEVICE_DIRECT;
}

// The stick's eight ways, by the angle of (dx, dy) (down the screen positive): each a quarter of the way round from
// its neighbours
uint32_t Way(float dx, float dy) {
    const float angle = std::atan2(-dy, dx) * 180.0f / 3.14159265f; // (0 right, 90 up)
    const int way = static_cast<int>(std::floor((angle + 360.0f + 22.5f) / 45.0f)) % 8;
    static const uint32_t kWays[8] = {kRight, kUp | kRight, kUp, kUp | kLeft, kLeft, kDown | kLeft, kDown, kDown | kRight};
    return kWays[way];
}

} // namespace

TouchControls::TouchControls(const std::string &buttons) {
#ifdef __ANDROID__
    m_shown = !SDL_HasGamepad(); // (a phone's own controls, unless a gamepad is there from the start)
    m_touched = SDL_GetTicks();
#endif
    // The game's buttons by its names (a shot, a bomb, and so on), else the board's four
    for (size_t from = 0; !buttons.empty() && m_names.size() < static_cast<size_t>(kButtons);) {
        const size_t bar = buttons.find('|', from);
        std::string name = buttons.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
        name.erase(0, name.find_first_not_of(' '));
        name.erase(name.find_last_not_of(' ') + 1);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        // (a long name shortened to what tells it apart: SHOT L and SHOT R, AUTO for Full auto, GUARD for Guard
        // barrier)
        if (const size_t space = name.find(' '); name.size() > 7 && space != std::string::npos) {
            const std::string first = name.substr(0, space), last = name.substr(name.rfind(' ') + 1);
            name = first == "SHOT" ? "SHOT " + last.substr(0, 1) : first == "FULL" ? last : first;
        }
        m_names.push_back(name.substr(0, 7));
        if (bar == std::string::npos) {
            break;
        }
        from = bar + 1;
    }
}

void TouchControls::Allow(bool allowed) {
    if (allowed == m_allowed) {
        return;
    }
    m_allowed = allowed;
    m_fingers.clear();
    m_lingering.clear();
    m_stick = 0;
    Changed();
}

void TouchControls::Follow(bool on) {
    if (on == m_follow) {
        return;
    }
    m_follow = on;
    m_fingers.clear();
    m_lingering.clear();
    m_stick = 0;
    m_tied = false;
    Changed();
}

void TouchControls::Scripted(SDL_EventType type, float x, float y) {
    SDL_Event event{};
    event.type = type;
    event.tfinger.touchID = kScriptedTouch;
    event.tfinger.fingerID = 1;
    event.tfinger.x = m_width > 0 ? x / m_width : 0;
    event.tfinger.y = m_height > 0 ? y / m_height : 0;
    Event(event);
}

std::string TouchControls::Steering() const {
    return m_steering;
}

void TouchControls::Layout(int width, int height, const SDL_FRect &picture) {
    if (width == m_width && height == m_height && picture.x == m_picture.x && picture.y == m_picture.y && picture.w == m_picture.w &&
        picture.h == m_picture.h) {
        return;
    }
    m_width = width;
    m_height = height;
    m_picture = picture;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    m_controls.clear();
    const int count = m_names.empty() ? kButtons : static_cast<int>(m_names.size());
    // (the buttons in a row, or two rows of two for four, about the point (bx, by); in the game's order from the right,
    // so the first, the shot, is the furthest from the stick: AUTO BOMB SHOT)
    auto addButtons = [&](float bx, float by, float button) {
        const float gap = button * 2.35f;
        for (int i = 0; i < count; i++) {
            const int place = count - 1 - i;
            float x, y;
            if (count > 3) {
                const int row = place / 2, column = place % 2;
                x = bx + (column - 0.5f) * gap;
                y = by + (row - 0.5f) * gap;
            } else {
                // (a little rising to the right, as fingers lie)
                x = bx + (place - (count - 1) * 0.5f) * gap;
                y = by - (place - (count - 1) * 0.5f) * button * 0.5f;
            }
            const std::string label = m_names.empty() ? std::to_string(i + 1) : m_names[static_cast<size_t>(i)];
            m_controls.push_back({x, y, button, 1u << (kFirstButton + i), label});
        }
    };
    Control menu{0, 0, 0, 0, "MENU"};
    menu.menu = true;
    if (h > w) {
        // Upright: under the picture (at the top of the window), the stick at the left and the buttons at the right,
        // Menu, Coin and Start in a row along the bottom; over the picture's foot where there is little room
        const float room = std::max(h - (picture.y + picture.h), h * 0.3f);
        const float top = h - room;
        const float small = std::min(w * 0.06f, room * 0.08f), row = h - small * 1.8f;
        const float middle = top + (row - small * 1.5f - top) * 0.5f;
        m_stickRadius = std::min(w * 0.2f, (row - top) * 0.36f);
        m_stickX = w * 0.25f;
        m_stickY = middle;
        m_stickArea = {0, top, w * 0.5f, row - small * 1.2f - top};
        addButtons(w * 0.72f, middle, std::min(w * 0.075f, (row - top) * 0.14f));
        menu.x = w * 0.15f;
        menu.y = row;
        menu.radius = small;
        m_controls.push_back(menu);
        m_controls.push_back({w * 0.65f, row, small, kCoinBit, "COIN"});
        m_controls.push_back({w * 0.85f, row, small, kStartBit, "START"});
    } else {
        // Sideways: each side, the room beside the picture, or a fifth of the window if there is less (then over the
        // picture); the stick at the left, the buttons at the right, Menu, Coin and Start at the top
        const float side = std::max(picture.x, w * 0.2f);
        m_stickRadius = std::min(side * 0.4f, h * 0.2f);
        m_stickX = side * 0.5f;
        m_stickY = h * 0.66f;
        const float small = std::min(side * 0.12f, h * 0.06f), top = small * 1.6f;
        m_stickArea = {0, top + small * 1.4f, side * 1.15f, h};
        addButtons(w - side * 0.5f, h * 0.62f, std::min(side * 0.16f, h * 0.09f));
        menu.x = side * 0.3f;
        menu.y = top;
        menu.radius = small;
        m_controls.push_back(menu);
        m_controls.push_back({w - side * 0.75f, top, small, kCoinBit, "COIN"});
        m_controls.push_back({w - side * 0.25f, top, small, kStartBit, "START"});
    }
    m_knobX = m_stickX;
    m_knobY = m_stickY;
    Changed();
}

int TouchControls::Hit(float x, float y) const {
    // (the nearest whose disc, a little larger than drawn, is under the finger)
    int found = -1;
    float best = 0;
    for (size_t i = 0; i < m_controls.size(); i++) {
        const Control &c = m_controls[i];
        const float d = std::hypot(x - c.x, y - c.y) / (c.radius * 1.3f);
        if (d < 1 && (found < 0 || d < best)) {
            found = static_cast<int>(i);
            best = d;
        }
    }
    return found;
}

bool TouchControls::InStick(float x, float y) const {
    return x >= m_stickArea.x && x < m_stickArea.x + m_stickArea.w && y >= m_stickArea.y && y < m_stickArea.y + m_stickArea.h;
}

void TouchControls::Feed() {
    SDL_Event events[16];
    int got;
    while ((got = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_CANCELED)) > 0) {
        for (int i = 0; i < got; i++) {
            Event(events[i]);
        }
    }
}

bool TouchControls::Event(const SDL_Event &event) {
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_KEY_DOWN:
        // (a gamepad or the keys are being played with; not the system's Back, which a phone has too)
        if (m_shown && !(event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_AC_BACK)) {
            m_shown = false;
            m_fingers.clear();
            m_lingering.clear();
            m_stick = 0;
            Changed();
        }
        return false;
    default: return false;
    }
    if (before && OnScreen(event.tfinger) && before(event, event.tfinger.x * m_width, event.tfinger.y * m_height)) {
        return true;
    }
    if (!m_allowed || !OnScreen(event.tfinger)) {
        return true;
    }
    m_touched = SDL_GetTicks();
    if (!m_shown || m_idle) {
        m_shown = true;
        m_idle = false;
        Changed();
    }
    const float x = event.tfinger.x * m_width, y = event.tfinger.y * m_height;
    const SDL_FingerID finger = event.tfinger.fingerID;
    if (event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_CANCELED) {
        if (auto it = m_fingers.find(finger); it != m_fingers.end()) {
            if (it->second.control == kFollowing) {
                m_tied = false;
            } else if (it->second.control < 0) {
                if (it->second.reads < kLeast && m_stick) {
                    m_lingering.push_back({m_stick, kLeast - it->second.reads});
                }
                m_stick = 0;
                m_knobX = m_stickX;
                m_knobY = m_stickY;
            } else if (it->second.reads < kLeast) {
                m_lingering.push_back({m_controls[static_cast<size_t>(it->second.control)].bits, kLeast - it->second.reads});
            }
            m_fingers.erase(it);
            Changed();
        }
        return true;
    }
    auto it = m_fingers.find(finger);
    if (event.type == SDL_EVENT_FINGER_DOWN) {
        // The stick takes a finger anywhere in its part of the screen away from the buttons; following the finger, a
        // finger anywhere away from them is followed (a second put down: that one, from where it went down)
        const int hit = Hit(x, y);
        const bool stick = hit < 0 && (m_follow || InStick(x, y));
        if (hit < 0 && !stick) {
            return true;
        }
        if (hit >= 0 && m_controls[static_cast<size_t>(hit)].menu) {
            m_menuPressed = true;
        }
        if (stick && m_follow) {
            for (auto other = m_fingers.begin(); other != m_fingers.end();) {
                other = other->second.control == kFollowing ? m_fingers.erase(other) : std::next(other);
            }
            m_tied = false;
        }
        it = m_fingers.emplace(finger, Finger{stick ? (m_follow ? kFollowing : -1) : hit, 0, x, y, x, y}).first;
    } else if (it == m_fingers.end()) {
        return true;
    }
    it->second.nowX = x;
    it->second.nowY = y;
    if (it->second.control == kFollowing) {
        // (steered at each frame's read: Steer)
    } else if (it->second.control < 0) {
        // Eight ways: each a quarter of the way round from its neighbours, none within a fifth of the reach
        const float dx = x - m_stickX, dy = y - m_stickY, d = std::hypot(dx, dy);
        const float reach = std::min(d, m_stickRadius);
        m_knobX = d > 0 ? m_stickX + dx / d * reach : m_stickX;
        m_knobY = d > 0 ? m_stickY + dy / d * reach : m_stickY;
        m_stick = d > m_stickRadius * 0.2f ? Way(dx, dy) : 0;
    } else if (event.type == SDL_EVENT_FINGER_MOTION) {
        // (a finger slid from one button onto another presses that one instead, as on a panel; not onto or off the
        // menu's, which is pressed once, where the finger goes down)
        const int hit = Hit(x, y);
        const auto own = [&](int control) { return m_controls[static_cast<size_t>(control)].menu; };
        if (hit >= 0 && hit != it->second.control && !own(hit) && !own(it->second.control)) {
            it->second = {hit, 0};
        }
    }
    Changed();
    return true;
}

uint32_t TouchControls::Steer(bool menuOpen) {
    const Finger *following = nullptr;
    for (const auto &[finger, held] : m_fingers) {
        following = held.control == kFollowing ? &held : following;
    }
    m_steering.clear();
    if (!following) {
        return 0;
    }
    const float fx = following->nowX, fy = following->nowY;
    float sx = 0, sy = 0;
    if (menuOpen || !m_ship.where || !m_ship.fromWindow || !m_ship.where(sx, sy)) {
        // No ship (or the menu): a stick centred where the finger went down; turned as the cabinet stood, for the
        // game's own screens, the menu as it is drawn (upright on the window)
        m_tied = false;
        m_steering = menuOpen ? "the menu" : "no ship";
        float dx = fx - following->x, dy = fy - following->y;
        if (std::hypot(dx, dy) <= m_stickRadius * 0.2f) {
            return 0;
        }
        if (!menuOpen && m_ship.fromWindow) {
            const float wx = dx, wy = dy;
            m_ship.fromWindow(wx, wy, dx, dy);
        }
        return Way(dx, dy);
    }
    // The ship and the finger tied (where the finger went down, or where it was when the ship came back)
    if (!m_tied) {
        m_tied = true;
        m_fingerX = fx;
        m_fingerY = fy;
        m_axis[0].anchor = m_axis[0].last = sx;
        m_axis[1].anchor = m_axis[1].last = sy;
        m_axis[0].push = m_axis[1].push = 0;
    }
    float moved[2];
    m_ship.fromWindow(fx - m_fingerX, fy - m_fingerY, moved[0], moved[1]);
    const float now[2] = {sx, sy};
    uint32_t bits = 0;
    for (int a = 0; a < 2; a++) {
        Axis &axis = m_axis[a];
        if (axis.push) {
            // (the ship's step learnt as it goes: the game's speed, slower with a shot held in most of Cave's; less
            // than a pixel is a nudge, not its step; pushed and not moving for a few frames, it is against the
            // screen's edge: the finger's further way forgotten, so that its way back moves the ship at once)
            const float went = (now[a] - axis.last) * static_cast<float>(axis.push);
            if (went >= 1.0f) {
                axis.step = std::min(went, 16.0f);
                axis.stuck = 0;
            } else if (went > 0.25f) {
                axis.stuck = 0;
            } else if (++axis.stuck >= 4) {
                axis.anchor = now[a] - moved[a];
                axis.stuck = 0;
            }
        } else {
            axis.stuck = 0;
        }
        axis.last = now[a];
        const float off = axis.anchor + moved[a] - now[a];
        // (let go within half a step, and at least a pixel: held still, the ship stays still)
        const float near = std::max(axis.step * 0.5f, 1.0f);
        axis.push = off > near ? 1 : off < -near ? -1 : 0;
        bits |= a == 0 ? (axis.push > 0 ? kRight : axis.push < 0 ? kLeft : 0) : (axis.push > 0 ? kDown : axis.push < 0 ? kUp : 0);
    }
    char line[128];
    snprintf(line, sizeof line, "ship %.1f,%.1f to %.1f,%.1f step %.1f,%.1f", sx, sy, m_axis[0].anchor + moved[0], m_axis[1].anchor + moved[1],
             m_axis[0].step, m_axis[1].step);
    m_steering = line;
    return bits;
}

uint32_t TouchControls::Read(bool menuOpen) {
    const uint32_t stick = m_follow ? Steer(menuOpen) : m_stick;
    m_stickHeld = stick && stick == m_stickBefore ? m_stickHeld + 1 : 0;
    m_stickBefore = stick;
    // (held past a moment in the menu, let go for a frame in every four: another press of it, on to the next row)
    uint32_t bits = menuOpen && m_stickHeld > 18 && m_stickHeld % 4 == 0 ? 0 : stick;
    for (auto &[finger, held] : m_fingers) {
        if (held.control >= 0) {
            bits |= m_controls[static_cast<size_t>(held.control)].bits;
        }
        held.reads++;
    }
    for (auto &[lingering, left] : m_lingering) {
        bits |= lingering;
        left--;
    }
    m_lingering.erase(std::remove_if(m_lingering.begin(), m_lingering.end(), [](const auto &l) { return l.second <= 0; }), m_lingering.end());
    m_read = Shown() ? bits : 0;
    return m_read;
}

unsigned TouchControls::Menu() const {
    const uint32_t pad = m_read;
    return (pad & kUp ? recomp::kMenuUp : 0) | (pad & kDown ? recomp::kMenuDown : 0) | (pad & kLeft ? recomp::kMenuLeft : 0) |
           (pad & kRight ? recomp::kMenuRight : 0) | (pad & (1u << kFirstButton) ? recomp::kMenuConfirm : 0) |
           (pad & (1u << (kFirstButton + 1)) ? recomp::kMenuBack : 0);
}

bool TouchControls::TakeMenuPress() {
    const bool pressed = m_menuPressed;
    m_menuPressed = false;
    return pressed;
}

bool TouchControls::TakePicture(std::vector<uint32_t> &pixels, int &width, int &height) {
    if (Shown() && !m_idle && m_fingers.empty() && SDL_GetTicks() - m_touched > kIdleMs) {
        m_idle = true;
        Changed();
    }
    if (!m_changed) {
        return false;
    }
    m_changed = false;
    if (!Shown() || m_idle || m_width <= 0 || m_height <= 0) {
        pixels.clear();
        return true;
    }
    Draw();
    // (the overlays take colours as they are, not premultiplied)
    pixels.resize(m_pixels.size());
    for (size_t i = 0; i < m_pixels.size(); i++) {
        const uint32_t p = m_pixels[i], a = p >> 24;
        if (a == 0) {
            pixels[i] = 0;
            continue;
        }
        const auto un = [a](uint32_t c) { return std::min<uint32_t>(255, (c * 255 + a / 2) / a); };
        pixels[i] = un(p & 0xFF) | un((p >> 8) & 0xFF) << 8 | un((p >> 16) & 0xFF) << 16 | a << 24;
    }
    width = m_pictureWidth;
    height = m_pictureHeight;
    return true;
}

void TouchControls::Draw() {
    // At half the window's size (stretched to it)
    const float by = 0.5f;
    m_pictureWidth = std::max(1, m_width / 2);
    m_pictureHeight = std::max(1, m_height / 2);
    m_pixels.assign(static_cast<size_t>(m_pictureWidth) * m_pictureHeight, 0);
    Picture p{m_pixels, m_pictureWidth, m_pictureHeight};
    const float dot = std::max(1.0f, std::round(std::min(m_width, m_height) * by / 260.0f));
    std::vector<bool> down(m_controls.size(), false);
    for (const auto &[finger, held] : m_fingers) {
        if (held.control >= 0) {
            down[static_cast<size_t>(held.control)] = true;
        } else if (held.control == kFollowing) {
            // (following the finger: a faint ring about it, nothing where the ship is)
            p.Disc(held.nowX * by, held.nowY * by, m_stickRadius * 0.3f * by, 2 * dot, 255, 255, 255, 0.3f);
        }
    }
    if (!m_follow) {
        // The stick: its reach, and the knob where the finger holds it
        p.Disc(m_stickX * by, m_stickY * by, m_stickRadius * by, 0, 255, 255, 255, 0.10f);
        p.Disc(m_stickX * by, m_stickY * by, m_stickRadius * by, 2 * dot, 255, 255, 255, 0.35f);
        p.Disc(m_knobX * by, m_knobY * by, m_stickRadius * 0.42f * by, 0, 255, 255, 255, m_stick ? 0.55f : 0.35f);
    }
    // The game's buttons each in a colour (the shot red, the bomb blue, ...), the rest grey
    static const int kColours[4][3] = {{240, 70, 70}, {70, 140, 255}, {255, 200, 50}, {90, 210, 140}};
    for (size_t i = 0; i < m_controls.size(); i++) {
        const Control &c = m_controls[i];
        int button = -1;
        for (int b = 0; b < kButtons; b++) {
            button = c.bits == (1u << (kFirstButton + b)) ? b : button;
        }
        const int *colour = button >= 0 ? kColours[button] : nullptr;
        const int r = colour ? colour[0] : 200, g = colour ? colour[1] : 200, b = colour ? colour[2] : 200;
        p.Disc(c.x * by, c.y * by, c.radius * by, 0, r, g, b, down[i] ? 0.65f : 0.25f);
        p.Disc(c.x * by, c.y * by, c.radius * by, 1.5f * dot, r, g, b, 0.7f);
        // (a long label smaller, to fit)
        const float size = std::max(1.0f, std::floor(c.radius * by * 1.6f / (std::max<size_t>(c.label.size(), 3) * recomp::text::kCharWidth)));
        p.Text(c.x * by, c.y * by, std::min(size, dot * 1.5f), c.label, 0.85f);
    }
}

} // namespace port

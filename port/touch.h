// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's port/touch.h.
//
// Controls on a touch screen (a phone): a stick, the game's buttons (game.cfg's `buttons`, up to four), and Coin,
// Start and the menu; player 1's, beside a gamepad's and the keys. Held sideways they go where the window has room
// beside the picture (a standing game's picture leaves a phone most of its width), over its edges where it has not;
// held upright, under the picture, which then goes to the top of the screen. They are shown on Android until a
// gamepad or a key is used (and not at all with the Touch controls feature off), and elsewhere only once a touch
// screen is touched. With no finger on the screen for five seconds they are no longer drawn (the picture whole
// again); a touch brings them back, and is a press of what is under it, as when they are drawn.
// In the menu the stick moves through it and the first two buttons choose and go back.
// A finger is read when the game reads its controls (Feed, from the port's read), as the keys are: no frame later.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace port {

class TouchControls {
public:
    // `buttons`: the game's names for its buttons ("Shot|Bomb|...")
    explicit TouchControls(const std::string &buttons);
    // The window's size in its pixels, and where the picture is in it
    void Layout(int width, int height, const SDL_FRect &picture);
    // An event of the window's: a finger's is the controls' (true); a gamepad's or a key's hides them
    bool Event(const SDL_Event &event);
    // The finger events waiting in SDL's queue taken now (after SDL_PumpEvents), each given to Event
    void Feed();
    bool Shown() const {
        return m_shown && m_allowed;
    }
    // Off (the Touch controls feature): never shown, never pressed
    void Allow(bool allowed);
    // Their picture to be given again (TakePicture), to a presenter made anew
    void Stale() {
        m_changed = true;
    }
    // Upright: the picture is to go at the top of the window (the controls under it)
    bool WantsPictureAtTop(int width, int height) const {
        return Shown() && height > width;
    }
    // Player 1's controls held (port/bindings.h's bits), read once a frame: a press lasts a few frames however short
    // the tap; and the menu's choose and back (recomp::kMenu* bits) as last read. With the menu open, the stick held
    // in one way goes on through its rows, as a held key does in a list.
    uint32_t Read(bool menuOpen);
    unsigned Menu() const;
    // The menu button was pressed since this was last asked
    bool TakeMenuPress();
    // Their picture, when it changed since this was last asked (and for nothing shown, an empty one): half the
    // window's size, stretched over the whole window; red in the low byte, alpha (how much shows) in the high, not
    // premultiplied (as the overlays are: gpu_base.h, shading.h)
    bool TakePicture(std::vector<uint32_t> &pixels, int &width, int &height);

private:
    struct Control {
        float x, y, radius; // in the window's pixels
        uint32_t bits;      // what it presses (none for the menu button)
        std::string label;
        bool menu = false;
    };
    int Hit(float x, float y) const;
    bool InStick(float x, float y) const;
    void Changed() {
        m_changed = true;
    }
    void Draw();

    bool m_shown = false, m_allowed = true, m_changed = true, m_menuPressed = false;
    bool m_idle = false;    // (shown, but no finger for a while: not drawn)
    uint64_t m_touched = 0; // (when a finger was last on the screen, SDL_GetTicks)
    int m_width = 0, m_height = 0;
    SDL_FRect m_picture{};
    float m_stickX = 0, m_stickY = 0, m_stickRadius = 0; // the stick's middle and reach
    float m_knobX = 0, m_knobY = 0;                      // where its finger is
    SDL_FRect m_stickArea{};                             // where a finger going down takes the stick
    std::vector<Control> m_controls;
    std::vector<std::string> m_names; // the game's buttons'
    struct Finger {
        int control; // the stick (-1) or a control
        int reads;   // frames it has been read in
    };
    std::map<SDL_FingerID, Finger> m_fingers;            // each finger down
    std::vector<std::pair<uint32_t, int>> m_lingering;   // a short tap's: its bits, and the frames still to give them
    uint32_t m_stick = 0, m_read = 0, m_stickBefore = 0;
    int m_stickHeld = 0; // the frames the stick has been held the same way
    std::vector<uint32_t> m_pixels;
    int m_pictureWidth = 0, m_pictureHeight = 0;
};

} // namespace port

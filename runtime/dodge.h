// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Dodge training (an alternative mode): the player's ship alone against an emitter at the top of the screen, where a
// boss would be, that fires the game's own enemy fire, a pattern at a time, for practice at dodging it. The patterns
// are the game's, taken from recordings of play (tools/dodge/extract.py): each a burst of bullets with, for every
// bullet, when it comes, where, its kind and speed, and its angle: fixed, or the game's own aim at the ship plus a turn.
// The bullets are made by the game's own routine and are the game's from then on (it moves them, draws them and tests
// them against the ship).
//
// What a game gives is in Facts; the rest (which pattern is fired when, the scenarios, the menu's page, the words on
// the picture) is here. All that changes from frame to frame is kept in the game's own memory (Facts::state), so the
// practice point, rewind and run-ahead put it back with the game.
#pragma once

#include "../machine/board.h"
#include "../recomp-common/menu.h"
#include "../recomp-common/settings.h"

#include <cstdint>
#include <string>
#include <vector>

namespace spelunk::dodge {

// Bullet::how's bits
enum : uint8_t {
    kHowMask = 7,
    kFixed = 0,     // the angle as it is
    kWithFirst = 1, // the burst's first bullet's angle, as it was fired this time, plus this turn
    kAimEach = 2,   // the game's aim at the ship now, plus this turn
    kAimFirst = 3,  // the aim as it was when the burst began (from the first bullet's place), plus this turn
    kFromObject = 8, // (an aim) from the firing object's place, not the bullet's
    kFineAim = 16,   // (an aim) the game's aim of 256 directions, not its one of 64
    kFine = 32,      // made by the game's routine that takes one of 256 directions (else the one of 64)
    kParent = 64,    // the bullet is given its firing object as a parent (it stays with it a while)
};

struct Bullet {
    uint16_t dt;    // frames after the burst's first
    int16_t dx, dy; // from the emitter, in the game's units (64ths of a pixel), across and down the board
    int16_t ox, oy; // the firing object's place, likewise
    uint8_t type;
    int8_t speed;
    uint8_t angle; // of 256
    uint8_t how;
};

struct Pattern {
    uint8_t stage, loop, boss, object;
    uint16_t first, count; // its bullets
    uint16_t length, pause; // frames: the burst's, and from one burst's beginning to the next's
    uint16_t seen;
    uint16_t rank; // among all, 0 the easiest
    uint16_t firstSprite, spriteCount; // its look: what fires it, as the game drew it (none: the game's stand-in)
    uint16_t firstColours, coloursCount; // the look's colours, for the game's palette
};

// One of the sprites' colours as a look needs it: which, and its palette entries
struct Colours {
    uint16_t colour;
    uint16_t entries[16];
};

// A sprite of a pattern's look, as the game's own sprite list has one: across and down from the emitter in the board's
// pixels, and the game's words for its look, number and size
struct Sprite {
    int16_t dx, dy;
    uint16_t look, number, size;
};

struct Facts {
    const Pattern *patterns = nullptr;
    size_t patternCount = 0;
    const Bullet *bullets = nullptr;
    const Sprite *sprites = nullptr;
    // The stages' names (the last may be a hidden boss's, only in the second loop)
    std::vector<std::string> stages;
    // Where the emitter is (the game's units)
    int emitterX = 0, emitterY = 0;
    // 44 bytes of the game's memory nothing else uses while the mode is on (cleared by the game when a game begins)
    uint32_t state = 0;
    // A bullet made by the game's own routine: its place, kind, speed, angle (of 256) and the Bullet::how bits kFine
    // and kParent. Called from inside the game's frame (Step)
    void (*fire)(Board &board, int x, int y, int type, int speed, int angle, unsigned how) = nullptr;
    // The game's aim (of 256) from a place at the ship
    int (*aim)(Board &board, int x, int y, bool fine) = nullptr;
    // Every bullet gone, and the game put in the loop (0 the first) the next pattern is from
    void (*clear)(Board &board, int loop) = nullptr;
    int (*loop)(Board &board) = nullptr;
};

// The mode's page (under Alternative modes: runtime/game.h), its features (what is fired) and its row that begins it
constexpr const char *kPage = "Dodge training";
std::vector<recomp::FeatureInfo> Features(const Facts &facts);
recomp::MenuCommand BeginRow();

// The frame's choices, from the features' values (`values` at the first of them) and the cabinet's Start (held: the
// next pattern); before each frame
void Choices(const Facts &facts, const int *values, bool start);
// Whether the mode was begun (from its row: a new game is then the mode's, until the board is made anew for anything else)
void SetOn(bool on);
bool On();
// The emitter's turn, once a frame from inside the game's frame while the mode's arena is up: fires what is due
void Step(Board &board, const Facts &facts);
// A ship was lost (counted and shown)
void Hit(Board &board, const Facts &facts);
// The mode's state made new (the arena has just come up)
void Begin(Board &board, const Facts &facts);
// The patterns the emitter has now (two at most; -1: none), fired or about to be: what it is drawn as
void Showing(const Board &board, const Facts &facts, int patterns[2]);
// The words for the top of the picture: the pattern being fired, and the hits taken
std::string Readout(const Board &board, const Facts &facts);

} // namespace spelunk::dodge

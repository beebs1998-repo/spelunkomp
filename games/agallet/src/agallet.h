// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Air Gallet (agallet, the European set of 1996): what is where in its memory, with how each was found.
// (The training facts found 2026-11-15 by RAM dumps of headless runs, a game played three stages through with the safe
// time held, and the code that reads each word. The program is of DonPachi's kind: a main loop of eight tasks, records
// with a box at the same places, a hit test of the same shape.)
#pragma once

#include <cstdint>

namespace spelunk::agallet {

// The credits, a word of work RAM whose low byte this is (found 2026-11-15: three coins, the bytes that went 0, 1, 2, 3:
// 1000AB, a count of coins, and 1001CD and 1001CF; this is the one the screen's CREDIT shows, and 1001CF its value
// before the last coin: the coin routine, 010688, copies the one to the other). Kept at 1, Start begins a game: at the
// second press (games/agallet/src/game.cpp)
constexpr uint32_t kCredits = 0x1001CD;

// --- The main loop's tasks ---
// Eight records of 16 bytes from 100006, task n running the jump at 000100 + 8n (0004A8), started by 00D674 (d0 the
// task). Task 0 is the game's own (005B04: the attract show, a game's beginning and end), task 1 the stage (00AC7E),
// tasks 2 and 3 the two players' ships (001FF6, 001FBC)
// What the game is doing (a word): 4 the title, 3 the logo, 2 the ranking, 5 the demonstration, 6 from a game's
// beginning to its end (through its continues)
constexpr uint32_t kMode = 0x10080C;
constexpr uint32_t kModePlay = 6;

// --- A game's beginning ---
// A new game for the player who pressed Start (003D4C: the stage and the players' weapons cleared, the stage's task
// started, the ships counted, the credit taken, kMode set to 6). This place is in it, after the stage is cleared and
// before the task that reads it has run: the stage chosen is put in kStage here, and the game begins at it as the
// attract show's demonstration begins at its own (003E74, which takes kStage from a table). A later stage begins with
// the ship flying, a new game's weapons and bombs; only the first has the launch from the carrier
constexpr uint32_t kNewGame = 0x003DC2;
// The stage (a word: 0 to 5; 6 after the last, the ending), one more as a stage's tally ends (003634). The stage's task
// copies it to 1002FC as it sets a stage up (00ACD2), which the rest of the game reads
constexpr uint32_t kStage = 0x1001C0;
// The stage's task setting a stage up (its first run: bit 5 of its record not yet set, 00AC7E)
constexpr uint32_t kBeginStage = 0x00AC86;
// The stage's task copies the stage to this word as it sets a stage up (00ACD2); the rest of the game reads it here
constexpr uint32_t kStageNow = 0x1002FC;
// How far the stage has come (a long; its high word counts up about 62 a thousand frames; 0 as a stage begins)
constexpr uint32_t kPlace = 0x1002F2;
// The boss: no word says a boss is on. The stage's task asks for the bosses' tune (7) when kPlace is the long its
// table of eight bytes a stage has for the stage (00B152; the table at 00B110: stages 2 to 6 at 1E4, 262, 24A, 1A7 and
// 2AE in the high word; kPlace goes on from there and stops with the boss). The first stage's own script asks for it
// at 128 (00B646), and at 138 sets this word to 1 and starts kPlace again from 0 (00B7DC: the chase of the shuttle,
// its boss; the word is cleared with the stage). (Found by watching the tunes asked for, 00D9A0, in stages 1, 2, 4
// and 6)
constexpr uint32_t kBossTunes = 0x00B110;
constexpr uint32_t kFirstBossPlace = 0x128, kFirstBossOn = 0x10972A;

// --- Player 1 ---
// The ships left, the one flown among them (a word: 3 for a new game, from the board's setting 1000D6; the screen
// shows one icon fewer). A ship lost takes one (0030BE) and at 0 the continue is offered (0036E0). Player 2's: 1002EE
constexpr uint32_t kShips = 0x1002EC;
// The ship's record (0xC0 bytes; player 2's at 100E10), flown while its first word is negative. Its place is the words
// at +2 across and +4 down the board's 320 x 240, in 128ths of a pixel
constexpr uint32_t kShip1 = 0x100D50, kShip2 = 0x100E10;
// The frames the ship is still out of harm for (a word at +0x20: counted down a frame at a time, 0x78 for a new ship,
// FFFF while a stage's end holds it; while it is not 0 the ship's task clears the hit the test marks: 0020FC)
constexpr uint32_t kSafeFrames = kShip1 + 0x20;
// The bombs left (a word at +0x22, its low byte this: 3 for a new ship, 5 at most: 0025F4)
constexpr uint32_t kBombs = kShip1 + 0x23;

// --- What hits the ship (the game's own test, 01005E for player 1 and 01004E for player 2, a5 the ship's record) ---
// A record's box goes from its place less the word at +0x12 to its place plus the word at +0x10 across, less +0x16 to
// plus +0x14 down (both ends in it), as in DonPachi. A hit sets bit 4 of the ship's first byte
constexpr int kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets: 50 records of 0x20 bytes, each a box; one hits when its first word is negative with bit 8 and its box
// meets the ship's (0100DA)
constexpr uint32_t kBullets = 0x101ED0;
constexpr int kBulletSize = 0x20, kBulletCount = 50;
// The enemies: 156 records of 0x40 bytes. One whose first word is negative with bit 14 hits the ship when the ship's
// place (a point, not its box) is inside its box (0101A8). The test takes every other record a frame (the odd ones,
// then the even: bit 1 of 1001F1), and only those from the 58th on unless the word at 109270 is set: the 57 before
// them are the things on the ground
constexpr uint32_t kEnemies = 0x1055D0;
constexpr int kEnemySize = 0x40, kEnemyCount = 156, kEnemiesOnGround = 57;
constexpr uint32_t kGroundHits = 0x109270;
constexpr uint16_t kHitsShip = 0x4000;
// --- What the player's shots hit (the same routine on, 0101F4) ---
// Every enemy whose first word is negative with bit 13, against each of the player's shots
constexpr uint16_t kShootable = 0x2000;
// Each player's shots: 32 records of 0x30 bytes, each a box; tested while the first word is negative without bit 12
// (its hit this frame), its power the word at +0x18
constexpr uint32_t kShots1 = 0x100F10, kShots2 = 0x101510;
constexpr int kShotSize = 0x30, kShotCount = 32;
// (not drawn: the pickups, 20 records of 0x20 bytes from 102510, tested against the ship's box at 010134; the bombs'
// blasts, 160 records of 0x20 bytes from 102DD0 and 1041D0)

// --- Other things read on the way ---
// 100092: a word that keeps the ship out of harm while not 0 (the ship's task, 002130: a setting of the board's,
// not used here). 109274 / 109276: each player's controls as the game reads them. 1001C4: the controls just pressed
// (a long; bit 7 of each word is Start). 1001D2: negative for the board's own free play (its coin setting 15)

} // namespace spelunk::agallet

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Ou-Jou Black Label (ddpdojblk): what is where in its memory, with how each was found.
// (Found 2026-11-11. Black Label runs from the B half of the program, 200000 on: its code is Dai-Ou-Jou's (ddpdoj.h,
// where each place is described) moved by about 100000, and its work RAM partly moved; each place below was found
// by the same code in this half, and checked by RAM dumps of headless runs: ships lost, a bomb used, a run with the
// safe time held through three stages.)
#pragma once

#include <cstdint>

namespace spelunk::ddpdojblk {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the byte that went 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x80395A;

// --- A game's beginning (Dai-Ou-Jou's 15B892, 15E88E) ---
// A real game is begun at 25CA78 by making the game's task (kind 0x0A) and putting its stage, 0, in its word at +4
// (25CA82); here, just after, a0 is the task. (The attract show's demonstration makes its task at 25C5E2, from a table
// of its own; 25CEA2 is a third way in, not taken by a game begun with player 1's Start.)
constexpr uint32_t kNewGame = 0x25CA88;
constexpr int kTaskStage = 4;
// The routine that sets the stage being played (d0): at a game's beginning and at each stage's start after a boss
constexpr uint32_t kSetStage = 0x25FD0C;
constexpr uint32_t kStage = 0x813092; // the stage, from 0 (a word)

// While a stage's boss is on (a word: 0 through the stage and its mid-bosses; set from its warning to the stage's
// end; bits 3 and 4 of the high byte set as the boss is beaten, 242958)
constexpr uint32_t kBossOn = 0x8130F8;

// --- Player 1's record ---
// The ships left besides the one flown (the low byte of the word at 8130BE: 2 for a new game, 0xFF while no game is
// played)
constexpr uint32_t kShips = 0x8130BF;
// The bombs left, and what they are filled to at a new ship (bytes)
constexpr uint32_t kBombs = 0x81040A;
constexpr uint32_t kBombStock = 0x81040B;
// The ship's record (player 2's at 810448): flown while its first word is negative. The byte at +0x3E is the frames
// left out of harm (0xF0 as a ship comes back, 2493F2; counted down at 249536; 0xFF held at a bomb, 242938)
constexpr uint32_t kShip1 = 0x8103E6, kShip2 = 0x810448;
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;

// --- What hits the ship (the game's own test: 244D62 for a ship at a4; Dai-Ou-Jou's 1440A2) ---
// As in Dai-Ou-Jou: positions in 64ths of the board's pixels, a record's position at +2 and +4 and its box about it
// (+0x12 left, +0x10 right, +0x16 up, +0x14 down; both ends in it)
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (2459D0): records of 0x40 bytes, each a point that hits when inside the ship's box and its first byte
// has none of the bits 0x51; 70 tested, more while the words from 81B414 are set in turn: 110, 160, 190, 210
constexpr uint32_t kBullets = 0x817F8C;
constexpr int kBulletSize = 0x40;
constexpr uint32_t kMoreBullets = 0x81B414;
constexpr int kBulletCounts[5] = {70, 110, 160, 190, 210};
// The enemies whose bodies hit the ship (244E5C): records of 0x20 bytes; as many tested as the words at 815E9E and
// 815EA0 add up to; one hits when its first word has bit 0 and the boxes meet (moved by 0x2800, left edge below 0xF800)
constexpr uint32_t kEnemies = 0x81459C;
constexpr int kEnemySize = 0x20;
constexpr uint32_t kEnemyCount1 = 0x815E9E, kEnemyCount2 = 0x815EA0;

// --- What the player's shots hurt (the same routine after the ship's test, 244EE0 on; Dai-Ou-Jou's 144220) ---
// As in Dai-Ou-Jou (ddpdoj.h, where each is described): the enemies in two groups, 100 from kEnemies, then the
// ground's 50 from here; a shot hurts those with bit 13 (0x2000); those without bit 0 are drawn as the ground's
constexpr uint32_t kGroundEnemies = 0x81521C;
constexpr int kAirSlots = 100, kGroundSlots = 50;
constexpr int kShootable = 0x2000;
constexpr int kAirMoved = 0x2800, kAirOnScreen = 0x9700, kGroundMoved = 0x1800, kGroundOnScreen = 0x8800;
// The shots (a0 at 28B6B8): 36 records of 0x30 bytes for each player, their boxes as Dai-Ou-Jou's
constexpr uint32_t kShots1 = 0x810572, kShots2 = 0x810C32;
constexpr int kShotSlots = 36, kShotSize = 0x30;
constexpr int kShotRight = 0x10, kShotWidth = 0x12, kShotDown = 0x14, kShotHalfHeight = 0x16, kShotPower = 0x18;
// The laser's three records (a2, a3 the aura, a1 the beam at 28B6B8), tested while the ship's byte at +0x3F is not 0
constexpr uint32_t kLaserOther1 = 0x811802, kLaserOther2 = 0x811E02;
constexpr uint32_t kLaserAura1 = 0x811892, kLaserAura2 = 0x811E92;
constexpr uint32_t kLaser1 = 0x811EF2, kLaser2 = 0x811F12;
constexpr int kShipLaserOn = 0x3F;
constexpr int kLaserLength = 6, kLaserDown = 8, kLaserUp = 0xA, kLaserEnd = 0x10;

// A word negative while player 1's ship is flown (its stage is set before the ship is chosen: Begin at the stage
// presses the shot on until it is)
constexpr uint32_t kShipFlown = 0x8103E6;
} // namespace spelunk::ddpdojblk

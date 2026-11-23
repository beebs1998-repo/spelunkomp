// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Ibara Kuro Black Label: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::ibarablk {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C2860B0u;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C05176Au;

// The same engine as Ibara's (games/ibara/src/ibara.h says what each is); its places found 2026-11-11 by the same
// routines' code.

// --- Player 1's ship (a record of 0x2A0 bytes; the routine that loses a ship is 0x0C077190) ---
constexpr uint32_t kShipRecord = 0x0C29D1D8u;
constexpr uint32_t kShipState = 0x0C29D1E0u; // (32 bits) 1-9 flown, 0x10 more as it is hit, 0x20 lost
constexpr uint32_t kShips = 0x0C29D432u;     // the ships left besides the one flown (record +0x25A)
constexpr uint32_t kBombs = 0x0C29D435u;     // the bombs in stock (record +0x25D; the bomb button's routine 0x0C073600)
// Out of harm while flown, the frames counted to 60 while lost (record +0x288): held at 59 (Ibara's kSafeFrames)
constexpr uint32_t kSafeFrames = 0x0C29D460u;
constexpr uint32_t kSafeFramesHeld = 59;

// --- The game's script (as Ibara's) ---
constexpr uint32_t kBeginStage = 0x0C0724B0u;  // command 2's routine ("begin stage")
constexpr uint32_t kScript = 0x0C2ABAA4u;      // the script's next word
constexpr uint32_t kStagesBegun = 0x0C3C4170u; // (a byte)
constexpr uint32_t kStageScript = 0x0C23E0A6u; // the stage script being played (a byte)
constexpr int kBeginStageSize = 6;
// The sound chip's sequences a boss comes on to (as Ibara's: 9 and 10, the last stage's 7 and 8; found by listing the
// sequences started through a whole game played with the ship out of harm)
constexpr int kBossTunes[] = {9, 10, 7, 8};

// --- What hits the ship (as Ibara's: ibara.h; the tests are 0x0C06ECE0, bodies, and 0x0C06EF50, bullets) ---
constexpr uint32_t kCamera = 0x0C3C4048u;
constexpr uint32_t kShipBoxes = 0x0C17E12Cu;
// The bullets: 256 records of 84 bytes, the box list at +32, the position at +40, +44. Unlike Ibara's, the test takes
// only where each of a bullet's boxes is (a point, given a half-size of 1 each way), not its size
constexpr uint32_t kBullets = 0x0C297B40u;
constexpr int kBulletCount = 256, kBulletSize = 84;
// The enemies whose bodies are tested: 192 records of 180 bytes (as Ibara's); only the ship's boxes without the flag
// 0x20000000 are tested against them
constexpr uint32_t kEnemies = 0x0C286578u;
constexpr int kEnemyCount = 192, kEnemySize = 180;

// --- What the shots hit (as Ibara's: ibara.h; the tests are 0x0C06E1D0, the shots, and 0x0C06E540, the other shots) ---
// An enemy is tested against the shots while +0 has kShootable; one lacking 0x10000000 is a ground one
constexpr uint32_t kShootable = 0x02000000u;
// A shot: a record of 52 bytes, tested while +0 has 0x08000000; its boxes from kShotBoxes by +4, +8, +12 (as the
// ship's), its position at +16, +20
constexpr uint32_t kShotBoxes = 0x0C17E1B0u;
constexpr int kShotSize = 52;
// The shots: 32 for each player from kShots (player 2's 0x680 on); the other shots: three groups of 16 for each from
// kOtherShots (0x340 apart, player 2's 0x9C0 on)
constexpr uint32_t kShots = 0x0C2A6D9Cu, kOtherShots = 0x0C2380D0u;

} // namespace spelunk::ibarablk

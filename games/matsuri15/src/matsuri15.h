// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama Matsuri Ver 1.5: where things are in the game's memory, with what each was found from.
// The program is Mushihimesama's reworked for the 2011 Cave Matsuri: the same engine, but most routines and nearly all
// of RAM moved, so each place here was found again in this program (games/mushisam/src/mushisam.h is the model).
#pragma once

#include <cstdint>

namespace spelunk::matsuri15 {

// The credits (a byte): the same place as Mushihimesama's (three coins counted 0, 1, 2, 3 there)
constexpr uint32_t kCredits = 0x0C056BE7u;
// The main loop's wait for the next frame: the same address as Mushihimesama's
constexpr uint32_t kIdleLoop = 0x0C04A2AAu;

// --- A game's beginning ---
// As Mushihimesama's: the routine that sets a new game up takes the stage in r4, the one that begins a stage's play
// takes it in r5. Found by breaking on them: a real game calls the first from kNewGameCaller (the attract show from
// 0x0C05CB6A), and the second is called with the stage record (0x0C1FB5C0) in r4 and 0 in r5 as stage 1 begins.
constexpr uint32_t kNewGameCaller = 0x0C0D988Cu;
constexpr uint32_t kNewGame = 0x0C05C590u;    // (stage in r4)
constexpr uint32_t kBeginStage = 0x0C05DBC0u; // (stage in r5)
// The sound chip's sequences a boss comes on to (Mushihimesama's numbers; checked on the trip to stage 1's boss)
constexpr int kBossTunes[] = {4, 5, 16, 17};

// --- Player 1's record (0x0C1FBC0C: the literal the hit test's ship-state write uses; Mushihimesama's layout, with 8
// more bytes before the ship's state and more again before the out-of-harm count) ---
constexpr uint32_t kShipRecord = 0x0C1FBC0Cu;
constexpr uint32_t kShips = 0x0C1FBC17u;      // the ships left (a byte; one less as a ship is lost, when bombs go back to 3)
constexpr uint32_t kBombs = 0x0C1FBC1Bu;      // the bombs left (a byte; one less as a bomb is set off, by 0x0C05FE9C)
constexpr uint32_t kShipState = 0x0C1FBC44u;  // 1 while the ship is flown, 2 as it is lost (written by 0x0C05FEEC)
constexpr uint32_t kShipX = 0x0C1FBC50u;      // where a ship starts (16.16)
constexpr uint32_t kShipAt = 0x0C1FBC58u;     // where the ship is now (16.16, across then up)
// The frames the ship is still out of harm for (0x100 as it appears, counted down by 0x0C05F5A0): held up, nothing hits it
constexpr uint32_t kSafeFrames = 0x0C1FBCE0u;

// --- What hits the ship (the game's hit test at kHitTest, called each frame with the ship's record in r4; found as
// the routine that called the write of 2 to kShipState) ---
constexpr uint32_t kHitTest = 0x0C05B3E0u;
// The ship's box: a pointer at record+0xE8 (read by 0x0C05F9D0), 0 while out of harm; boxes as Mushihimesama's (the
// position at +12/+16, how far it reaches right +20, left +24, up +28, down +32, all 16.16)
constexpr uint32_t kShipBox = kShipRecord + 0xE8u;
// The bullets: a pointer at kBullets to a block where the list is at +0x7A000 and the count at +0x7E000 (0x0C08EE40);
// entries of 8 bytes, the bullet's record at +4, its position at +12, +16
constexpr uint32_t kBullets = 0x0C19CC40u;
constexpr uint32_t kBulletList = 0x7A000u, kBulletCount = 0x7E000u;
constexpr uint32_t kBulletStride = 8u, kBulletRecord = 4u;
// The enemies' bodies: lists of lists, headers of 8 bytes here (the count at +0, the first record at +4; records 80
// bytes apart, tested while the word at +4 is not 0, the box from +12). The second list's places from 0x0C098AE0
constexpr uint32_t kBodyHeaders[2] = {0x0C1FA120u, 0x0C2A2B00u};
constexpr uint32_t kBodyHeaderCounts[2] = {0x0C1FA11Cu, 0x0C2A2AFCu};
constexpr uint32_t kHeaderStride = 8u, kHeaderFirst = 4u;

// --- What the shots hit (the collision pass at 0x0C05ACF0, which calls kHitTest first, then each body list against
// the shots and lasers) ---
constexpr uint32_t kHurtByShotsFirst = 1u; // (+0 must be this in the first list)
// The shots: headers as the bodies'
constexpr uint32_t kShotHeaders = 0x0C1F917Cu, kShotHeaderCount = 0x0C1F9178u;
// The lasers (from 0x0C09EE40): entries of 8 bytes, each one record at +4
constexpr uint32_t kLasers = 0x0C2BFF78u, kLaserCount = 0x0C2BFF74u;

} // namespace spelunk::matsuri15

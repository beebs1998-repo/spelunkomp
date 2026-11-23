// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Fukkatsu Black Label: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::dfkbl {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C1E0D7Fu;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C1D1346u;


// --- As Dai-Fukkatsu 1.5 (games/ddpdfk/src/ddpdfk.h), found the same ways: the stage-begin routine by its code's
// shape, the record by an idle ship being lost, the safe time as the countdown that keeps it alive, the tunes by
// listing those started through three stages ---
constexpr uint32_t kBeginStage = 0x0C1F04B0u; // (stage in r5)
constexpr uint32_t kBeginStageCaller = 0x0C2A5572u;
constexpr int kBossTunes[] = {7, 8, 3, 4};
constexpr uint32_t kShips = 0x0C71742Bu;      // the ships left (a byte)
constexpr uint32_t kBombs = 0x0C71742Eu;      // the bombs left (a byte; not tried with a bomb used)
constexpr uint32_t kShipState = 0x0C717498u;  // 1 in play
constexpr uint32_t kSafeFrames = 0x0C7175C0u;

// What hits the ship: the collision kinds of games/ddpdfk/src/ddpdfk.h (found 2026-11-11 from the ship's own member:
// its box, 2 each way, found in RAM, back through the group that holds it to the table), tested by a pass rewritten for
// the Black Label (0x0C1E64E0): both players in one, a position the whole part of the member's and its owner's 16.16
// values added; the bullets (0x0C1E8120) tested on the frames the bodies and items (0x0C1E8860, 0x0C1E83C0,
// 0x0C1E8620) are not, as the frame counter's low bit (0x0C7159F0) and the player go, and a group of bullets far from
// the ship left out for a few frames (the boxes are drawn on every frame)
constexpr uint32_t kKinds = 0x0C713AACu;
constexpr bool kTestedLong = true, kAddFixed = true;
// (each group of bullets tested by its first member alone)
constexpr bool kBulletFirstOnly = true;
constexpr uint32_t kPosition = 56, kBox = 36;
constexpr int kShipKind = 2, kShipKindAll = 14, kBulletKind = 20;
constexpr int kBodyKinds[] = {0, 1, 21};
// What the shots hit (found 2026-11-12: the enemies' pass 0x0C1E67B0, run after the ship's, tests each member of the body kinds whose
// 32 bits at +12 are not 0 against the player's shots: kinds 6 and 7 (0x0C1E8D10) and 10 and 11 (0x0C1E91D0), player 2's
// 8, 9, 12 and 13, unless bit 7 of the member's byte at +27 is set; and kind 4 (player 2's 5)
// (0x0C1E8AB0), unless bit 1 of it is set. The shots' boxes are read as every other kind's.) A member they can hurt
// that does not hit the ship (bit 0 of +27 clear) is a ground enemy: a tank, a turret
constexpr int kShotKinds[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
constexpr uint8_t kNotShot = 0x80, kNotShotByKind4 = 0x02;

// The ship's box (0x0C20E096): as Dai-Fukkatsu 1.5's, the table here, the ship's type its byte at +0xC1
constexpr uint32_t kSafeReach = 24, kShipSizes = 0x0C489DB0u, kShipType = 0xC1, kSizesAt = 12, kSizesMode = 0, kSizesModeAt = 0;

} // namespace spelunk::dfkbl

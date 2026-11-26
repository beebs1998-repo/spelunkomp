// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Fukkatsu Ver 1.0: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::ddpdfk10 {

// Ver 1.0's program is an earlier build of Ver 1.5's: everything here was found 2026-11-23 from games/ddpdfk/src/ddpdfk.h,
// its routines by their instruction bytes (each the same, a little lower) and its places in RAM through the literal
// pools of the same code (the player's record 0x1340 lower, the collision kinds 0x1338 lower, the ships' reaches 0x1208
// lower; the credits where they were). The rest of what each says was found in Ver 1.5.

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C1E0D7Fu;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C1D1346u;


// --- A game's beginning: the routine that begins a stage's play takes the stage in r5 (found by its code's shape,
// as SaiDaiOuJou's; a real game calls it from kBeginStageCaller). A game begun at a later stage starts with
// 1,000,000 points: the game's own doing ---
constexpr uint32_t kBeginStage = 0x0C1F0000u;
constexpr uint32_t kBeginStageCaller = 0x0C2A2312u;

// The sound chip's sequences a boss comes on to (found by listing the sequences started through the first three
// stages: 7 and 8 at the first and third stages' bosses, 3 and 4 at the second's; the later ones are not checked)
constexpr int kBossTunes[] = {7, 8, 3, 4};

// --- Player 1's record (found by RAM dumps as a ship idles and is lost, and with a bomb used) ---
constexpr uint32_t kShips = 0x0C5C23C3u;      // the ships left (a byte)
constexpr uint32_t kBombs = 0x0C5C23C6u;      // the bombs left (a byte), and how many can be held in the next
constexpr uint32_t kShipState = 0x0C5C241Cu;  // 1 in play
// The frames the ship is still out of harm for (counted down from about 0xF0 as it comes back)
constexpr uint32_t kSafeFrames = 0x0C5C2544u;

// --- What hits the ship (found 2026-11-11: a ship lost (the ships word written by 0x0C1F3360 from 0x0C20BC60) back to
// the ship's hit handler 0x0C20D750, its caller the collision pass 0x0C1E64D0, run for each player) ---
// The collision kinds of Mushihimesama Futari (games/futari15/src/futari15.h) in a later build: a table of list heads,
// 12 bytes a kind, the first group at +4; a group's next at +0, tested while its 32 bits at +8 are not 0, its first
// member at +28; a member's next at +0, tested while its 32 bits at +8 are not 0, its position the whole parts of the
// 16.16 words at +56 (across) and +60 (up) plus those of the block +64 points at (its owner's), its box reaching +36
// right, +40 left, +44 up and +48 down (32 bits, both ends in it). Across and up from the bottom of the picture stood up.
constexpr uint32_t kKinds = 0x0C5BF230u;
constexpr bool kTestedLong = true;
constexpr bool kAddFixed = false;
// (each group of bullets tested by every member)
constexpr bool kBulletFirstOnly = false;
constexpr uint32_t kPosition = 56, kBox = 36;
// The ship: kind 2 (player 2's 3), its box 2 each way, tested against the bullets and the bodies; kind 14 (15) is tested
// as it is but against every body, not only those marked
constexpr int kShipKind = 2, kShipKindAll = 14;
// The bullets: kind 20, points at their owner's position alone (0x0C1E7F90)
constexpr int kBulletKind = 20;
// The bodies: kinds 0, 1 and 21 (0x0C1E8440), tested while the member's 32 bits at +12 are not 0, and against kind 2 only
// when bit 0 of its byte at +27 is set
constexpr int kBodyKinds[] = {0, 1, 21};
// What the shots hit (found 2026-11-12: the enemies' pass 0x0C1E67C0, run after the ship's, tests each member of the body kinds whose
// 32 bits at +12 are not 0 against the player's shots: kinds 6 and 7 (0x0C1E88D0) and 10 and 11 (0x0C1E8DA0), player 2's
// 8, 9, 12 and 13, unless bit 7 of the member's byte at +27 is set; and kind 4 (player 2's 5)
// (0x0C1E8660), unless bit 1 of it is set. The shots' boxes are read as every other kind's.) A member they can hurt
// that does not hit the ship (bit 0 of +27 clear) is a ground enemy: a tank, a turret
constexpr int kShotKinds[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
constexpr uint8_t kNotShot = 0x80, kNotShotByKind4 = 0x02;

// The ship's box (0x0C20BDD0): 24 each way while it is out of harm (the safe time), else read from a table of each
// ship's reaches (pointers at kShipSizes by the ship's type, its byte at +0xC8 of the ship's block; the reaches right,
// left, up, down as bytes at +12: 2 each way for all three ships), then made smaller in some states (0x0C20C0D0, not
// followed here)
constexpr uint32_t kSafeReach = 24, kShipSizes = 0x0C47715Cu, kShipType = 0xC8, kSizesAt = 12, kSizesMode = 0, kSizesModeAt = 0;

// --- What the board's codes unlock (found 2026-11-25 by entering each after a coin and comparing RAM with a run
// without; each byte set alone puts its part on the select screens) ---
// The Type-B ship (2P holding A, B and C, 1P: D, A, D, C, B, D, up, B, B, left, down, right, C, D, B, A, right,
// right, A, D) and Strong Style (2P holding A, B and D, 1P: Ver 1.5's code), a byte each, 1 when unlocked
constexpr uint32_t kTypeBUnlocked = 0x0C5C0ADCu, kStrongUnlocked = 0x0C5C0ADDu;

} // namespace spelunk::ddpdfk10

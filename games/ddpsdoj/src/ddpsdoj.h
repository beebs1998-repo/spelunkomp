// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi SaiDaiOuJou: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::ddpsdoj {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C1E112Fu;
// The main loop's wait for the next frame (MAME's driver names it as the game's idle loop)
constexpr uint32_t kIdleLoop = 0x0C1D1346u;


// --- A game's beginning (found 2026-11-11 by watching the stage number written as a stage is cleared) ---
// kNewGame sets a game's stage up (the stage in r4; a real game calls it from kNewGameCaller, the attract show from
// elsewhere). kNextStageCall (r4 the game's record, r5 the stage to go to, r6 1) is the game's way on to a stage: a new
// game calls it with 0, a boss beaten (0x0C202020) with the stage after; it keeps the stage at kNextStage in the record
// and, 45 frames on, starts the sequence that plays the stage's start (0x0C28FB20): for stage 0 the ship's launch, for
// any other the tally and the next stage's start (its graphics, then kNewGame with the stage). A stage number put in
// either at a game's start gives a stage without its ship (the launch is stage 0's alone) or without all its graphics. So
// a game begun for a later stage plays the first stage's start, and when its ship is the player's, kNextStageCall is
// called with the stage chosen, as a boss beaten calls it (put in before kEachFrame, the collision pass, which runs each
// frame in play): the first stage's tally is shown and the stage begins as in play
constexpr uint32_t kNewGame = 0x0C1F00A0u;      // (stage in r4)
constexpr uint32_t kNewGameCaller = 0x0C27128Cu;
constexpr uint32_t kNextStageCall = 0x0C1F27F0u;
constexpr uint32_t kEachFrame = 0x0C1E7610u;
constexpr uint32_t kGameRecord = 0x0C4CA60Cu;    // a pointer to the game's record
constexpr uint32_t kNextStage = 0xA4;            // (in the record: the stage to go on to, 32 bits)
constexpr uint32_t kStage = 0x0C5F7DB0u;         // the stage being played, from 0 (32 bits)
// 1 (32 bits) while player 1's ship is on its way in (and before it is made), 0 once it is the player's: found
// 2026-11-12 by RAM dumps every 50 frames across a game's start, the stick moving the ship once it turned 0 (the word at
// +0xE4 of the ship's block, used before, stays 0x100 all through)
constexpr uint32_t kShipComingIn = 0x0C5F99ECu;
// 0 (32 bits) from the ship's making until it has flown in, 1 after (and before it is made): found with kShipComingIn,
// which is 0 for a frame as the ship is made; both together mark the ship the player's
constexpr uint32_t kShipFlown = 0x0C5F99A4u;
// The sound chip's sequences a boss comes on to: 12 its warning, 13 its theme (found by listing the sequences started
// through the first three stages: 2 and 3 the first stage's, 4 and 5 the second's, 6 and 7 the third's, 16 and 17 a
// stage cleared; the later bosses' not checked)
constexpr int kBossTunes[] = {12, 13};

// --- Player 1's record (found by RAM dumps as a ship idles and is lost, and with a bomb used) ---
constexpr uint32_t kShips = 0x0C5F9973u;      // the ships left (a byte)
constexpr uint32_t kBombs = 0x0C5F9976u;      // the bombs left (a byte), and how many can be held in the next
// The frames the ship is still out of harm for (219 as it comes back, counted down)
constexpr uint32_t kSafeFrames = 0x0C5F9B30u;

// --- What hits the ship (found 2026-11-11 from the code's shape: the collision kinds of Dai-Fukkatsu, games/ddpdfk/src/
// ddpdfk.h, as the Dai-Fukkatsu Black Label tests them, games/dfkbl/src/dfkbl.h; the pass 0x0C1E7610) ---
// Each frame the ship is tested against the bullets (0x0C1E9260: each group of bullets by its first member, one far
// from the ship left out for a few frames); the bodies (0x0C1E99A0) and items on the frames the frame counter's low bit
// (0x0C5F7D60) is not the player's
constexpr uint32_t kKinds = 0x0C5F5E0Cu;
constexpr bool kTestedLong = true, kAddFixed = true, kBulletFirstOnly = true;
constexpr uint32_t kPosition = 56, kBox = 36;
constexpr int kShipKind = 2, kShipKindAll = 14, kBulletKind = 20;
constexpr int kBodyKinds[] = {0, 1, 21};
// What the shots hit (found 2026-11-12: the enemies' pass 0x0C1E78F0, run after the ship's, tests each member of the body kinds whose
// 32 bits at +12 are not 0 against the player's shots: kinds 6 and 7 (0x0C1E9EA0) and 10 and 11 (0x0C1EA390), player 2's
// 8, 9, 12 and 13, unless bit 7 of the member's byte at +27 is set; and kind 4 (player 2's 5)
// (0x0C1E9BF0), unless bit 1 of it is set. The shots' boxes are read as every other kind's.) A member they can hurt
// that does not hit the ship (bit 0 of +27 clear) is a ground enemy: a tank, a turret
constexpr int kShotKinds[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
constexpr uint8_t kNotShot = 0x80, kNotShotByKind4 = 0x02;
// The ship's box (0x0C210620): 24 each way while it is out of harm (the safe time), else read from a table of each ship's
// reaches (pointers at kShipSizes by the ship's type, its number in the 32 bits at +0xC4 of the ship's block; the reaches
// right, left, up, down as bytes at +8, or at +12 while the word kSizesMode is 1; 3 each way for every ship seen), then
// made smaller in some states (0x0C21087C, not followed here)
constexpr uint32_t kSafeReach = 24, kShipSizes = 0x0C4D7C24u, kShipType = 0xC7, kSizesAt = 8, kSizesMode = 0x0C5FC86Cu, kSizesModeAt = 12;

} // namespace spelunk::ddpsdoj

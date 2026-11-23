// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mazinger Z (mazinger, the World set of 1994/06/27): what is where in its memory, with how each was found.
// (Found 2026-11-15 by RAM dumps of headless runs, the interpreter's watch on the bytes that changed, and the code
// that reads each. Nothing here has been compared with MAME or a board.)
#pragma once

#include <cstdint>

namespace spelunk::mazinger {

// The credits, a byte of work RAM (found 2026-11-15: three coins, the one byte that went 0, 1, 2, 3; kept at 1, Start
// begins a game)
constexpr uint32_t kCredits = 0x1000A0;

// --- A game's beginning and its stages ---
// The game is one long routine that waits for each frame (0064EA), not a set of tasks. A real game begins at 004496
// (the stage set to 0, the loop cleared, the robots chosen on a screen of their own: 0100CC) and comes here with the
// robots chosen, to fade out and set the stage up. Only a new game comes here: the attract show's demonstration sets
// its own stage up (00416C), a player joining or continuing does so inside the stage, and the stages after the first
// begin at 005078. (Just before it the program has a stage select of its own, 00454E, which this set never shows: the
// word at 03FFFE is not 0. It puts its choice in kStage and kLoop and comes here too, which is what the stage start
// does.)
constexpr uint32_t kNewGame = 0x004624;
// The first stage set up after that fade: its map and script are loaded from kStage (040000, called at 0046B6), and
// here that is done (so kScript is this stage's, not what the game before left)
constexpr uint32_t kBeginStage = 0x0046BC;
// The stage being played, from 0 (a word: 0 to 7; a stage's end adds one, 004FBC, and past the eighth the second loop
// begins at 0 again, or after the second loop the ending: 004FC2). Found: a run with the safe time held, the word that
// went 0, 1, 2, 3, 4 as the results screens came
constexpr uint32_t kStage = 0x100A24;
// The loop (a byte: 0 the first time round, 1 the second: 005070)
constexpr uint32_t kLoop = 0x104EBE;
constexpr int kStages = 8;
// The game's flags (a byte): bit 1 while player 1 is in the game and bit 2 player 2, bit 3 while a stage is set up, bit
// 4 from a stage's boss beaten to the next stage (no bomb then, 00A4DA; no hit, 0108AE), bit 6 through the attract
// show's demonstration
constexpr uint32_t kFlags = 0x100A22;
// The frames of the stage so far (a word, one more each frame the game runs: 00476E). Its low bit says whose turn the
// hit test is
constexpr uint32_t kStageFrames = 0x100A26;

// --- When a stage's boss is on ---
// A stage is a script (040722 runs it: from the pointer at kScript, each entry a place in the stage's scroll and what
// to bring on there). The boss is the script's last entries: there is no word that says a boss is on (a search of
// the RAM of three stages played found none), so the boss is taken to be on once the script has passed the place
// below for the stage: the entry that starts the boss's tune (command 0x0C with tune 0x0D, or 0x0E for the last
// stage's: 040936) where the script has one, or the entry that brings the boss on
constexpr uint32_t kScript = 0x104E2E;

// --- Each player's record (0x30 bytes; player 2's follows player 1's) ---
constexpr uint32_t kPlayer1 = 0x100A90, kPlayer2 = 0x100AC0;
// A record's first word is what the robot is doing: 7 not in the game, 8 choosing a robot, 1 flown, 2 through its
// special attack (button 2: 00A570), 0x0F and up while a hit is dropped (01088C): 0x0F for the 0x68 frames after a
// hit (flown still: 00AB94), 0x12 coming in, 0x15 flying off at a stage's end, 0x18 blowing up
constexpr int kStateOut = 7, kStateChoosing = 8, kStateNotHit = 0x0F;
// Where it is (words at +6 and +0xA: whole pixels, +6 across the board's 240 and +0xA along its 384, as the sprites
// are placed: 037B4E)
constexpr int kX = 6, kY = 0x0A;
// The frames the robot is still out of harm for (a byte at +0x24: 0xF0 as it comes in, 00AD68, and 0x78 after a hit,
// 0108FA, counted down a frame at a time, 00AF60; while it is not 0 a hit is dropped: 0108A8). Found: the bytes of the
// record watched through a game's first seconds
constexpr uint32_t kSafeFrames = kPlayer1 + 0x24;
// The bombs left (a byte at +0x26: 3 for a new game, 0107BA; one more for each picked up, to 9, 03274A; a bomb used
// takes one and sets bit 7 until it is over, 00A4F4 and 00AAA0). The bomb is button 3; button 2 is the robot's special
// attack, which costs nothing. Found: the bomb button pressed with the byte watched
constexpr uint32_t kBombs = kPlayer1 + 0x26;
// The energy (a byte at +0x2B): the bars left in its low four bits, a full set in its high four (0x33 by the board's
// settings as they come; 0x55, 0x44, 0x22 and 0x11 by the others: the table at 010802). A hit takes one bar (0108D4)
// and the robot is lost when none is left: there are no ships besides. Found: the byte that went 0x33, 0x32, 0x31, 0x30
// as the bars went
constexpr uint32_t kEnergy = kPlayer1 + 0x2B;
constexpr uint32_t kEnergyBars = 0x0F;

// --- What hits what (the game's own test, 00608E, run once each frame the game runs) ---
// Everything but the robots is one table of 0x11F records of 0x30 bytes. A record is in use while its first word is
// not 0, and takes part in the test unless bit 15 of its flags (the word at +0x12) is set. The test sets that bit on
// what was hit, so the boxes are read as the test is entered.
constexpr uint32_t kHitTest = 0x00608E;
constexpr uint32_t kObjects = 0x100AF0;
constexpr int kObjectSize = 0x30, kObjectCount = 0x11F, kObjectFlags = 0x12;
// A record's box: its position (kX, kY) less and plus the byte at +0x2E across, and the byte at +0x2F along. Two boxes
// meet when their centres are less than the sum of those apart (one pixel more to one side: 00614C)
constexpr int kHalfAcross = 0x2E, kHalfAlong = 0x2F;
// The flags' low byte. Bit 5 clear: an enemy, or a bullet. One with bit 0 hits a robot it touches (0061E6: player 1's
// robot on odd frames, player 2's on even ones), and is hurt by it; one with bit 1 is hurt by the player's shots
// (00613E), one with bit 2 by the attacks that also clear bullets (00618A). A bullet is one that hits and that the
// shots pass through (bit 0 without bit 1); an enemy the shots hurt that does not hit is on the ground. With 64 records
// or more in use the shots are tested against every other enemy each frame (00644A, 00649A).
// Bit 5 set: the players' own. One without bit 4 is a shot (the first kind) or, with bit 7, an attack of the second
// kind; one with bit 4 and bit 0 is a thing to pick up, which the same test hands to the robot that touches it
constexpr uint16_t kHitsRobot = 0x01, kShotsHurt = 0x02, kAttacksHurt = 0x04, kNoShot = 0x10, kPlayers = 0x20, kNotTested = 0x8000;
// A robot's box for that test: 4 pixels each side of its position across, 7 along (the same for every robot: 006200).
// The robot is tested where it was the frame before (the robots move after the test: 004786), the enemies where they
// are; the boxes are drawn where each is shown
constexpr int kRobotHalfAcross = 4, kRobotHalfAlong = 7;

} // namespace spelunk::mazinger

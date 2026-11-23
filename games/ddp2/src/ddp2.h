// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi II: Bee Storm (ddp2, World ver. 102 of 2001-09-10): what is where in its memory, with how each was found.
// (IGS's game, not Cave's: none of Dai-Ou-Jou's shapes are here. Its program is C compiled for the 68000. The
// cartridge's ARM, machine/pgm.h and docs/PGM.md, moves the enemies' bullets, whose records are in the RAM the two
// share at D00000; the ships, the enemies, the shots and every hit test are the 68000's, in its work RAM.)
// (Found 2026-11-15 by RAM dumps of headless runs, work RAM and the ARM's with --dump-arm: three coins, three ships
// lost, a bomb used, 40,000 frames played out of harm; then the code that reads and writes each, watched with the
// interpreter, SPELUNK_WATCH.)
#pragma once

#include <cstdint>

namespace spelunk::ddp2 {

// The credits, a byte of work RAM (three coins, the bytes that went 0, 1, 2, 3; of the six that did, the one the
// coin's code adds to at 14A0CA and Start's takes from at 14A2C0. The others are the BIOS's counts in the battery
// RAM.) The byte after it counts coins towards a credit (14A0B0), against the setting at 803800.
constexpr uint32_t kCredits = 0x80EED6;
// A coin's code (14A09A) also puts a message in the game's queue (14A0FC on), and only that takes the demonstration
// and the title to the "push start" screen, where Start is looked at (a press before that screen is up is lost): so
// free play here keeps a credit in by the board's own coin switch (game.cpp). (The game's own free play is bit 3 of
// the settings byte 80385F in the battery RAM: with it set the coin's code and Start's leave the credits alone,
// 14A09A and 14A2AA, and the title says FREE PLAY. Not used: it is the board's setting, kept with the player's
// others.) The service switch's credit: 14A19A.

// --- A game's beginning, and its stages ---
// A real game is set up by a run of calls from 14BBE2 on (the attract show's demonstration sets its stage another
// way, 1690BC); the one at 14BC00 (168F74) puts FFFF in the two stage words, and here, just after it, they are still
// so. Each stage, the first too, is then begun by 14BDCA: 16909C adds one to the place in the order (kStageOrder)
// and looks the stage up for it (16EEC6: Practice's four, the table 106F50 = 0, 1, 3, 4; Combat's and Internet
// Rank's seven, 106F56 = 0, 1, 3, 4, 5, 6, 7), 169102 loads it. So a game begins at another stage with its place
// less one put in kStageOrder at kNewGame. (168F86 is a stage select of the developers', not reached.)
constexpr uint32_t kNewGame = 0x14BC06;
constexpr uint32_t kBeginStage = 0x14BDCA;
constexpr uint32_t kStageOrder = 0x815622; // the stage's place in the mode's order, from 0 (a word; FFFF before a game)
constexpr uint32_t kStage = 0x815624;      // the stage itself (a word: the tables above)
constexpr uint32_t kMode = 0x815A98;       // 0: Practice (four stages); not 0: all seven (a word, set at the mode select)
constexpr int kPracticeStages = 4;

// A stage's boss arrives with its warning (the boss's name across the picture), which the routine at 16AD8C begins,
// once for each boss. (The words it works with, 81598A to 815990, are the stage's title card's and the results' too:
// 81598E is the step of whichever is on, 815990 its count. No word says only "a boss is on".) The trip to the boss
// ends there (game.cpp).
constexpr uint32_t kBossWarning = 0x16AD8C;

// --- The players ---
// A player's count (20 bytes: player 1's at 815A6E, player 2's at 815A82; set up at 16F038): +0 in a game, +6 the
// ships besides the one flown (from the setting 803808; 16F2A0 takes one, and the game is over when there is none to
// take; never more than the setting, or the game stops with an assertion), +7 the ships lost
constexpr uint32_t kShips = 0x815A74;
// The bombs (a byte; 16D4E0 sets it to 5 at a game's start, 16D5B8 takes one), the byte after it the count shown,
// which follows it. A ship of the Energy kind (kWeaponKind 2, not 1) has its gauge in the same byte: 0x40 is full
constexpr uint32_t kBombs = 0x8159A6;
constexpr uint32_t kWeaponKind = 0x8159A4;
constexpr int kBombsFull = 5, kEnergyFull = 0x40;

// A ship's record (0x192 bytes: player 1's at 81253A, player 2's at 8126CC)
constexpr uint32_t kShip1 = 0x81253A, kShip2 = 0x8126CC;
constexpr int kShipState = 0;   // (a word: 4 while it is flown, and only then is it tested)
constexpr int kShipX = 0x2C, kShipY = 0x2E; // the place, in the picture's pixels (x along the board's 448)
// The count of its time out of harm (a word; a new ship's is set from +CE at 150BC8, and 1511E6 takes 2 a frame while
// it draws the shield): every hit test leaves the ship alone while it is not 0 (14EE26, 14FEA4, 1500AA, 15024A).
// Held at 0x100 nothing was lost in 40,000 frames
constexpr uint32_t kSafeFrames = kShip1 + 0xC6;

// --- The hit tests (all the 68000's: 15028E, a player a frame in turn, kTestedPlayer) ---
// The ship's box is 4 pixels either side of its place along x and 3 along y, the edges in it. The enemies' bullets
// are points (14EBD2; 14F3A2 while the bee storm is out, which also clears those inside its boxes): records of 16
// bytes at D00000 + 1500 in the shared RAM, 128 of them, +0 the kind (2: a bullet; 4 and more: things to pick up),
// +6 x, +8 y. The ARM moves them.
constexpr uint32_t kBullets = 0x1500;
constexpr int kBulletSlots = 128, kBulletSize = 16;
// The enemies: 32 records of 0x192 bytes from 80EFE2 (+0x162 not 0: there), each with up to +B8 boxes of 0x14 bytes
// from +BA: +0 flags (bit 0: the shots hurt it, bit 1: it hits the ship, bit 2: it stops the laser), +4 +6 the top
// left, +8 +A the bottom right (in it), +C its strength (a long: gone at 0 or less)
constexpr uint32_t kEnemies = 0x80EFE2;
constexpr int kEnemySlots = 32, kEnemySize = 0x192, kEnemyThere = 0x162, kEnemyBoxes = 0xB8, kEnemyBox = 0xBA, kBoxSize = 0x14;
// The shots (150104): 32 records of 0x38 bytes a player from 81287A (player 2's from 812F7A), the box at +14 +16
// (top left) and +18 +1A (bottom right, in it); a shot that is spent has its right edge at -2048
constexpr uint32_t kShots1 = 0x81287A, kShots2 = 0x812F7A;
constexpr int kShotSlots = 32, kShotSize = 0x38;
// The laser (14FC42, in place of the shots while its record's first byte is 4 or more): player 1's record at
// 813746, player 2's at 81391E. +8 and +C point at the place it leaves the ship from; +16 and +18 the aura's reach
// behind that and to each side; +1CC, +1CE the beam's start from it, +3 its length in 32s, +14 its width, and with
// +2 at 0 a head of 32 more
constexpr uint32_t kLaser1 = 0x813746, kLaser2 = 0x81391E;
// The bee storm (14FEFC, in place of both while the word at 813DAA is 1): its record at 813DA6, shaped as the
// laser's (+E, +12 the pointers; +18, +1A the aura; +2E2 the start, +6 the length in 32s, +2A less +16 the side's
// start and twice +16 the width)
constexpr uint32_t kBeeStorm = 0x813DA6, kBeeStormOn = 0x813DAA;

// --- Other places ---
// A command for the ARM: 145918 (the command's number on the stack; D00010 and the latch at D10001, then the loop
// waiting for the answer at 145954). Five a frame in play.
// The 68000's own wait for the frame: the routine at 149DC0, which does nothing (the branch at 149DCE) while the count
// of frames at 80ED38 is the one it last saw (kept at 80EE54). The headless runner's --spin 149DCE counts its rounds.
// D01F12 (shared RAM): a count of frames in which no enemy makes a bullet (180 after a ship is lost: 150C08).

} // namespace spelunk::ddp2

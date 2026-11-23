// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Ou-Jou (ddpdoj, the Japanese set of 2002.04.05): what is where in its memory, with how each was found.
// (Found 2026-11-11 by RAM dumps of headless runs: a bomb used, three ships lost, a run with the safe time held through
// two stages; then the code that reads and writes each, watched with the interpreter.)
#pragma once

#include <cstdint>

namespace spelunk::ddpdoj {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the byte that went 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x80394A;

// --- A game's beginning ---
// The game runs as tasks: records of 0x50 bytes from 80E1B4 (new ones made by 14110A, kind in the first word's low
// byte, and run from the table at 140EEA). A real game is begun at 15B882 by making the game's task (kind 0x0A, its
// routine 15F2FC) and putting its stage in the task's word at +4 (15B88C: always 0); the task's first run (15F136)
// gives that word to the routine that sets the stage (15E88E, at 15F236), and sets the stage, the ships and the first
// stage's launch up from it. Here, just after the 0 is put there, a0 is the task. (The attract show's demonstration
// does not come here.)
constexpr uint32_t kNewGame = 0x15B892;
constexpr int kTaskStage = 4;
// The routine that sets the stage being played (d0) and the words made from it: at a game's beginning (15F236) and at
// each stage's start after a boss (the stage's end task, kind 6, at 18B9CA)
constexpr uint32_t kSetStage = 0x15E88E;
constexpr uint32_t kStage = 0x812FF6; // the stage, from 0 (a word: 0 to 4 in a loop)

// While a stage's boss is on (a word: 0 through the stage and its mid-bosses; set from the boss's warning to the
// stage's end, bits 3 and 4 of its high byte set as the boss is beaten at 1428E6; cleared by the stage's end at 18BA50)
constexpr uint32_t kBossOn = 0x813058;

// --- Player 1's record ---
// The ships left besides the one flown (the low byte of the word at 813022: 2 for a new game, a ship lost takes one,
// and a ship lost at 0 ends the game: 14931A; 0xFF while no game is played)
constexpr uint32_t kShips = 0x813023;
// The bombs left, and what they are filled to at a new ship (bytes: 3 and 3 for a new game with this ship; the stock
// grows by one with each ship lost)
constexpr uint32_t kBombs = 0x81037E;
constexpr uint32_t kBombStock = 0x81037F;
// The ship's record (0x62 bytes; player 2's at 8103BC): flown while its first word is negative (the game's hit test,
// 1440A2, is run only then). The word's bit 12 (the byte's bit 4) is set by the hit test when something hits it; the
// ship's routine (148826) clears it while the byte at +0x3E is not 0, and goes to the ship's loss when it finds it set
// with that byte at 0. That byte is the frames left out of harm: 0xF0 as a ship comes back (1486F4), counted down a
// frame at a time (148838; 0xFF is held, not counted)
constexpr uint32_t kShip1 = 0x81035A, kShip2 = 0x8103BC;
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;

// --- What hits the ship (the game's own test: 1440A2 for a ship at a4, from 189A08 on) ---
// Positions are in 64ths of the board's pixels (across its 448, down its 224; the monitor is on its side). In a
// record (the ship's, an enemy's): the position (words at +2 across, +4 down) and the box about it: from the position
// less the word at +0x12 to the position plus +0x10 across, less +0x16 to plus +0x14 down (both ends in it). The
// ship's: 0x100, 0xC0, 0xC0, 0xC0 (about 7 x 6 pixels)
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (144CFC): records of 0x40 bytes from here, each a point (+2, +4) that hits when it is inside the ship's
// box and its first byte has none of the bits 0x51. 70 of them are tested, more while the words at 81B1B8, 81B1BA,
// 81B1BC and 81B1BE are set in turn (the bosses' patterns): 110, 160, 190, 210
constexpr uint32_t kBullets = 0x817D30;
constexpr int kBulletSize = 0x40;
constexpr uint32_t kMoreBullets = 0x81B1B8;
constexpr int kBulletCounts[5] = {70, 110, 160, 190, 210};
// The enemies whose bodies hit the ship (14419C): records of 0x20 bytes from here, the first word negative while one
// is there; as many are tested as the words at 815DFA and 815DFC add up to (the first that are there). One hits when
// its first word has bit 0 and the boxes meet (both moved by 0x2800 there: an enemy whose left edge, so moved, is
// past 0xF800 is left out)
constexpr uint32_t kEnemies = 0x8144F8;
constexpr int kEnemySize = 0x20;
constexpr uint32_t kEnemyCount1 = 0x815DFA, kEnemyCount2 = 0x815DFC;

// --- What the player's shots hurt (the same routine, after the ship's test: 144220 on; found 2026-11-12) ---
// The enemies' records are in two groups: 100 from kEnemies (as many there as the word at 815DFA says), then 50 from
// here (the word at 815DFC): the ground's, the tanks and turrets the ship flies over. A shot hurts an enemy whose first
// word has bit 13 (0x2000) and whose box (as above) meets the shot's, its left edge on the screen (moved by 0x2800 and
// below 0x9700 in the first group, by 0x1800 and below 0x8800 in the ground's). Those without bit 0 never hit the ship:
// drawn as the ground's kind
constexpr uint32_t kGroundEnemies = 0x815178;
constexpr int kAirSlots = 100, kGroundSlots = 50;
constexpr int kShootable = 0x2000;
constexpr int kAirMoved = 0x2800, kAirOnScreen = 0x9700, kGroundMoved = 0x1800, kGroundOnScreen = 0x8800;
// The shots (a0 at 189A08): 36 records of 0x30 bytes for each player, one there while its first word is negative. Its
// box as the test takes it: across from the position (+2) plus the word at +0x10 less +0x12, to the position plus
// +0x10; down from the position (+4) plus +0x14 less twice +0x16, to the position plus +0x14 (both ends in it). It does
// nothing while its power (+0x18) is negative
constexpr uint32_t kShots1 = 0x8104E6, kShots2 = 0x810BA6;
constexpr int kShotSlots = 36, kShotSize = 0x30;
constexpr int kShotRight = 0x10, kShotWidth = 0x12, kShotDown = 0x14, kShotHalfHeight = 0x16, kShotPower = 0x18;
// While the shot button is held (the laser: the ship's byte at +0x3F not 0, and its low byte's bit 7 clear), three
// more records hurt enemies (1444CA on): the laser's beam (a1 at 189A08; 1446EC, while its first word is negative and
// its bit 9 set): across from its position to the position plus 0x400 plus the word at +6, but cut at the first enemy
// it meets (+0x10, where the test leaves it), down from the position less +0xA to the position plus +8; the laser's
// aura about the ship (a3, set with the beam by 153E28), with an enemy's box; and one more with an enemy's box (a2,
// tested while there and its position below 0x7000; it hurts the bodies too; not seen there in the runs)
constexpr uint32_t kLaserOther1 = 0x811776, kLaserOther2 = 0x811D76;
constexpr uint32_t kLaserAura1 = 0x811806, kLaserAura2 = 0x811E06;
constexpr uint32_t kLaser1 = 0x811E66, kLaser2 = 0x811E86;
constexpr int kShipLaserOn = 0x3F;
constexpr int kLaserLength = 6, kLaserDown = 8, kLaserUp = 0xA, kLaserEnd = 0x10;

// A word negative while player 1's ship is flown (its stage is set before the ship is chosen: Begin at the stage
// presses the shot on until it is)
constexpr uint32_t kShipFlown = 0x81035A;
} // namespace spelunk::ddpdoj

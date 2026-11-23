// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi (ddonpach, the World set of 1997 2/5): what is where in its memory, with how each was found.
// (Found 2026-11-10 by RAM dumps of headless runs: a bomb used, ships lost, and a run with the safe time held to
// play through three stages; then the code that reads each word.)
#pragma once

#include <cstdint>

namespace spelunk::ddonpach {

// The credits, a byte of work RAM (found: three coins, the bytes that went 0, 1, 2, 3; of the three this is the one
// that, kept at 1, lets Start begin a game)
constexpr uint32_t kCredits = 0x1013AB;

// --- A game's beginning ---
// The start of a game for player d7 (only a real game comes here: the attract show's demonstration sets its stage
// up from a table of its own at 004690)
constexpr uint32_t kNewGame = 0x00456E;
// What the game is doing (a word): 0x0B as a game begins, 7 while one is played, 8 while a continue is offered
constexpr uint32_t kMode = 0x1017A6;
constexpr uint32_t kModePlay = 7, kModeContinue = 8;
// The first state of the main loop's task that begins a stage: from here it reads the stage (kStage) to find the
// stage's map (002D04 on)
constexpr uint32_t kBeginStage = 0x002C84;
// The main loop's tasks: 0x3D records of 0x40 bytes from 100002, task n running the routine at 00043A + 8n (000AA2);
// a record's first word is 0x8000 + n while the task is on. Task 0x3A is the stage's end (000CA2), switched on when a
// boss is beaten: the word at +0x3C counts down to the next stage (set to 4 at 001392); at 0 the game goes on (0013C2:
// the stage's objects cleared, kStage one more, past the sixth the next loop) and the next stage begins at kBeginStage.
// Switched on with its first state done (+2 bit 0) and that count at 1, it goes straight on the next frame
constexpr uint32_t kTasks = 0x100002;
constexpr uint32_t kStageEndTask = kTasks + 0x3A * 0x40;
constexpr int kStageEndCountdown = 0x3C;
// The boss's state (a byte): bit 0 while it is on, bits 1 and 4 set as it is beaten (the stage's end waits for bit 1
// at 00107C before going on); the stage's end sets bits 3 and 2 itself
constexpr uint32_t kBossState = 0x10E859;
constexpr uint8_t kBossBeaten = 0x13;
// Not 0 from a new game's start (set to 1 at 00503A) until the first stage's carrier has launched the ship (cleared at
// 04A93C): until then the ship waits (00659E). The stage's own number put in kStage as a game begins gives that
// stage's map but no carrier, so the ship never comes and the bombs' row is not drawn: a later stage is reached
// through the game's own way on instead (runtime/shmup.h)
constexpr uint32_t kShipWaits = 0x10E856;
constexpr uint32_t kSection = 0x101974;  // the part of the stage being played (a word; 0 as a stage begins)
constexpr uint32_t kStage = 0x101978;    // the stage being played, from 0 (a word: 0 to 5)
constexpr uint32_t kLoop = 0x10197A;     // the loop (a word: 0 the first time round, 1 the second)
// While a stage's boss is on (a word: 1, from its warning to its end; 0 through the stage and its mid-bosses)
constexpr uint32_t kBossOn = 0x10E87A;

// --- Player 1's record ---
constexpr uint32_t kShips = 0x101965;     // the ships, the one flown among them (the low byte of a word: 3 for a new
                                          // game; a ship lost takes one, and at 0 the game ends: 0012DE)
constexpr uint32_t kScore = 0x10161E;     // the score, tens of points (8 BCD digits: the screen adds the last 0)
constexpr uint32_t kHighScore = 0x101624; // (the same way)
constexpr uint32_t kBombs = 0x102CB0;     // the bombs left (a byte)
constexpr uint32_t kBombStock = 0x102CB1; // what the bombs are filled to at a new ship (a byte: 3 for a new game,
                                          // one more for each ship lost, up to 6)
// The frames the ship is still out of harm for after it appears (a byte: 0xEF as it comes back, counted down a
// frame at a time; held up, nothing hits it)
constexpr uint32_t kSafeFrames = 0x102CCA;

// --- What hits the ship (the game's own test, 0057EE, run for each ship: a4 its record) ---
// Positions are in 64ths of the board's pixels (the sprites are placed at a position over 64: 05A7DC), across the
// board's 320 and down its 240 (the monitor's sideways, the picture is turned to stand up).
constexpr uint32_t kShip1 = 0x102C8C, kShip2 = 0x102CCC; // each ship's record (0x40 bytes): flown while its first word is negative
// In a record (a ship's, an enemy's): the position (words at +6 across, +8 down) and the box about it: from the
// position less the word at +0x12 to the position plus the word at +0x10 across, less +0x16 to plus +0x14 down (both
// ends in it). A ship's: 0x100, 0xC0, 0xC0, 0x40, about 7 x 4 pixels
constexpr int kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets: records of 0x40 bytes from here, 225 of them (245 in the second loop: 0057EE tests five a round, 45
// rounds or 49), each a point: it hits a ship when it is inside the ship's box and its first byte has none of the
// bits 0x51 (0x80: it is there)
constexpr uint32_t kBullets = 0x106CB6;
constexpr int kBulletSize = 0x40, kBulletCount = 225, kBulletCountLoop2 = 245;
// The enemies whose bodies hit the ship: records of 0x20 bytes from here, 114 of them, each with a box; one hits
// when its first byte has 0x80 and 0x40 and the boxes meet (005964 on; both boxes moved by 0x2800 there, and an
// enemy that far right in the moved frame, past 0x7800, is left out)
constexpr uint32_t kEnemies = 0x104AF6;
constexpr int kEnemySize = 0x20, kEnemyCount = 114;

// The other objects' records (048A3C: those whose task's byte at +0xD has bits 7 and 2), 0x49 of 0x20 bytes
constexpr uint32_t kEnemies2 = 0x10D696;
constexpr int kEnemy2Count = 0x49;

// --- What the player's shots hit (the same routine, 0059E8 on, the players taking alternate frames: 000B8C) ---
// The enemies the shots can hurt: those records whose first byte has 0x80 and 0x20 (the shots test the first 80 for
// 0x21, the rest for 0x20: 005A04, 005AE6; the laser 0x21, its head 0x20). One without 0x40 never hits the ship: a tank,
// a turret, a thing on the ground (found: such records sit under the tanks of stage 1)
constexpr uint8_t kShootable = 0x20;
// Each ship's shots (a0 at 000B96 / 000BC2): 36 records of 0x28 bytes, each with a box as an enemy's; one is tested
// while its first word is negative and has not 0x2000 (or 0x1000, its hit this frame) and its power (+0x18) is not
// negative
constexpr uint32_t kShots1 = 0x102D8E, kShots2 = 0x10332E;
constexpr int kShotSize = 0x28, kShotCount = 36;
// The laser (a1 there), tested while its first word is negative and bit 1 of its first byte set: from the across word
// at +2 to where it stops (+0x10: the left edge of the first enemy it meets, or 0x5400), up the word at +0xA and down
// +8 from the word at +4 (005D18)
constexpr uint32_t kLaser1 = 0x103F0E, kLaser2 = 0x103F2E;
// The laser's two other parts (a2, a3 = a2 + 0x60): boxes as an enemy's, tested while their first word is negative
// (005BBC, 005C70). The laser and these are tested only while the ship's second byte has not bit 7 and its byte at
// +0x3F is not 0 (the laser held: 005BAE)
constexpr uint32_t kLaserParts1 = 0x103B4E, kLaserParts2 = 0x103E6E;

// --- The stage's objects ---
// Task 4 (048992) brings the stage's objects in: it moves the 36 things at 106836 (04DD0A), then (from here) reads the
// stage's table (004895E: a table a stage, the same for both loops) for the entries whose first word is the stage's
// count 10198E, and starts a task for each (058166: d2 and d3 the entry's two longs, the object's kind the top byte of d3)
constexpr uint32_t kStageObjects = 0x048998;


// --- Bullets made (what Dodge training fires with: dodge.cpp) ---
// The game's routines that make an enemy's bullet: d0 its kind (the low 6 bits: one of 40, a table of 16 bytes each at
// 05104E) with, in the high word, what is added to the kind's own speed; d1 its angle; d2 its place (across in the high
// word); d3 its parent's record or 0 (the bullet is put at the parent's place plus d4, and some kinds stay there for a
// while: 05193C, 05221C). Each finds a free record among kBullets' first 95 or 205 (150 or 245 in the second loop) and
// makes nothing while a bomb is out or the stage is over (1017BE, 101764, 10E878, 10C976). All registers are kept.
// The first takes one of 64 directions, the second one of 256 (and sets bit 19 of the bullet's first long, which says so);
// the second loop has its own two, as its bullets are moved by a routine of their own (0552EA for 051390).
// (What enemies call are 050DDC and 050F94, which in the second loop add to the speed and make two or three bullets
// through these.)
constexpr uint32_t kMakeBullet = 0x050DE4, kMakeBulletFine = 0x050F9C;
constexpr uint32_t kMakeBulletLoop2 = 0x054E22, kMakeBulletFineLoop2 = 0x054EF4;
// The game's aims, from the place in d0 (across) and d1 (down) at the place in d2 and d3, the answer in d1: one of 64
// directions (a table of the eighths between the two distances, 059AE8), and one of 256 (059CFE). 0 is up the screen
// (the board's across, growing), a quarter turn is to the screen's right
constexpr uint32_t kAimCoarse = 0x059A6A, kAimFine = 0x059C8C;

// --- The frame's sprites ---
// Where the next sprite of the frame's list goes, in the sprite chip's memory (a long: set to the list's start at
// 0006D4 each frame). An entry is 16 bytes: the look (the palette in the high byte, priority 0x30, 8 turned top to
// bottom on the screen, 4 left to right), the sprite's number, its left top corner across and down the board in
// pixels, and its size in 16s of pixels (across in the high byte)
constexpr uint32_t kSpriteList = 0x101216;
constexpr uint16_t kSpriteTurned = 0x0008;
// The palette (the board's, at C00000): a sprite's colour (the look's high byte, 64 of them) has its 16 entries from
// 512 bytes times the colour
constexpr uint32_t kPalette = 0xC00000, kSpriteColourBytes = 512;
// The carrier that launches a game's first ship (found in the list as stage 1 begins): one sprite, 5 x 5
constexpr uint16_t kCarrierLook = 0x2830, kCarrierSprite = 0x680B;

} // namespace spelunk::ddonpach

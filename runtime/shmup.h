// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The training features every shooting game gets from a few facts about it (its names header): a game begun at any
// stage, straight to a stage's boss, the ship out of harm, bombs and ships that do not run out. Each is a feature, off
// unless chosen; with all off nothing here touches the game. The tools that need no facts (the practice point,
// rewind, fast forward) are the port's own (runtime/timeline.h, runtime/moments.h, port/main.cpp).
//
// For both hardwares: a CV1000 game's stage start is its routines' stage argument (a register, the SH-3's native
// calls); a 68000 board game's is a word of RAM set as its stage-begin routine is entered (a hook: the places are
// named in the game's names header, so the translator stops there). What must survive run-ahead and rewind on the
// 68000 boards is kept in the board's featureWords.
#pragma once

#include "../machine/board.h"
#include "game.h"

#include <string>
#include <vector>

namespace spelunk {
class M68K; // (a 68000 board's CPU: Facts::newGameStage)
}

namespace spelunk::shmup {

// A place in the program the shared code hooks (a 68000 game's names header names its places with this, so that
// tools/m68k/recomp.py sees them: "HookAt(kName)" where "kName = 0x..." is in the same sources)
constexpr uint32_t HookAt(uint32_t address) {
    return address;
}

struct Facts {
    // The stages' names, in order
    std::vector<std::string> stages;
    // Free play (a 68000 board's: the credit at Start, runtime/arcade.h): 0, or Start is pressed again for the player
    // this many frames apart until the game has taken the credit (a game that answers the first with its title)
    uint32_t startAgain = 0;

    // --- How a game is begun at another stage (CV1000) ---
    // The game's routines that begin a new game take the stage (0 for the first) in a register: each one's address
    // and the register. A game begun at another stage has that number put there instead
    // The first is the routine that sets a new game up, with where a real game calls it from (the return address:
    // the attract show's demonstration calls it from elsewhere, and is left alone)
    struct StageArgument {
        uint32_t address;
        int reg;
        uint32_t caller = 0;
    };
    std::vector<StageArgument> stageArguments;
    // Or (CV1000: Ibara's family, whose stages are begun by a script of the game's own) the routine that begins each
    // stage, hooked, and a function called as it is entered with the stage chosen (0 for the first): where this is a
    // real game's first stage beginning, it sends the game's script on to the stage chosen and returns true (a trip to
    // the boss starts there); anywhere else it leaves the game alone and returns false
    uint32_t scriptStage = 0;
    bool (*toStage)(Board &board, int stage) = nullptr;

    // --- How a game is begun at another stage (68000 boards) ---
    // A game is begun as ever, and when its first ship is the player's it is sent on by the game's own way to the next
    // stage, from the stage before the one chosen: so the stage begins as it does in play (the ship flying, the stage
    // set up in full), whatever its first stage does to bring the ship in.
    // newGame: the routine a real game (not the attract show's) begins with. stageWord: the stage (a word, 0 for the
    // first); loopWord: the loop (a word, 0 the first time round; none if 0); stages past `stagesPerLoop` are the next
    // loop's. modeWord: what the game is doing, with its values while a game is played or waits for a continue (the
    // new-game routine entered then is a player joining or continuing). readyWord: a word that is not 0 from a new
    // game's start until its first ship is the player's. nextStage: sets the game on its own way to the next stage (as
    // when a stage's boss is beaten). beginStage: the routine (or task) that begins a stage, where a trip to the boss
    // starts from
    uint32_t newGame = 0, stageWord = 0, loopWord = 0, modeWord = 0, readyWord = 0, beginStage = 0;
    int stagesPerLoop = 0;
    void (*nextStage)(Board &board) = nullptr;
    // Or, where the new game's routine itself takes the stage it begins at (the PGM games: a task's word), what puts
    // the stage chosen there, called at newGame (the CPU as it is there; 0 the first stage): no readyWord or
    // nextStage then, and the stage begins as the game would begin it
    void (*newGameStage)(M68K &cpu, Board &board, int stage) = nullptr;
    // (with newGameStage) the stages (bit n: stage n, from 0) that the game itself only ever begins from the stage
    // before's end, so that begun from a new game they never start (Ketsui's fourth: its scroll stands still and the
    // picture stays black). For those, sendOn is called before each frame from the new game on until it has set the
    // game on its own way into the stage (true): the stage's beginning that follows is where a trip to the boss starts
    uint32_t sendOnStages = 0;
    bool (*sendOn)(Board &board, int stage) = nullptr;
    std::vector<uint32_t> playModes;

    // Player 1's record, in main RAM (0: not known, and the feature is not offered). The ships and bombs left, a
    // byte each
    uint32_t ships = 0, bombs = 0;
    int bombsHeld = 3;
    // The fewest ships kept: 1 where the count is of the ships besides the one flown, 2 where it counts that one too
    // (the game ends when a ship lost takes it to 0)
    int shipsKept = 1;
    // The ships' count's size in bytes (1, 2 or 4: Deathsmiles keeps its life as a 16-bit word), and so shipsKept's
    int shipsBytes = 1;
    // A second count raised with the first, by as much (0: none; its size is shipsBytes too): Guwange's lives are a
    // ceiling (ships) and the life left under it, which its hits take from a part at a time; a ship lost takes a
    // whole ship from both
    uint32_t shipsAlso = 0;
    // The bits of the count that are the ships (0: all of it): Mazinger Z keeps its energy's bars in the low four bits
    // of a byte whose high four are a full set
    uint32_t shipsMask = 0;
    // (the feature's name and description, for a game whose count is not of ships: a title of 22 characters at most)
    std::string shipsTitle = "Ships never run out", shipsWords = "A ship lost is not counted: the game never ends for want of one.";
    // A byte the bombs may go up to (DoDonPachi's stock grows with each ship lost): the bombs are kept at it when known
    uint32_t bombsMax = 0;
    // A byte whose bits `bombsTallyBits` are set while the game counts the bombs left itself (a stage's end that turns
    // each bomb left into points, one at a time: DonPachi's): the bombs are not filled again then (0: none)
    uint32_t bombsTally = 0;
    uint8_t bombsTallyBits = 0;
    // What stands in for bombs in a game without them (Espgaluda II's guard barrier): 32-bit words (or 16) and the
    // value each is filled to, and the feature's name and description
    struct Gauge {
        uint32_t address, full;
        int bytes = 4; // (2: a word)
    };
    std::vector<Gauge> gauges;
    // (a title of 22 characters at most: a longer one makes the menu's rows draw smaller on a picture stood up)
    std::string bombsTitle = "Bombs never run out", bombsWords = "The bombs are filled again as they are used.";
    // The count of frames the ship is still out of harm for after it appears (a word of safeFramesBytes bytes: 4, 2
    // or 1), and a value to hold it at
    uint32_t safeFrames = 0, safeFramesHeld = 0;
    int safeFramesBytes = 4;
    // When a stage's boss is on: the sound chip's sequences that are the bosses' tunes (their warning and their
    // theme: a CV1000's), or a word of RAM that is not 0 while it is (bossWord)
    std::vector<int> bossTunes;
    uint32_t bossWord = 0;
    // (the bits of bossWord that say so: a word of flags whose other bits are set before the boss comes)
    uint32_t bossBits = 0xFFFF;
    // Or (68000 boards) what says so from the board as it is, for a game with no such word (Mazinger Z: its stage's
    // script has passed the place the boss comes on at; Air Gallet: its stage has asked for the bosses' tune)
    bool (*bossOn)(const Board &board) = nullptr;
    // A 32-bit word that is 1 while the ship is flown (CV1000; 0: not known; not needed with scriptStage, whose
    // stages begin with the ship flown)
    uint32_t shipState = 0;
    // A word that is negative while the ship is flown (PGM games whose stage is set before the ship is chosen): Begin at
    // the stage presses the shot on, through the choice of ship, until it is (0: not needed); or, where the stage
    // begins from a map of stages after the new game (Deathsmiles), the same until shipState says the ship is flown
    uint32_t flownWord = 0;
    bool beginWaitsFlown = false;
    // The hit boxes as the game tests them (none: the feature is not offered): read as the board is after a frame;
    // or, where the game keeps them only while it tests them (the CV1000 games build them anew each frame), read at the
    // entry of its hit test (hitTest: the routine, hooked; the boxes read there are the frame's)
    void (*hitBoxes)(const Board &board, std::vector<HitBox> &out) = nullptr;
    uint32_t hitTest = 0;
    // The kinds of box hitBoxes gives (bits 1 << HitBox::Kind): each has its colour on the Hit boxes page
    unsigned hitKinds = 1u << HitBox::Ship | 1u << HitBox::Bullet | 1u << HitBox::Body;
};

// The features the facts allow, in this order: the stage a game begins at, straight to its boss, out of harm, bombs
// that do not run out, ships that do not run out, hit boxes (drawn by the port: it only needs the feature's name)
std::vector<recomp::FeatureInfo> Features(const Facts &facts);
// The game's routines hooked on this board (when the board is made, and again when it is made anew)
void Install(Board &board, const Facts &facts);
// Before each frame, with the features' values (`values` at the first of them)
void BeforeFrame(Board &board, const Facts &facts, const int *values);
// Game::unseen: a game begun with "Straight to the boss" on is run unseen, the ship out of harm and firing, until
// the stage's boss is on
int Unseen(Board &board, const Facts &facts, const int *values);

// "Begin at the stage" (Game::beginNow), for a game whose stage start is its own (DoDonPachi SaiDaiOuJou's): whether
// a new game is being driven to its beginning now, and that its stage has begun (the drive ends there)
bool Beginning();
void StageBegun();

// A shooting game's whole Game (runtime/game.h): free play (the credits counter's address: runtime/arcade.h), then
// the training features. One game a program: the facts are kept here
Game MakeGame(uint32_t creditsAddress, const Facts &facts);

} // namespace spelunk::shmup

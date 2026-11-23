// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mazinger Z: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/native68k.h"
#include "../../../runtime/shmup.h"
#include "mazinger.h"

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    int Signed(const Board &board, uint32_t address) {
        return static_cast<int16_t>(Word(board, address));
    }
    uint8_t Byte(const Board &board, uint32_t address) {
        return board.ram[address & 0xFFFF];
    }

    // A record's box in the board's pixels: the game's "across" is down the board's 240 lines and its "along" across
    // the board's 384 pixels (the monitor stands on its side). The far edges are one past the half sizes: two boxes
    // meet when their centres are less than the sum of their halves apart, and at exactly that on one side
    HitBox Box(const Board &board, uint32_t record, int halfAcross, int halfAlong, HitBox::Kind kind) {
        const int across = Signed(board, record + mazinger::kX), along = Signed(board, record + mazinger::kY);
        return {static_cast<float>(along - halfAlong), static_cast<float>(across - halfAcross), static_cast<float>(along + halfAlong + 1),
                static_cast<float>(across + halfAcross + 1), kind};
    }

    // The boxes of everything but the robots, read as the game's hit test (00608E) is entered: the test marks what it
    // hits as not to be tested, and those are the ones wanted. Only drawn: no part of the game's state
    std::vector<HitBox> g_tested;
    uint64_t g_testedFrame = ~0ull;
    const Board *g_board = nullptr;

    void ReadTested(const Board &board, std::vector<HitBox> &out) {
        using namespace mazinger;
        for (int n = 0; n < kObjectCount; n++) {
            const uint32_t at = kObjects + static_cast<uint32_t>(n) * kObjectSize;
            const int flags = Word(board, at + kObjectFlags);
            if (!Word(board, at) || (flags & kNotTested)) {
                continue;
            }
            HitBox::Kind kind;
            if (!(flags & kPlayers)) {
                if (flags & kHitsRobot) {
                    kind = flags & kShotsHurt ? HitBox::Body : HitBox::Bullet;
                } else if (flags & (kShotsHurt | kAttacksHurt)) {
                    kind = HitBox::Ground;
                } else {
                    continue;
                }
            } else if (!(flags & kNoShot)) {
                kind = HitBox::Shot;
            } else {
                continue; // (a thing to pick up, or nothing the test looks at)
            }
            out.push_back(Box(board, at, Byte(board, at + kHalfAcross), Byte(board, at + kHalfAlong), kind));
        }
    }

    // Game::hitBoxes: the boxes the last test was entered with (the game runs no test on a frame it is late for: the
    // last one's stay for the few frames that can last), and each robot's where it is now
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace mazinger;
        if (board.frame - g_testedFrame - 1 < 4) {
            out.insert(out.end(), g_tested.begin(), g_tested.end());
        }
        for (uint32_t player : {kPlayer1, kPlayer2}) {
            const int state = Word(board, player);
            if (state != 0 && state != kStateOut && state != kStateChoosing && state < kStateNotHit) {
                HitBox box = Box(board, player, kRobotHalfAcross, kRobotHalfAlong, HitBox::Ship);
                box.x1 -= 1; // (the robot's own box ends at its half sizes: the pixel more is the other box's)
                box.y1 -= 1;
                out.push_back(box);
            }
        }
    }

    // Whether the stage's boss is on: the stage's script (its pointer) has passed the place the boss comes on at
    bool BossOn(const Board &board) {
        struct Place {
            uint32_t from, end; // (the script from the entry after the boss's, and where the stage's script ends)
        };
        // Stage 1 to 8: the entry after the one that starts the boss's tune (tune 0x0D; the last stage's 0x0E). Stage 3
        // has no such entry (its tune plays on): the entry at scroll 0x0A00, passed as the battleship sinks and the
        // boss comes in. Found by reading each stage's script (the eight pointers at 000400) and by each stage played
        // with the pointer and the picture noted every 250 frames
        static constexpr Place kBoss[mazinger::kStages] = {
            {0x0388F2, 0x038A26}, {0x0391A2, 0x0391AA}, {0x03B440, 0x03B448}, {0x0395F4, 0x039622},
            {0x03A57C, 0x03A59E}, {0x03A982, 0x03A9A0}, {0x03B376, 0x03B3A0}, {0x03AC8C, 0x03ACCC},
        };
        const int stage = Word(board, mazinger::kStage);
        const uint32_t script = (static_cast<uint32_t>(Word(board, mazinger::kScript)) << 16) | static_cast<uint32_t>(Word(board, mazinger::kScript + 2));
        return stage < mazinger::kStages && script >= kBoss[stage].from && script < kBoss[stage].end;
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7", "Stage 8",
                    "Loop 2, stage 1", "Loop 2, stage 2", "Loop 2, stage 3", "Loop 2, stage 4",
                    "Loop 2, stage 5", "Loop 2, stage 6", "Loop 2, stage 7", "Loop 2, stage 8"};
        f.newGame = shmup::HookAt(mazinger::kNewGame);
        f.beginStage = shmup::HookAt(mazinger::kBeginStage);
        f.stageWord = mazinger::kStage;
        f.newGameStage = [](M68K &, Board &board, int stage) {
            // (as the program's own stage select leaves them: 00460C)
            board.ram[mazinger::kStage & 0xFFFF] = 0;
            board.ram[(mazinger::kStage + 1) & 0xFFFF] = static_cast<uint8_t>(stage % mazinger::kStages);
            board.ram[mazinger::kLoop & 0xFFFF] = static_cast<uint8_t>(stage / mazinger::kStages);
        };
        f.bossOn = BossOn;
        f.ships = mazinger::kEnergy;
        f.shipsMask = mazinger::kEnergyBars;
        f.shipsKept = 2; // (a hit that takes the last bar ends the game)
        f.shipsTitle = "Energy never runs out";
        f.shipsWords = "A bar of energy lost to a hit is given back: the game never ends for want of one.";
        f.bombs = mazinger::kBombs; // (bit 7 is set while a bomb goes off: the count is above 3 then, and left alone)
        f.bombsHeld = 3;
        f.safeFrames = mazinger::kSafeFrames;
        f.safeFramesHeld = 0x78;
        f.safeFramesBytes = 1;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(mazinger::kCredits, f);
        // (the shared hooks, then the game's own: the boxes are read where its hit test begins)
        static void (*const shared)(Board &) = g.start;
        g.start = [](Board &board) {
            shared(board);
            g_board = &board;
            g_testedFrame = ~0ull;
            AddHook(shmup::HookAt(mazinger::kHitTest), "mazinger: the hit test", [](M68K &) {
                g_tested.clear();
                g_testedFrame = g_board->frame;
                ReadTested(*g_board, g_tested);
                return false; // (the routine itself runs)
            });
        };
        return g;
    }();
    return game;
}

} // namespace spelunk

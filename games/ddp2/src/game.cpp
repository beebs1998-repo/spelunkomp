// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi II: Bee Storm: the game's own features (free play; the training features from its facts:
// runtime/shmup.h).
#include "../../../runtime/native68k.h"
#include "../../../runtime/shmup.h"
#include "ddp2.h"

#include <cstdio>
#include <cstdlib>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const size_t mask = board.ram.size() - 1;
        return (board.ram[address & mask] << 8) | board.ram[(address + 1) & mask];
    }
    int Signed(const Board &board, uint32_t address) {
        return static_cast<int16_t>(Word(board, address));
    }
    uint32_t Long(const Board &board, uint32_t address) {
        return static_cast<uint32_t>(Word(board, address)) << 16 | static_cast<uint32_t>(Word(board, address + 2));
    }
    // A word of the RAM shared with the ARM, at the 68000's D00000 + offset (the ARM's memory is low byte first)
    int Shared(const Board &board, uint32_t offset) {
        return static_cast<int16_t>(board.armShared[offset & 0xFFFF] | board.armShared[(offset + 1) & 0xFFFF] << 8);
    }

    // The boxes the game's hit tests (15028E and what it calls) use, read as they read them. The game's places are
    // the picture's pixels as the board draws it.
    void BoxesNow(const Board &board, std::vector<HitBox> &out) {
        using namespace ddp2;
        // (from a to b with both in the box)
        auto closed = [&](int x0, int y0, int x1, int y1, HitBox::Kind kind) {
            out.push_back({static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1 + 1), static_cast<float>(y1 + 1), kind});
        };
        // (the tests of the laser and the bee storm want an enemy's box to reach past the edge: neither edge is in it)
        auto open = [&](int x0, int y0, int x1, int y1) {
            if (x1 - x0 > 1 && y1 - y0 > 1) {
                out.push_back({static_cast<float>(x0 + 1), static_cast<float>(y0 + 1), static_cast<float>(x1), static_cast<float>(y1), HitBox::Shot});
            }
        };
        // (every test is for a ship that is flown: with none, nothing is tested, and the records are what the last game
        // left)
        if (Word(board, kShip1 + kShipState) != 4 && Word(board, kShip2 + kShipState) != 4) {
            return;
        }
        for (int n = 0; n < kEnemySlots; n++) {
            const uint32_t enemy = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if (!Word(board, enemy + kEnemyThere)) {
                continue;
            }
            const int boxes = Word(board, enemy + kEnemyBoxes);
            for (int k = 0; k < boxes && k < 10; k++) {
                const uint32_t box = enemy + kEnemyBox + static_cast<uint32_t>(k) * kBoxSize;
                const int flags = Word(board, box);
                if (!(flags & 3) || static_cast<int32_t>(Long(board, box + 0xC)) <= 0) {
                    continue;
                }
                closed(Signed(board, box + 4), Signed(board, box + 6), Signed(board, box + 8), Signed(board, box + 10),
                       (flags & 2) ? HitBox::Body : HitBox::Ground);
            }
        }
        const bool beeStorm = Word(board, kBeeStormOn) == 1;
        if (beeStorm) {
            const int x = Signed(board, Long(board, kBeeStorm + 0xE)), y = Signed(board, Long(board, kBeeStorm + 0x12));
            open(x - Signed(board, kBeeStorm + 0x18), y - Signed(board, kBeeStorm + 0x1A), x, y + Signed(board, kBeeStorm + 0x1A));
            const int from = x - Signed(board, kBeeStorm + 0x2E2), side = y + Signed(board, kBeeStorm + 0x2A) - Signed(board, kBeeStorm + 0x16);
            open(from, side, from + Word(board, kBeeStorm + 6) * 32 + (board.ram[(kBeeStorm + 2) & 0x1FFFF] ? 0 : 32),
                 side + 2 * Signed(board, kBeeStorm + 0x16));
        }
        for (int player = 0; player < 2; player++) {
            const uint32_t ship = player ? kShip2 : kShip1, laser = player ? kLaser2 : kLaser1;
            if (Word(board, ship + kShipState) != 4) {
                continue;
            }
            const int x = Signed(board, ship + kShipX), y = Signed(board, ship + kShipY);
            closed(x - 4, y - 3, x + 4, y + 3, HitBox::Ship);
            if (beeStorm) {
                continue;
            }
            if (board.ram[laser & 0x1FFFF] >= 4) {
                const int lx = Signed(board, Long(board, laser + 8)), ly = Signed(board, Long(board, laser + 0xC));
                open(lx - Signed(board, laser + 0x16), ly - Signed(board, laser + 0x18), lx, ly + Signed(board, laser + 0x18));
                const int from = lx - Signed(board, laser + 0x1CC), to = from + board.ram[(laser + 3) & 0x1FFFF] * 32;
                const int side = ly - Signed(board, laser + 0x1CE), far = side + Signed(board, laser + 0x14);
                open(from, side, to, far);
                if (!board.ram[(laser + 2) & 0x1FFFF]) {
                    open(to, side, to + 32, far);
                }
                continue;
            }
            for (int n = 0; n < kShotSlots; n++) {
                const uint32_t shot = (player ? kShots2 : kShots1) + static_cast<uint32_t>(n) * kShotSize;
                const int x0 = Signed(board, shot + 0x14), x1 = Signed(board, shot + 0x18);
                if (x1 >= x0) {
                    closed(x0, Signed(board, shot + 0x16), x1, Signed(board, shot + 0x1A), HitBox::Shot);
                }
            }
        }
        for (int n = 0; n < kBulletSlots; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            if (Shared(board, at) == 2) {
                closed(Shared(board, at + 6), Shared(board, at + 8), Shared(board, at + 6), Shared(board, at + 8), HitBox::Bullet);
            }
        }
    }

    // The boxes for the picture on show. The board shows a sprite list a frame after the game made it (the DMA takes
    // the list late in a frame and the chip shows it the next: machine/pgm.h), and this game makes its list from the
    // same places its tests use, in the same frame (Cave's games on the board are a frame behind in their lists
    // already, so the port draws a PGM game's boxes with no lag: Hw::kPictureLag). So what is drawn with a frame's
    // picture is the frame before's boxes: kept here from the last call, when that was for the frame before (after a
    // rewind or a state loaded, for one frame, the frame's own). Only drawn: no part of the game's state.
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        static std::vector<HitBox> now, before;
        static uint64_t frame = ~0ull;
        if (board.frame != frame) {
            std::vector<HitBox> read;
            BoxesNow(board, read);
            before = board.frame == frame + 1 ? now : read;
            now = std::move(read);
            frame = board.frame;
        }
        out.insert(out.end(), before.begin(), before.end());
    }

    // Free play. A credit put in the game's counter, as the toolkit's other games are given one, is not enough here:
    // the demonstration and the title go to the "push start" screen only for a coin they are told of (the coin's
    // code, 14A0E6, puts a message in a queue), and a press of Start before that screen is up (some 45 frames after
    // the coin) is lost. So a credit is kept in by the board's own coin switch: whenever the counter is at 0 a coin is
    // dropped, the game is at its "push start" screen before the player is, and Start is the player's own press, with
    // no wait. (featureWords[1]: the frames since the coin's drop, plus one. runtime/arcade.h's CreditAtStart is given
    // no counter here and does nothing but keep the Start switches in featureWords[0].)
    constexpr uint32_t kCoinFrames = 6, kCoinRest = 12;
    void KeepCoin(Board &board, bool on) {
        uint32_t &since = board.featureWords[1];
        if (since) {
            since++;
            if (since == 1 + kCoinFrames) {
                board.systemHeld = 0;
            } else if (since == 1 + kCoinFrames + kCoinRest) {
                since = 0;
            }
            return;
        }
        if (on && board.frame > 1200 && board.ram[ddp2::kCredits & (board.ram.size() - 1)] == 0) {
            since = 1;
            board.systemHeld = kSysCoin1;
        }
    }

    size_t g_endlessBombs = ~size_t(0);
    Board *g_board = nullptr;
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7"};
        f.newGame = shmup::HookAt(ddp2::kNewGame);
        f.newGameStage = [](M68K &, Board &board, int choice) {
            // (Practice has four stages: a later one asked for is its last)
            const bool practice = Word(board, ddp2::kMode) == 0;
            const int place = practice && choice >= ddp2::kPracticeStages ? ddp2::kPracticeStages - 1 : choice;
            const uint32_t at = ddp2::kStageOrder & static_cast<uint32_t>(board.ram.size() - 1);
            const int before = place - 1; // (the stage's beginning adds one: FFFF for the first)
            board.ram[at] = static_cast<uint8_t>(before >> 8);
            board.ram[at + 1] = static_cast<uint8_t>(before);
        };
        f.beginStage = shmup::HookAt(ddp2::kBeginStage);
        f.stageWord = ddp2::kStageOrder;
        f.ships = ddp2::kShips;
        f.bombs = ddp2::kBombs;
        f.bombsHeld = ddp2::kBombsFull;
        f.safeFrames = ddp2::kSafeFrames;
        f.safeFramesHeld = 0x100;
        f.safeFramesBytes = 2;
        // (no word says a boss is on: the trip to the boss is ended by the hook at the boss's warning, below; this word
        // with no bits to look at only tells the shared code that the game has a cue)
        f.bossWord = ddp2::kStageOrder;
        f.bossBits = 0;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(0, f);
        for (size_t n = 0; n < g.features.size(); n++) {
            if (g.features[n].name == "endless_bombs") {
                g_endlessBombs = n;
            }
        }
        static void (*const install)(Board &) = g.start;
        g.start = [](Board &board) {
            install(board);
            // A boss's warning begins: a trip to the boss (runtime/shmup.cpp: the frames run unseen towards it, kept
            // in featureWords[3]) is over
            g_board = &board;
            AddHook(shmup::HookAt(ddp2::kBossWarning), "ddp2: a boss's warning", [](M68K &) {
                if (getenv("SPELUNK_TRACE_MODES")) {
                    fprintf(stderr, "a boss's warning: frame %llu%s\n", static_cast<unsigned long long>(g_board->frame),
                            g_board->featureWords[3] ? " (the trip to the boss ends)" : "");
                }
                g_board->featureWords[3] = 0;
                return false;
            });
        };
        static void (*const rest)(Board &, const std::vector<int> &) = g.beforeFrame;
        g.beforeFrame = [](Board &board, const std::vector<int> &values) {
            KeepCoin(board, values[0] != 0);
            rest(board, values);
            // (a ship of the Energy kind has its gauge where the bombs are counted: kept full instead)
            if (board.frame >= 1200 && g_endlessBombs < values.size() && values[g_endlessBombs] &&
                board.ram[ddp2::kWeaponKind & (board.ram.size() - 1)] == 2) {
                board.ram[ddp2::kBombs & (board.ram.size() - 1)] = ddp2::kEnergyFull;
            }
        };
        return g;
    }();
    return game;
}

} // namespace spelunk

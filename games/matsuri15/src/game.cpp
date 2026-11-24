// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama Matsuri Ver 1.5: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "matsuri15.h"

namespace spelunk {

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    float Fixed(const Board &board, uint32_t address) {
        return static_cast<int32_t>(Long(board, address)) / 65536.0f;
    }
    // A box of the game's (16.16, up from the bottom of the picture stood up) as the board's pixels: the board's
    // across is the game's up, its down the game's across
    void Box(const Board &board, uint32_t box, HitBox::Kind kind, std::vector<HitBox> &out) {
        const float x = Fixed(board, box + 12), y = Fixed(board, box + 16);
        out.push_back({y - Fixed(board, box + 32), x - Fixed(board, box + 24), y + Fixed(board, box + 28), x + Fixed(board, box + 20), kind});
    }
    uint16_t Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint16_t>(p[0] << 8 | p[1]);
    }
    // Each record of a list of lists (headers of 8 bytes: the count at +0, the first record at +4, records 80 bytes)
    template <typename Each> void Records(const Board &board, uint32_t headers, uint32_t headerCount, Each each) {
        using namespace matsuri15;
        const uint32_t count = Long(board, headerCount);
        for (uint32_t h = 0; h < count && h < 256; h++) {
            const uint32_t header = headers + h * kHeaderStride, records = Long(board, header), first = Long(board, header + kHeaderFirst);
            for (uint32_t n = 0; n < records && n < 1024; n++) {
                each(first + n * 80);
            }
        }
    }
    // At the game's hit test: what it is about to test
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace matsuri15;
        for (int list = 0; list < 2; list++) {
            Records(board, kBodyHeaders[list], kBodyHeaderCounts[list], [&](uint32_t record) {
                const uint16_t shot = Word(board, record);
                if (Word(board, record + 4)) {
                    Box(board, record, HitBox::Body, out);
                } else if (list == 0 ? shot == kHurtByShotsFirst : shot != 0) {
                    Box(board, record, HitBox::Ground, out);
                }
            });
        }
        Records(board, kShotHeaders, kShotHeaderCount, [&](uint32_t record) {
            if (Word(board, record)) {
                Box(board, record, HitBox::Shot, out);
            }
        });
        // (the lasers: one record an entry)
        const uint32_t lasers = Long(board, kLaserCount);
        for (uint32_t n = 0; n < lasers && n < 256; n++) {
            Box(board, Long(board, kLasers + n * 8 + 4), HitBox::Shot, out);
        }
        if (const uint32_t block = Long(board, kBullets)) {
            const uint32_t count = Long(board, block + kBulletCount);
            for (uint32_t n = 0; n < count && n < 4096; n++) {
                const uint32_t bullet = Long(board, block + kBulletList + n * kBulletStride + kBulletRecord);
                const float x = Fixed(board, bullet + 12), y = Fixed(board, bullet + 16);
                out.push_back({y, x, y + 1 / 65536.0f, x + 1 / 65536.0f, HitBox::Bullet});
            }
        }
        if (const uint32_t box = Long(board, kShipBox)) {
            Box(board, box, HitBox::Ship, out);
        } else {
            // (out of harm the game makes no box: drawn where it would be, its size as the game's)
            const float x = Fixed(board, kShipAt), y = Fixed(board, kShipAt + 4), r = 2.625f;
            out.push_back({y - r, x - r, y + r, x + r, HitBox::Ship});
        }
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.stageArguments = {{matsuri15::kNewGame, 4, matsuri15::kNewGameCaller}, {matsuri15::kBeginStage, 5}};
        f.ships = matsuri15::kShips;
        f.bombs = matsuri15::kBombs;
        f.safeFrames = matsuri15::kSafeFrames;
        f.safeFramesHeld = 0x97;
        f.bossTunes.assign(std::begin(matsuri15::kBossTunes), std::end(matsuri15::kBossTunes));
        f.shipState = matsuri15::kShipState;
        f.hitTest = matsuri15::kHitTest;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        return shmup::MakeGame(matsuri15::kCredits, f);
    }();
    return game;
}

} // namespace spelunk

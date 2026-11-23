// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Air Gallet: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "agallet.h"

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    // The boxes the game's hit test (01005E) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace agallet;
        // (128ths of a pixel of the chips' picture, which the screen shows from its second pixel; the far edges are in
        // the box, so it reaches to the 128th after them)
        const float left = static_cast<float>(board.video.left);
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + 2), y = Word(board, record + 4);
            out.push_back({(x - Word(board, record + kBoxLeft)) / 128.0f - left, (y - Word(board, record + kBoxUp)) / 128.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 128.0f - left, (y + Word(board, record + kBoxDown) + 1) / 128.0f, kind});
        };
        // The enemies whose bodies hit the ship; then those the shots can hurt that never do (on the ground)
        const bool groundHits = Word(board, kGroundHits) != 0;
        auto hits = [&](int n, int flags) { return (flags & (0x8000 | kHitsShip)) == (0x8000 | kHitsShip) && (groundHits || n >= kEnemiesOnGround); };
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if (hits(n, Word(board, at))) {
                box(at, HitBox::Body);
            }
        }
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            const int flags = Word(board, at);
            if ((flags & (0x8000 | kShootable)) == (0x8000 | kShootable) && !hits(n, flags)) {
                box(at, HitBox::Ground);
            }
        }
        struct Player { uint32_t ship, shots; };
        for (const Player &p : {Player{kShip1, kShots1}, Player{kShip2, kShots2}}) {
            if (!(Word(board, p.ship) & 0x8000)) {
                continue;
            }
            for (int n = 0; n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                if ((Word(board, at) & 0x9000) == 0x8000) {
                    box(at, HitBox::Shot);
                }
            }
        }
        for (int n = 0; n < kBulletCount; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            if ((Word(board, at) & 0x8100) == 0x8100) {
                box(at, HitBox::Bullet);
            }
        }
        for (uint32_t ship : {kShip1, kShip2}) {
            if (Word(board, ship) & 0x8000) {
                box(ship, HitBox::Ship);
            }
        }
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        // (a credit that does not arrive as a coin takes the attract show only as far as the title, at the first Start
        // its ranking or demonstration sees; the title begins a game at the next, 30 frames later or more: found
        // 2026-11-15 by coins and Starts at measured frames)
        f.startAgain = 40;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.newGame = shmup::HookAt(agallet::kNewGame);
        f.beginStage = shmup::HookAt(agallet::kBeginStage);
        f.newGameStage = [](M68K &, Board &board, int stage) {
            board.ram[agallet::kStage & 0xFFFF] = 0;
            board.ram[(agallet::kStage + 1) & 0xFFFF] = static_cast<uint8_t>(stage);
        };
        f.stageWord = agallet::kStage;
        f.modeWord = agallet::kMode;
        f.playModes = {agallet::kModePlay};
        // (Begin at the stage: the shot pressed on, through the first stage's launch from the carrier, until the ship
        // is the player's)
        f.flownWord = agallet::kShip1;
        f.ships = agallet::kShips;
        f.shipsBytes = 2;
        f.shipsKept = 2;
        f.bombs = agallet::kBombs;
        f.bombsHeld = 3;
        f.safeFrames = agallet::kSafeFrames;
        f.safeFramesHeld = 0x78;
        f.safeFramesBytes = 2;
        f.bossOn = [](const Board &board) {
            using namespace agallet;
            const int stage = Word(board, kStageNow), place = Word(board, kPlace);
            if (stage == 0) {
                return Word(board, kFirstBossOn) != 0 || place >= static_cast<int>(kFirstBossPlace);
            }
            // (the stage's place in the game's own table, as its task reads it)
            const uint8_t *entry = &board.program[kBossTunes + static_cast<uint32_t>(stage & 7) * 8];
            return stage < 6 && place >= ((entry[0] << 8) | entry[1]);
        };
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        return shmup::MakeGame(agallet::kCredits, f);
    }();
    return game;
}

} // namespace spelunk

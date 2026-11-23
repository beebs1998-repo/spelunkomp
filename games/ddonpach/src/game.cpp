// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ddonpach.h"

#include <cstring>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    // The boxes the game's hit test (0057EE) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace ddonpach;
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + 6), y = Word(board, record + 8);
            // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
            out.push_back({(x - Word(board, record + kBoxLeft)) / 64.0f, (y - Word(board, record + kBoxUp)) / 64.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 64.0f, (y + Word(board, record + kBoxDown) + 1) / 64.0f, kind});
        };
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            const uint8_t flags = board.ram[at & 0xFFFF];
            if ((flags & 0xC0) == 0xC0 && ((Word(board, at + 6) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots can hurt whose bodies do not hit the ship (on the ground)
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            const uint8_t flags = board.ram[at & 0xFFFF];
            if ((flags & (0x80 | kShootable)) == (0x80 | kShootable) && !(flags & 0x40) &&
                ((Word(board, at + 6) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Ground);
            }
        }
        // Each ship's shots, and its laser while it is held
        struct Player { uint32_t ship, shots, laser, laserParts; };
        for (const Player &p : {Player{kShip1, kShots1, kLaser1, kLaserParts1}, Player{kShip2, kShots2, kLaser2, kLaserParts2}}) {
            if (!(Word(board, p.ship) & 0x8000)) {
                continue;
            }
            for (int n = 0; n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                if ((Word(board, at) & 0xA000) == 0x8000 && !(Word(board, at + 0x18) & 0x8000)) {
                    box(at, HitBox::Shot);
                }
            }
            if ((board.ram[(p.ship + 1) & 0xFFFF] & 0x80) || !board.ram[(p.ship + 0x3F) & 0xFFFF]) {
                continue;
            }
            for (uint32_t part : {p.laserParts, p.laserParts + 0x60}) {
                if (Word(board, part) & 0x8000) {
                    box(part, HitBox::Shot);
                }
            }
            if ((Word(board, p.laser) & 0x8200) == 0x8200) {
                const int x = Word(board, p.laser + 2), y = Word(board, p.laser + 4);
                out.push_back({x / 64.0f, (y - Word(board, p.laser + 0xA)) / 64.0f, Word(board, p.laser + 0x10) / 64.0f,
                               (y + Word(board, p.laser + 8) + 1) / 64.0f, HitBox::Shot});
            }
        }
        const int bullets = Word(board, kLoop) ? kBulletCountLoop2 : kBulletCount;
        for (int n = 0; n < bullets; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const uint8_t flags = board.ram[at & 0xFFFF];
            if ((flags & 0x80) && !(flags & 0x51)) {
                const float x = Word(board, at + 6) / 64.0f, y = Word(board, at + 8) / 64.0f;
                out.push_back({x, y, x + 1 / 64.0f, y + 1 / 64.0f, HitBox::Bullet});
            }
        }
        for (uint32_t ship : {kShip1, kShip2}) {
            if (Word(board, ship) & 0x8000) {
                box(ship, HitBox::Ship);
            }
        }
    }
} // namespace

// (dodge.cpp)
void AddDodgeTraining(Game &game);

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.newGame = shmup::HookAt(ddonpach::kNewGame);
        f.beginStage = shmup::HookAt(ddonpach::kBeginStage);
        f.nextStage = [](Board &board) {
            uint8_t *task = &board.ram[ddonpach::kStageEndTask & 0xFFFF];
            std::memset(task, 0, 0x40);
            task[0] = 0x80;
            task[1] = 0x3A;
            board.ram[ddonpach::kBossState & 0xFFFF] |= ddonpach::kBossBeaten;
        };
        f.readyWord = ddonpach::kShipWaits;
        f.stageWord = ddonpach::kStage;
        f.loopWord = ddonpach::kLoop;
        f.stagesPerLoop = 6;
        f.modeWord = ddonpach::kMode;
        f.playModes = {ddonpach::kModePlay, ddonpach::kModeContinue};
        f.ships = ddonpach::kShips;
        f.shipsKept = 2;
        f.bombs = ddonpach::kBombs;
        f.bombsMax = ddonpach::kBombStock;
        f.safeFrames = ddonpach::kSafeFrames;
        f.safeFramesHeld = 0xEF;
        f.safeFramesBytes = 1;
        f.bossWord = ddonpach::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(ddonpach::kCredits, f);
        AddDodgeTraining(g);
        return g;
    }();
    return game;
}

} // namespace spelunk

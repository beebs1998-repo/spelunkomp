// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Ou-Jou: the game's own features (free play; the training features from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ddpdoj.h"

#include <algorithm>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const size_t mask = board.ram.size() - 1;
        return (board.ram[address & mask] << 8) | board.ram[(address + 1) & mask];
    }
    // The boxes the game's hit test (1440A2) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace ddpdoj;
        // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
        // (the reaches are signed: a negative one puts that edge on the other side of the position)
        auto reach = [&](uint32_t address) { return static_cast<int>(static_cast<int16_t>(Word(board, address))); };
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + kX), y = Word(board, record + kY);
            out.push_back({(x - reach(record + kBoxLeft)) / 64.0f, (y - reach(record + kBoxUp)) / 64.0f,
                           (x + reach(record + kBoxRight) + 1) / 64.0f, (y + reach(record + kBoxDown) + 1) / 64.0f, kind});
        };
        auto body = [&](uint32_t at) {
            return (Word(board, at) & 1) && ((Word(board, at + kX) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0xF800;
        };
        int enemies = Word(board, kEnemyCount1) + Word(board, kEnemyCount2);
        for (uint32_t at = kEnemies; enemies > 0 && at < kEnemyCount1; at += kEnemySize) {
            if (!(Word(board, at) & 0x8000)) {
                continue;
            }
            enemies--;
            if (body(at)) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots hurt (in either group) that the ship's test leaves out: the ground's
        auto group = [&](uint32_t from, int slots, uint32_t count, int moved, int onScreen) {
            int left = Word(board, count);
            for (int n = 0; n < slots && left > 0; n++) {
                const uint32_t at = from + static_cast<uint32_t>(n) * kEnemySize;
                const int flags = Word(board, at);
                if (!(flags & 0x8000)) {
                    continue;
                }
                left--;
                if ((flags & kShootable) && !body(at) && ((Word(board, at + kX) + moved - Word(board, at + kBoxLeft)) & 0xFFFF) < onScreen) {
                    box(at, HitBox::Ground);
                }
            }
        };
        group(kEnemies, kAirSlots, kEnemyCount1, kAirMoved, kAirOnScreen);
        group(kGroundEnemies, kGroundSlots, kEnemyCount2, kGroundMoved, kGroundOnScreen);
        // The player's shots, and the laser while it is on
        for (uint32_t shots : {kShots1, kShots2}) {
            for (int n = 0; n < kShotSlots; n++) {
                const uint32_t at = shots + static_cast<uint32_t>(n) * kShotSize;
                if (!(Word(board, at) & 0x8000) || (Word(board, at + kShotPower) & 0x8000)) {
                    continue;
                }
                const int x = Word(board, at + kX) + reach(at + kShotRight), y = Word(board, at + kY) + reach(at + kShotDown);
                out.push_back({(x - reach(at + kShotWidth)) / 64.0f, (y - 2 * reach(at + kShotHalfHeight)) / 64.0f,
                               (x + 1) / 64.0f, (y + 1) / 64.0f, HitBox::Shot});
            }
        }
        for (int player = 0; player < 2; player++) {
            const uint32_t ship = player ? kShip2 : kShip1;
            const int flags = Word(board, ship);
            if (!(flags & 0x8000) || (flags & 0x80) || !board.ram[(ship + kShipLaserOn) & (board.ram.size() - 1)]) {
                continue;
            }
            const uint32_t other = player ? kLaserOther2 : kLaserOther1, aura = player ? kLaserAura2 : kLaserAura1;
            if ((Word(board, other) & 0x8000) && Word(board, other + kX) < 0x7000) {
                box(other, HitBox::Shot);
            }
            if (Word(board, aura) & 0x8000) {
                box(aura, HitBox::Shot);
            }
            const uint32_t laser = player ? kLaser2 : kLaser1;
            if ((Word(board, laser) & 0x8200) == 0x8200) {
                const int x = Word(board, laser + kX), y = Word(board, laser + kY);
                const int end = std::min(x + 0x400 + reach(laser + kLaserLength), Word(board, laser + kLaserEnd));
                out.push_back({x / 64.0f, (y - reach(laser + kLaserUp)) / 64.0f, (end + 1) / 64.0f,
                               (y + reach(laser + kLaserDown) + 1) / 64.0f, HitBox::Shot});
            }
        }
        int more = 0;
        while (more < 4 && Word(board, kMoreBullets + 2 * more)) {
            more++;
        }
        for (int n = 0; n < kBulletCounts[more]; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const int flags = Word(board, at);
            if ((flags & 0x8000) && !(flags & 0x5100)) {
                const float x = Word(board, at + kX) / 64.0f, y = Word(board, at + kY) / 64.0f;
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

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.newGame = shmup::HookAt(ddpdoj::kNewGame);
        f.newGameStage = [](M68K &cpu, Board &board, int stage) {
            const uint32_t at = (cpu.a[0] + ddpdoj::kTaskStage) & static_cast<uint32_t>(board.ram.size() - 1);
            board.ram[at] = static_cast<uint8_t>(stage >> 8);
            board.ram[at + 1] = static_cast<uint8_t>(stage);
        };
        f.beginStage = shmup::HookAt(ddpdoj::kSetStage);
        f.flownWord = ddpdoj::kShipFlown;
        f.stageWord = ddpdoj::kStage;
        f.stagesPerLoop = 5;
        f.ships = ddpdoj::kShips;
        f.bombs = ddpdoj::kBombs;
        f.bombsMax = ddpdoj::kBombStock;
        f.safeFrames = ddpdoj::kSafeFrames;
        f.safeFramesHeld = 0xF0;
        f.safeFramesBytes = 1;
        f.bossWord = ddpdoj::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        return shmup::MakeGame(ddpdoj::kCredits, f);
    }();
    return game;
}

} // namespace spelunk

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Deathsmiles MegaBlack Label: the game's own features (free play and the training features, from its facts:
// runtime/shmup.h).
#include <algorithm>

#include "../../../runtime/shmup.h"
#include "dsmbl.h"

namespace spelunk {

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<int16_t>(p[0] << 8 | p[1]);
    }
    uint8_t Byte(const Board &board, uint32_t address) {
        return board.ram[address & (SH3::kRamSize - 1)];
    }
    constexpr float kTall = 240; // (the game counts up from the picture's bottom)

    // Each member of a kind that the game tests (its group's long at +8, its own at +8 and +12)
    template <typename Fn>
    void Members(const Board &board, int kind, Fn &&fn, bool testTwelve = true) {
        int guard = 0;
        for (uint32_t group = Long(board, dsmbl::kKinds + kind * 12 + 4); group && guard < 4096; group = Long(board, group)) {
            if (!Long(board, group + 8)) {
                continue;
            }
            for (uint32_t m = Long(board, group + 28); m && guard < 4096; m = Long(board, m), guard++) {
                if (Long(board, m + 8) && (!testTwelve || Long(board, m + 12))) {
                    fn(m);
                }
            }
        }
    }
    // A member's box (reaching right +32, left +36, up +40, down +44 from its place) as the board's pixels
    void MemberBox(const Board &board, uint32_t m, HitBox::Kind kind, std::vector<HitBox> &out) {
        int x = Word(board, m + 52), y = Word(board, m + 56);
        if (const uint32_t owner = Long(board, m + 60)) {
            x += Word(board, owner);
            y += Word(board, owner + 4);
        }
        const int right = static_cast<int>(Long(board, m + 32)), left = static_cast<int>(Long(board, m + 36));
        const int up = static_cast<int>(Long(board, m + 40)), down = static_cast<int>(Long(board, m + 44));
        out.push_back({static_cast<float>(x - left), kTall - static_cast<float>(y + up), static_cast<float>(x + right + 1),
                       kTall - static_cast<float>(y - down) + 1, kind});
    }
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace dsmbl;
        // The enemies: those whose bodies hit the character, and the rest, which only the shots hit (unless shots fly
        // through them too). Most enemies have both: a small box that hits the character and a larger one the shots
        // hit (members with the same owner at +60); such an enemy is drawn by its body alone
        std::vector<uint32_t> bodies;
        for (int kind : kBodyKinds) {
            Members(board, kind, [&](uint32_t m) {
                if (Byte(board, m + 27) & 1) {
                    MemberBox(board, m, HitBox::Body, out);
                    bodies.push_back(Long(board, m + 60));
                }
            });
        }
        for (int kind : kBodyKinds) {
            const bool canPass = std::find(std::begin(kShotsPassKinds), std::end(kShotsPassKinds), kind) != std::end(kShotsPassKinds);
            Members(board, kind, [&](uint32_t m) {
                const uint8_t flags = Byte(board, m + 27);
                const uint32_t owner = Long(board, m + 60);
                if (!(flags & 1) && !(canPass && (flags & 2)) && (!owner || std::find(bodies.begin(), bodies.end(), owner) == bodies.end())) {
                    MemberBox(board, m, HitBox::Ground, out);
                }
            });
        }
        // The player's shots (both players' kinds; the main shots' long at +12 tested, the others' not, as the game does)
        for (int kind = kShotKindFirst; kind <= kShotKindLast; kind++) {
            const bool main = std::find(std::begin(kMainShotKinds), std::end(kMainShotKinds), kind) != std::end(kMainShotKinds);
            Members(board, kind, [&](uint32_t m) { MemberBox(board, m, HitBox::Shot, out); }, main);
        }
        Members(board, kBulletKind, [&](uint32_t m) {
            if (const uint32_t owner = Long(board, m + 60)) {
                const float x = static_cast<float>(Word(board, owner)), y = kTall - static_cast<float>(Word(board, owner + 4));
                out.push_back({x, y, x + 1, y + 1, HitBox::Bullet});
            }
        });
        Members(board, kShipKind, [&](uint32_t m) {
            int x = Word(board, m + 52), y = Word(board, m + 56);
            const uint32_t owner = Long(board, m + 60);
            if (owner) {
                x += Word(board, owner);
                y += Word(board, owner + 4);
            }
            int right = static_cast<int>(Long(board, m + 32)), left = static_cast<int>(Long(board, m + 36));
            int up = static_cast<int>(Long(board, m + 40)), down = static_cast<int>(Long(board, m + 44));
            if (owner && right == kOutOfHarmReach && left == kOutOfHarmReach && up == kOutOfHarmReach && down == kOutOfHarmReach) {
                // (out of harm the game gives the ship its items' reach: drawn is the box it has when it can be hit)
                const uint32_t ship = owner - 4, box = Long(board, kShipBoxes + 4 * Byte(board, ship + kCharacter));
                const bool turned = Long(board, ship + kTurned) == 0x80;
                right = Byte(board, box + (turned ? 8 : 9));
                left = Byte(board, box + (turned ? 9 : 8));
                up = Byte(board, box + 10);
                down = Byte(board, box + 11);
            }
            out.push_back({static_cast<float>(x - left), kTall - static_cast<float>(y + up), static_cast<float>(x + right + 1),
                           kTall - static_cast<float>(y - down) + 1, HitBox::Ship});
        });
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Port town", "Graveyard", "Forest", "Lake", "Ruins", "Cavern", "Castle (7th place)", "Canyon",
                    "Castle (9th place)"};
        f.stageArguments = {{dsmbl::kNewGame, 5, dsmbl::kNewGameCaller}};
        f.ships = dsmbl::kLife;
        f.shipsBytes = 2;
        f.shipsKept = 2 * dsmbl::kLifePerShip - 1;
        f.bombs = dsmbl::kBombs;
        f.bombsHeld = 3;
        f.safeFrames = dsmbl::kSafeFrames;
        f.safeFramesHeld = 64;
        f.bossTunes.assign(std::begin(dsmbl::kBossTunes), std::end(dsmbl::kBossTunes));
        f.shipState = dsmbl::kShipState;
        f.beginWaitsFlown = true; // (Begin at the stage: through the map of stages)
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        return shmup::MakeGame(dsmbl::kCredits, f);
    }();
    return game;
}

} // namespace spelunk

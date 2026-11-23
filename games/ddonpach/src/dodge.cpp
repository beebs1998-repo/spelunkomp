// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi's Dodge training (runtime/dodge.h): the arena, and the game's own routines that make and aim the bullets.
//
// The arena is a boss's, without the boss. A game begun with the mode on is run on unseen to the boss of the stage it
// begins at (the training features' trip: runtime/shmup.h); as the boss's warning begins, every object of the stage is
// taken away and the task that brings the stage's objects in (the boss among them) is stopped from then on. What is
// left is the ship, the boss's background and the boss's life bar, full. The game's stage never ends.
// The patterns are data recorded from the game (work/ddonpach/dodge_patterns.inc, made by tools/dodge/extract.py, never
// committed): CMakeLists.txt defines SPELUNK_DODGE when the file is there, and without it the game has no Dodge training.
#if SPELUNK_DODGE
#include "../../../runtime/dodge.h"
#include "../../../runtime/native68k.h"
#include "../../../runtime/shmup.h"
#include "ddonpach.h"

#include <cstring>

namespace spelunk {

namespace {
    using namespace ddonpach;

#include "dodge_patterns.inc"

    // With no objects in the stage the enemies' records are nobody's (a record is free while its first byte is 0, and
    // is neither drawn nor tested then): the first is the emitter's, for its place (a bullet's parent is asked for the
    // long at +6 alone), and the mode's state is in the next two, their first words left 0
    constexpr uint32_t kEmitter = kEnemies, kState = kEnemies + 0x22;
    // Where the emitter is: 64 pixels down from the top of the screen, in the middle
    constexpr int kEmitterX = 256 * 64, kEmitterY = 120 * 64;

    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    void SetWord(Board &board, uint32_t address, int value) {
        board.ram[address & 0xFFFF] = static_cast<uint8_t>(value >> 8);
        board.ram[(address + 1) & 0xFFFF] = static_cast<uint8_t>(value);
    }
    bool ArenaUp(const Board &board) {
        return Word(board, kState) == 0xD0D6;
    }

    // One of the game's routines run from here, in the middle of the game's frame: every register as it was after
    struct Kept {
        M68K &c;
        uint32_t d[8], a[8];
        explicit Kept(M68K &cpu) : c(cpu) {
            std::memcpy(d, c.d, sizeof d);
            std::memcpy(a, c.a, sizeof a);
        }
        ~Kept() {
            std::memcpy(c.d, d, sizeof d);
            std::memcpy(c.a, a, sizeof a);
        }
    };

    void Fire(Board &board, int x, int y, int type, int speed, int angle, unsigned how) {
        M68K &c = board.cpu;
        const Kept kept(c);
        const bool fine = (how & dodge::kFine) != 0, loop2 = Word(board, kLoop) != 0;
        c.d[0] = (static_cast<uint32_t>(speed) & 0xFFFF) << 16 | static_cast<uint32_t>(type);
        c.d[1] = fine ? static_cast<uint32_t>(angle) : static_cast<uint32_t>(angle >> 2) & 0x3F;
        c.d[2] = (static_cast<uint32_t>(x) & 0xFFFF) << 16 | (static_cast<uint32_t>(y) & 0xFFFF);
        c.d[3] = 0;
        if (how & dodge::kParent) {
            c.d[3] = kEmitter;
            c.d[4] = (static_cast<uint32_t>(x - kEmitterX) & 0xFFFF) << 16 | (static_cast<uint32_t>(y - kEmitterY) & 0xFFFF);
        }
        c.CallGuest(fine ? (loop2 ? kMakeBulletFineLoop2 : kMakeBulletFine) : (loop2 ? kMakeBulletLoop2 : kMakeBullet));
    }

    int Aim(Board &board, int x, int y, bool fine) {
        // (at the ship flown: player 1's, or player 2's alone)
        uint32_t ship = 0;
        for (uint32_t s : {kShip2, kShip1}) {
            ship = (Word(board, s) & 0x8000) ? s : ship;
        }
        if (!ship) {
            return 128; // (no ship: straight down the screen)
        }
        M68K &c = board.cpu;
        const Kept kept(c);
        c.d[0] = static_cast<uint32_t>(x) & 0xFFFF;
        c.d[1] = static_cast<uint32_t>(y) & 0xFFFF;
        c.d[2] = static_cast<uint32_t>(Word(board, ship + 6));
        c.d[3] = static_cast<uint32_t>(Word(board, ship + 8));
        c.CallGuest(fine ? kAimFine : kAimCoarse);
        return static_cast<int>(fine ? c.d[1] & 0xFF : (c.d[1] & 0x3F) * 4);
    }

    // Every bullet's record free, as the game frees one (0513C0)
    void ClearBullets(Board &board) {
        for (int n = 0; n < kBulletCountLoop2; n++) {
            SetWord(board, kBullets + static_cast<uint32_t>(n) * kBulletSize, 0);
            SetWord(board, kBullets + static_cast<uint32_t>(n) * kBulletSize + 6, 0xFFFF);
        }
    }

    const dodge::Facts &Facts() {
        static const dodge::Facts facts = [] {
            dodge::Facts f;
            f.patterns = kDodgePatterns;
            f.patternCount = sizeof kDodgePatterns / sizeof kDodgePatterns[0];
            f.bullets = kDodgeBullets;
            f.sprites = kDodgeSprites;
            f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Hibachi"};
            f.emitterX = kEmitterX;
            f.emitterY = kEmitterY;
            f.state = kState;
            f.fire = Fire;
            f.aim = Aim;
            f.clear = [](Board &board, int loop) {
                ClearBullets(board);
                SetWord(board, kLoop, loop);
            };
            f.loop = [](Board &board) { return Word(board, kLoop); };
            return f;
        }();
        return facts;
    }

    // The arena: the stage's objects' tasks off (5 to 54: 048ADA), their records and the bullets free
    void RaiseArena(Board &board) {
        for (int n = 5; n <= 54; n++) {
            SetWord(board, kTasks + static_cast<uint32_t>(n) * 0x40, 0);
        }
        std::memset(&board.ram[kEnemies & 0xFFFF], 0, kEnemyCount * kEnemySize);
        std::memset(&board.ram[kEnemies2 & 0xFFFF], 0, kEnemy2Count * kEnemySize);
        ClearBullets(board);
        dodge::Begin(board, Facts());
        SetWord(board, kEmitter + 6, kEmitterX);
        SetWord(board, kEmitter + 8, kEmitterY);
    }

    // The emitter is drawn as what fires its pattern: the sprites the object drew as the pattern was recorded (a boss's:
    // all the boss's), put on the frame's sprite list as the game's objects put theirs (010CE4). A pattern with no
    // look, and the time before the first: the carrier a game's first ship is launched from (one sprite of 80 x 80
    // pixels), turned to face down the screen
    void DrawEmitter(M68K &c, Board &board) {
        uint32_t list = (static_cast<uint32_t>(Word(board, kSpriteList)) << 16) | static_cast<uint32_t>(Word(board, kSpriteList + 2));
        auto sprite = [&](int x, int y, uint32_t look, uint32_t number, uint32_t size) {
            // (as the board has them: 10 bits; one wholly off the picture is left out)
            if (x < -160 || x > 400 || y < -160 || y > 320) {
                return;
            }
            native::W16(c, list, look);
            native::W16(c, list + 2, number);
            native::W16(c, list + 4, static_cast<uint32_t>(x) & 0x3FF);
            native::W16(c, list + 6, static_cast<uint32_t>(y) & 0x3FF);
            native::W16(c, list + 8, size);
            list += 16;
        };
        int patterns[2];
        dodge::Showing(board, Facts(), patterns);
        bool drawn = false;
        for (int pattern : patterns) {
            if (pattern < 0) {
                continue;
            }
            const dodge::Pattern &p = kDodgePatterns[pattern];
            // (its colours into the palette: the game loads its enemies' colours stage by stage, and a boss's as it comes)
            for (int n = 0; n < p.coloursCount; n++) {
                const dodge::Colours &colours = kDodgeColours[p.firstColours + n];
                for (uint32_t i = 0; i < 16; i++) {
                    native::W16(c, kPalette + colours.colour * kSpriteColourBytes + i * 2, colours.entries[i]);
                }
            }
            for (int n = 0; n < p.spriteCount; n++) {
                const dodge::Sprite &s = kDodgeSprites[p.firstSprite + n];
                sprite(kEmitterX / 64 + s.dx, kEmitterY / 64 + s.dy, s.look, s.number, s.size);
                drawn = true;
            }
        }
        if (!drawn) {
            sprite(kEmitterX / 64 - 40, kEmitterY / 64 - 40, kCarrierLook | kSpriteTurned, kCarrierSprite, 0x0505);
        }
        SetWord(board, kSpriteList, static_cast<int>(list >> 16));
        SetWord(board, kSpriteList + 2, static_cast<int>(list & 0xFFFF));
    }

    size_t g_first = 0, g_toBoss = 0;
    Board *g_board = nullptr;
    void (*g_start)(Board &) = nullptr;
    void (*g_beginNow)() = nullptr;
    void (*g_before)(Board &, const std::vector<int> &) = nullptr;
} // namespace

void AddDodgeTraining(Game &game) {
    for (size_t i = 0; i < game.features.size(); i++) {
        g_toBoss = game.features[i].name == "to_boss" ? i : g_toBoss;
    }
    g_first = game.features.size();
    const std::vector<recomp::FeatureInfo> features = dodge::Features(Facts());
    game.features.insert(game.features.end(), features.begin(), features.end());
    g_start = game.start;
    game.start = [](Board &board) {
        g_start(board);
        g_board = &board;
        // The stage's objects are about to be brought in: with the arena up none are, and the emitter has its turn
        AddHook(shmup::HookAt(kStageObjects), "dodge training: the emitter for the stage's objects", [](M68K &c) {
            Board &board = *g_board;
            if (!ArenaUp(board)) {
                return false;
            }
            dodge::Step(board, Facts());
            DrawEmitter(c, board);
            Return(c);
            return true;
        });
    };
    g_before = game.beforeFrame;
    game.beforeFrame = [](Board &board, const std::vector<int> &values) {
        dodge::Choices(Facts(), &values[g_first], (board.system & kSysStart1) != 0);
        // (a game begun as the mode goes straight to its stage's boss: the training features' trip)
        std::vector<int> with = values;
        with[g_toBoss] = dodge::On() ? 1 : values[g_toBoss];
        g_before(board, with);
        if (board.frame < 1200) {
            return;
        }
        if (!ArenaUp(board)) {
            // (the trip there, and the boss's warning beginning: the game is in play, a ship flown)
            if (dodge::On() && board.featureWords[3] == 0 && Word(board, kBossOn) != 0 && Word(board, kMode) == static_cast<int>(kModePlay)) {
                RaiseArena(board);
            }
            return;
        }
        // A ship lost is counted and given back (a new game's three)
        uint8_t &ships = board.ram[kShips & 0xFFFF];
        if (ships < 3) {
            dodge::Hit(board, Facts());
            ships = 3;
        }
    };
    // The mode's row: a new game now, driven to its stage as Begin at the stage drives one, and on to the boss
    g_beginNow = game.beginNow;
    game.commands.push_back(dodge::BeginRow());
    game.command = [](int which) {
        dodge::SetOn(which == 0);
        if (which == 0) {
            g_beginNow();
        }
    };
    game.readout = [](const Board &board) { return dodge::Readout(board, Facts()); };
}

} // namespace spelunk

#else
#include "../../../runtime/game.h"

namespace spelunk {
void AddDodgeTraining(Game &) {}
} // namespace spelunk
#endif

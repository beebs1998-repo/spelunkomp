// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "dodge.h"

// (for the 68000 boards' games so far: DoDonPachi)
#ifdef SPELUNK_CAVE68K

#include <algorithm>
#include <cstdio>

namespace spelunk::dodge {

namespace {
    // ---- The frame's choices (the menu's: not part of the game's state) ----
    enum { kEveryPattern, kOnePattern, kScenario };
    enum { kWarmUp, kEnemies, kBosses, kHard, kCrossfire, kLastBoss, kGauntlet, kScenarios };
    struct Chosen {
        bool on = false, start = false;
        int fires = 0, stage = 0, number = 0, loop = 0, scenario = 0, frames = 0;
    } g;
    constexpr int kSeconds[] = {15, 30, 60, 10};
    constexpr int kGap = 45; // frames with nothing fired before a new pattern begins

    // ---- The state, in the game's memory at Facts::state (big-endian, as the game's own words) ----
    // (the two bytes at 30 are never written: DoDonPachi's are an object slot's first word, which must stay 0)
    enum : uint32_t {
        kMagic = 0,    // word: kBegun once the arena is up
        kSlot = 2,     // word: how far the turn-taking has come (the pattern's place in its list)
        kSlotTime = 4, // word: frames of the pattern in turn fired so far
        kGapLeft = 6,  // word: frames still to wait before firing
        kHits = 8,     // word
        kSinceHit = 10, // long: frames
        kStartHeld = 14, // byte
        kChannel0 = 16, kChannel1 = 32, // two patterns fired at once at most
        // in a channel:
        kPattern = 0,  // word: the pattern's number + 1 (0: none)
        kTime = 2,     // word: frames since its burst began
        kNext = 4,     // word: the next bullet of the burst
        kFirstAngle = 6, // byte: the angle the burst's first bullet was fired at
        kAims = 7,     // 4 bytes: the aims as the burst began (coarse and fine, from the bullet's place and the object's)
    };
    constexpr uint32_t kBegun = 0xD0D6;

    uint8_t &Byte(Board &board, uint32_t address) {
        return board.ram[address & (board.ram.size() - 1)];
    }
    uint32_t Read(const Board &board, uint32_t address, int bytes) {
        uint32_t value = 0;
        for (int i = 0; i < bytes; i++) {
            value = value << 8 | board.ram[(address + i) & (board.ram.size() - 1)];
        }
        return value;
    }
    void Write(Board &board, uint32_t address, uint32_t value, int bytes) {
        for (int i = 0; i < bytes; i++) {
            Byte(board, address + i) = static_cast<uint8_t>(value >> ((bytes - 1 - i) * 8));
        }
    }

    // ---- The patterns' lists ----
    struct Lists {
        std::vector<int> all[2];                   // a loop's, in the game's order
        std::vector<std::vector<int>> stage[2];    // ... by stage
        std::vector<int> byRank[2], enemies[2], bosses[2];
        size_t most = 1; // the most patterns any stage has
    };
    const Lists &ListsOf(const Facts &facts) {
        static Lists lists;
        static const Facts *made = nullptr;
        if (made != &facts) {
            made = &facts;
            lists = Lists();
            for (int loop = 0; loop < 2; loop++) {
                lists.stage[loop].resize(facts.stages.size());
            }
            for (size_t i = 0; i < facts.patternCount; i++) {
                const Pattern &p = facts.patterns[i];
                const int loop = p.loop ? 1 : 0;
                lists.all[loop].push_back(static_cast<int>(i));
                (p.boss ? lists.bosses : lists.enemies)[loop].push_back(static_cast<int>(i));
                if (p.stage < lists.stage[loop].size()) {
                    lists.stage[loop][p.stage].push_back(static_cast<int>(i));
                    lists.most = std::max(lists.most, lists.stage[loop][p.stage].size());
                }
            }
            for (int loop = 0; loop < 2; loop++) {
                lists.byRank[loop] = lists.all[loop];
                std::sort(lists.byRank[loop].begin(), lists.byRank[loop].end(),
                          [&](int a, int b) { return facts.patterns[a].rank < facts.patterns[b].rank; });
            }
        }
        return lists;
    }
    // (the same for a slot every time: a replay, a rewind and run-ahead see the same patterns)
    uint32_t Mix(uint32_t n) {
        n = (n ^ 61) ^ (n >> 16);
        n *= 9;
        n ^= n >> 4;
        n *= 0x27D4EB2D;
        return n ^ (n >> 15);
    }

    struct Pick {
        int pattern[2] = {-1, -1};
        int frames = 0; // how long before the next (0: for good)
    };
    // The stage's list in the loop chosen; a stage only the other loop has (the hidden boss's) is that loop's
    const std::vector<int> &StageList(const Lists &lists, int &loop, int stage) {
        stage = std::clamp(stage, 0, static_cast<int>(lists.stage[0].size()) - 1);
        if (lists.stage[loop][static_cast<size_t>(stage)].empty()) {
            loop ^= 1;
        }
        return lists.stage[loop][static_cast<size_t>(stage)];
    }
    Pick PickFor(const Facts &facts, uint32_t slot) {
        const Lists &lists = ListsOf(facts);
        Pick pick;
        int loop = g.loop ? 1 : 0;
        auto from = [&](const std::vector<int> &list, size_t at) { return list.empty() ? -1 : list[at % list.size()]; };
        const std::vector<int> &stage = StageList(lists, loop, g.stage);
        const int chosen = from(stage, static_cast<size_t>(g.number));
        pick.frames = g.frames;
        if (g.fires == kOnePattern) {
            pick.pattern[0] = chosen;
            pick.frames = 0;
        } else if (g.fires == kEveryPattern) {
            // (in the game's order, from the pattern chosen)
            const std::vector<int> &all = lists.all[loop];
            const size_t at = static_cast<size_t>(std::find(all.begin(), all.end(), chosen) - all.begin());
            pick.pattern[0] = from(all, (at < all.size() ? at : 0) + slot);
        } else {
            loop = g.loop ? 1 : 0;
            const std::vector<int> &ranked = lists.byRank[loop];
            const size_t n = ranked.size();
            switch (g.scenario) {
            case kWarmUp: pick.pattern[0] = from(ranked, Mix(slot) % std::max<size_t>(1, n / 5)); break;
            case kEnemies: pick.pattern[0] = from(lists.enemies[loop], slot); break;
            case kBosses: pick.pattern[0] = from(lists.bosses[loop], slot); break;
            case kHard: pick.pattern[0] = from(ranked, n - 1 - Mix(slot) % std::max<size_t>(1, n / 4)); break;
            case kCrossfire:
                // (a boss's with an enemy's from the easier half)
                pick.pattern[0] = from(lists.bosses[loop], Mix(slot));
                pick.pattern[1] = from(ranked, Mix(slot + 0x9E37) % std::max<size_t>(1, n / 2));
                break;
            case kLastBoss: {
                int last = 1;
                pick.pattern[0] = from(StageList(lists, last, static_cast<int>(facts.stages.size()) - 1), slot);
                break;
            }
            default: pick.pattern[0] = from(ranked, slot); break; // (the gauntlet: from the easiest up)
            }
        }
        return pick;
    }

    void FireDue(Board &board, const Facts &facts, uint32_t channel) {
        const uint32_t number = Read(board, channel + kPattern, 2);
        if (!number || number > facts.patternCount) {
            return;
        }
        const Pattern &p = facts.patterns[number - 1];
        uint32_t time = Read(board, channel + kTime, 2), next = Read(board, channel + kNext, 2);
        const Bullet *bullets = facts.bullets + p.first;
        if (time == 0 && p.count) {
            // (the aims as the burst begins, from its first bullet's place)
            const Bullet &b = bullets[0];
            for (int k = 0; k < 4; k++) {
                const bool object = (k & 2) != 0;
                Byte(board, channel + kAims + static_cast<uint32_t>(k)) = static_cast<uint8_t>(
                    facts.aim(board, facts.emitterX + (object ? b.ox : b.dx), facts.emitterY + (object ? b.oy : b.dy), (k & 1) != 0));
            }
        }
        while (next < p.count && bullets[next].dt <= time) {
            const Bullet &b = bullets[next];
            const int x = facts.emitterX + b.dx, y = facts.emitterY + b.dy;
            const bool object = (b.how & kFromObject) != 0, fine = (b.how & kFineAim) != 0;
            int angle = b.angle;
            switch (b.how & kHowMask) {
            case kWithFirst: angle += Byte(board, channel + kFirstAngle); break;
            case kAimEach: angle += facts.aim(board, object ? facts.emitterX + b.ox : x, object ? facts.emitterY + b.oy : y, fine); break;
            case kAimFirst: angle += Byte(board, channel + kAims + (object ? 2u : 0u) + (fine ? 1u : 0u)); break;
            default: break;
            }
            angle &= 0xFF;
            if (next == 0) {
                Byte(board, channel + kFirstAngle) = static_cast<uint8_t>(angle);
            }
            facts.fire(board, x, y, b.type, b.speed, angle, b.how);
            next++;
        }
        time++;
        // (the burst over and its pause too: the next burst)
        if (next >= p.count && time >= std::max<uint32_t>(p.pause, 20)) {
            time = next = 0;
        }
        Write(board, channel + kTime, time, 2);
        Write(board, channel + kNext, next, 2);
    }

    // A pattern's name: its stage, whether a boss's, and its number among the stage's
    std::string Name(const Facts &facts, int pattern) {
        const Lists &lists = ListsOf(facts);
        const Pattern &p = facts.patterns[pattern];
        const std::vector<int> &stage = lists.stage[p.loop ? 1 : 0][p.stage];
        const size_t at = static_cast<size_t>(std::find(stage.begin(), stage.end(), pattern) - stage.begin());
        char words[96];
        snprintf(words, sizeof words, "%s%s %zu/%zu", facts.stages[p.stage].c_str(), p.boss && p.stage + 1u < facts.stages.size() ? " BOSS" : "", at + 1,
                 stage.size());
        std::string name = words;
        for (char &ch : name) {
            ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
        }
        return p.loop ? "2-" + name : name;
    }
} // namespace

std::vector<recomp::FeatureInfo> Features(const Facts &facts) {
    const Lists &lists = ListsOf(facts);
    std::vector<std::string> numbers;
    for (size_t i = 1; i <= lists.most; i++) {
        numbers.push_back(std::to_string(i));
    }
    return {
        {"dodge_fires", kPage, "Fires",
         "Every pattern in turn: all of them in the game's order, from the one chosen below, each for the time chosen. One "
         "pattern: the one chosen below, over and over. A scenario: the set chosen below.",
         0, {"Every pattern in turn", "One pattern", "A scenario"}},
        {"dodge_stage", kPage, "Pattern's stage", "The stage whose enemies' and boss's patterns Pattern below counts through.", 0, facts.stages},
        {"dodge_pattern", kPage, "Pattern",
         "Which of the stage's patterns: its enemies' first, then its boss's. Its name is at the top of the picture while it is "
         "fired. A stage with fewer starts again from its first.",
         0, numbers},
        {"dodge_loop", kPage, "Loop",
         "The patterns as the game's first loop fires them, or as its second does: more bullets, faster, and the bullets "
         "enemies leave as they are destroyed.",
         0, {"First loop", "Second loop"}},
        {"dodge_scenario", kPage, "Scenario",
         "Warm-up: the easiest fifth of the patterns, in any order. Every enemy: the stages' enemies' patterns in the game's "
         "order. Every boss: the bosses'. Hard: the hardest quarter, in any order. Crossfire: a boss's pattern and an "
         "enemy's at once. The last boss: the hidden last boss's patterns. Gauntlet: every pattern, from the easiest to "
         "the hardest.",
         0, {"Warm-up", "Every enemy", "Every boss", "Hard", "Crossfire", "The last boss", "Gauntlet"}},
        {"dodge_time", kPage, "Each pattern for", "How long a pattern is fired before the next, when they are taken in turn.", 0,
         {"15 seconds", "30 seconds", "1 minute", "10 seconds"}},
    };
}

recomp::MenuCommand BeginRow() {
    return {kPage, "Begin Dodge training",
            "Your ship alone against an emitter at the top of the screen, where a boss would be, that fires the game's own enemy "
            "fire for you to dodge: every pattern the enemies and bosses have, taken from play. A new game now, as the mode: "
            "the board starts again and is run on unseen to a boss's arena (the stage chosen at Training mode's Start at "
            "stage), and the boss is not there. What is fired is chosen below; Start goes on to the next pattern. Ships never "
            "run out: each one lost is counted at the top of the picture. Reset, or Begin at the stage, leaves the mode."};
}

void Choices(const Facts &facts, const int *values, bool start) {
    (void)facts;
    g.fires = values[0];
    g.stage = values[1];
    g.number = values[2];
    g.loop = values[3];
    g.scenario = values[4];
    // (the game's frames: 57.44 a second)
    g.frames = kSeconds[std::clamp(values[5], 0, 3)] * 5744 / 100;
    g.start = start;
}

void SetOn(bool on) {
    g.on = on;
}

bool On() {
    return g.on;
}

void Begin(Board &board, const Facts &facts) {
    for (uint32_t i = 0; i < 44; i++) {
        if (i != 30 && i != 31) {
            Byte(board, facts.state + i) = 0;
        }
    }
    Write(board, facts.state + kMagic, kBegun, 2);
    Write(board, facts.state + kGapLeft, 90, 2);
}

void Hit(Board &board, const Facts &facts) {
    if (Read(board, facts.state + kMagic, 2) == kBegun) {
        Write(board, facts.state + kHits, std::min<uint32_t>(Read(board, facts.state + kHits, 2) + 1, 9999), 2);
        Write(board, facts.state + kSinceHit, 0, 4);
    }
}

void Step(Board &board, const Facts &facts) {
    const uint32_t s = facts.state;
    if (Read(board, s + kMagic, 2) != kBegun) {
        return;
    }
    uint32_t slot = Read(board, s + kSlot, 2), slotTime = Read(board, s + kSlotTime, 2), gap = Read(board, s + kGapLeft, 2);
    // Start, pressed: the next pattern (one pattern: it begins again)
    const bool held = Byte(board, s + kStartHeld) != 0;
    Byte(board, s + kStartHeld) = g.start ? 1 : 0;
    Pick pick = PickFor(facts, slot);
    bool next = g.start && !held;
    if (pick.frames && slotTime >= static_cast<uint32_t>(pick.frames)) {
        next = true;
    }
    if (next) {
        slot = (slot + 1) & 0xFFFF;
        slotTime = 0;
        pick = PickFor(facts, slot);
        // (the same pattern again, where there is one alone: from its beginning)
        Write(board, s + kChannel0 + kPattern, 0, 2);
    }
    // A pattern that is not the one being fired: it begins after a pause, in its own loop of the game
    for (int c = 0; c < 2; c++) {
        const uint32_t channel = s + (c ? kChannel1 : kChannel0);
        const uint32_t want = static_cast<uint32_t>(pick.pattern[c] + 1);
        if (Read(board, channel + kPattern, 2) != want) {
            Write(board, channel + kPattern, want, 2);
            Write(board, channel + kTime, 0, 2);
            Write(board, channel + kNext, 0, 2);
            gap = std::max<uint32_t>(gap, kGap);
            slotTime = 0;
        }
    }
    if (gap) {
        gap--;
        // (as the pause ends, with the last pattern's bullets mostly gone: the game's loop is the new pattern's)
        if (gap == 0 && pick.pattern[0] >= 0 && facts.loop(board) != facts.patterns[pick.pattern[0]].loop) {
            facts.clear(board, facts.patterns[pick.pattern[0]].loop);
        }
    } else {
        slotTime = std::min<uint32_t>(slotTime + 1, 0xFFFF);
        FireDue(board, facts, s + kChannel0);
        FireDue(board, facts, s + kChannel1);
    }
    Write(board, s + kSlot, slot, 2);
    Write(board, s + kSlotTime, slotTime, 2);
    Write(board, s + kGapLeft, gap, 2);
    Write(board, s + kSinceHit, Read(board, s + kSinceHit, 4) + 1, 4);
}

void Showing(const Board &board, const Facts &facts, int patterns[2]) {
    const uint32_t s = facts.state;
    for (int c = 0; c < 2; c++) {
        const uint32_t number = Read(board, s + (c ? kChannel1 : kChannel0) + kPattern, 2);
        patterns[c] = Read(board, s + kMagic, 2) == kBegun && number && number <= facts.patternCount ? static_cast<int>(number) - 1 : -1;
    }
}

std::string Readout(const Board &board, const Facts &facts) {
    const uint32_t s = facts.state;
    if (Read(board, s + kMagic, 2) != kBegun) {
        return "";
    }
    std::string words;
    for (uint32_t channel : {s + kChannel0, s + kChannel1}) {
        const uint32_t number = Read(board, channel + kPattern, 2);
        if (number && number <= facts.patternCount) {
            words += (words.empty() ? "" : " + ") + Name(facts, static_cast<int>(number) - 1);
        }
    }
    char more[64];
    snprintf(more, sizeof more, "   HITS %u   %u S", Read(board, s + kHits, 2), Read(board, s + kSinceHit, 4) * 100 / 5755);
    return words + more;
}

} // namespace spelunk::dodge

#endif

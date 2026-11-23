// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The PGM games' program ROMs are enciphered word by word, by the word's place: the board's IGS027A (an ARM7 with its
// own ROM) deciphers them as the 68000 reads. Which bits flip where are facts of the cartridges as MAME's pgmcrypt.cpp
// (BSD-3-Clause, David Haywood and ElSemi) records them: see THIRD_PARTY_NOTICES.md. The 256-byte table each game also
// takes is not here: it is worked out from the player's own program ROM as the set is loaded (SolveTable below).
#include "pgm_crypt.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace spelunk {

namespace {

// A test of a word's index (its place, counted in words from where the cipher begins): the index's bits under `mask`
// are, or are not, `value`. A test with no mask always holds.
struct Test {
    uint32_t mask = 0, value = 0;
    bool same = true;
    bool Holds(uint32_t index) const { return ((index & mask) == value) == same; }
};
constexpr Test Is(uint32_t mask, uint32_t value) { return {mask, value, true}; }
constexpr Test IsNot(uint32_t mask, uint32_t value) { return {mask, value, false}; }

// A bit of the word's low byte, flipped wherever both tests hold
struct Flip {
    uint8_t bit;
    Test test, also;
    Flip(uint8_t bit, Test test, Test also = {}) : bit(bit), test(test), also(also) {}
};

// A game's cipher: its flips (each bit of the low byte at most once) and whether the high byte is also exclusive-ored
// with a table's byte for the index's low byte
struct Cipher {
    const char *name;
    bool table;
    std::vector<Flip> flips;
};

const std::vector<Cipher> &Ciphers() {
    static const std::vector<Cipher> all = {
        // DoDonPachi Dai-Ou-Jou and its Black Label (the rules IGS used before them for Photo Y2K 2)
        {"ddp3", true, {
             {0x01, IsNot(0x040480, 0x000080)},
             {0x02, Is(0x084008, 0x084008)},
             {0x04, Is(0x000030, 0x000010), IsNot(0x180000, 0x080000)},
             {0x08, IsNot(0x000042, 0x000042)},
             {0x10, Is(0x008100, 0x008000)},
             {0x20, IsNot(0x022004, 0x000004)},
             {0x40, IsNot(0x011800, 0x010000)},
             {0x80, Is(0x004820, 0x004820)},
         }},
        // Ketsui
        {"ket", true, {
             {0x01, IsNot(0x040480, 0x000080)},
             {0x02, Is(0x004008, 0x004008)},
             {0x04, Is(0x080030, 0x000010)},
             {0x08, IsNot(0x000042, 0x000042)},
             {0x10, Is(0x008100, 0x008000)},
             {0x20, IsNot(0x002004, 0x000004)},
             {0x40, IsNot(0x011800, 0x010000)},
             {0x80, Is(0x000820, 0x000820)},
         }},
        // Espgaluda
        {"espgal", true, {
             {0x01, IsNot(0x040480, 0x000080)},
             {0x02, Is(0x084008, 0x084008)},
             {0x04, Is(0x000030, 0x000010)},
             {0x08, IsNot(0x000042, 0x000042)},
             {0x10, Is(0x048100, 0x048000)},
             {0x20, IsNot(0x022004, 0x000004)},
             {0x40, IsNot(0x011800, 0x010000)},
             {0x80, Is(0x000820, 0x000820)},
         }},
        // DoDonPachi II's cartridge ROM of ARM code (not the 68000's program, which is plain). Its high bytes are seen
        // to by a table the IGS027A's own ROM writes as it starts, which is run (machine/pgm.h): no table here.
        {"ddp2", false, {
             {0x01, IsNot(0x000480, 0x000080)},
             {0x08, IsNot(0x000042, 0x000042)},
             {0x10, Is(0x008100, 0x008000)},
             {0x20, IsNot(0x002004, 0x000004)},
             {0x40, IsNot(0x001800, 0x000000)},
             {0x80, Is(0x000820, 0x000820)},
         }},
    };
    return all;
}

using Table = std::array<uint8_t, 256>;

// SHA-1 (FIPS 180-4) of a few bytes, as 40 lowercase hex digits
std::string Sha1(const uint8_t *data, size_t size) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::vector<uint8_t> m(data, data + size);
    m.push_back(0x80);
    while (m.size() % 64 != 56) {
        m.push_back(0);
    }
    const uint64_t bits = static_cast<uint64_t>(size) * 8;
    for (int s = 56; s >= 0; s -= 8) {
        m.push_back(static_cast<uint8_t>(bits >> s));
    }
    auto rotl = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    for (size_t at = 0; at < m.size(); at += 64) {
        uint32_t w[80];
        for (int t = 0; t < 16; t++) {
            w[t] = static_cast<uint32_t>(m[at + t * 4]) << 24 | m[at + t * 4 + 1] << 16 | m[at + t * 4 + 2] << 8 | m[at + t * 4 + 3];
        }
        for (int t = 16; t < 80; t++) {
            w[t] = rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int t = 0; t < 80; t++) {
            uint32_t f, k;
            if (t < 20) {
                f = (b & c) | (~b & d), k = 0x5A827999;
            } else if (t < 40) {
                f = b ^ c ^ d, k = 0x6ED9EBA1;
            } else if (t < 60) {
                f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d, k = 0xCA62C1D6;
            }
            const uint32_t next = rotl(a, 5) + f + e + k + w[t];
            e = d, d = c, c = rotl(b, 30), b = a, a = next;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    char text[41];
    for (int n = 0; n < 5; n++) {
        snprintf(text + n * 8, 9, "%08x", h[n]);
    }
    return text;
}

// The table, from the enciphered words alone. The flips touch only the low byte and the table only the high, so a
// run of 256 words (from an index whose low byte is 0) whose plain high bytes are all one value F has the table, each
// byte exclusive-ored with F, for its high bytes. Programs have many such runs: stretches of zeros, and of words below
// 0100 (small numbers, offsets, ASCII); and of all ones. So: the high bytes of every run but those that are one value
// throughout (left plain: no information), the most repeated of them as a first guess, and then each byte of the table
// the value most of the runs close to that guess (or to its complement: the runs of ones) have there.
bool SolveTable(const uint16_t *words, size_t count, Table &table, std::string &how) {
    std::vector<Table> runs;
    for (size_t at = 0; at + 256 <= count; at += 256) {
        Table high;
        for (size_t j = 0; j < 256; j++) {
            high[j] = static_cast<uint8_t>(words[at + j] >> 8);
        }
        if (std::count(high.begin(), high.end(), high[0]) != 256) {
            runs.push_back(high);
        }
    }
    if (runs.empty()) {
        return false;
    }
    std::vector<Table> sorted = runs;
    std::sort(sorted.begin(), sorted.end());
    size_t best = 0, bestCount = 0;
    for (size_t i = 0; i < sorted.size();) {
        size_t j = i;
        while (j < sorted.size() && sorted[j] == sorted[i]) {
            j++;
        }
        if (j - i > bestCount) {
            best = i, bestCount = j - i;
        }
        i = j;
    }
    const Table guess = sorted[best];
    std::vector<std::array<uint16_t, 256>> votes(256);
    for (auto &v : votes) {
        v.fill(0);
    }
    size_t used = 0;
    for (const Table &run : runs) {
        for (const uint8_t fill : {uint8_t{0x00}, uint8_t{0xFF}}) {
            int differ = 0;
            for (size_t j = 0; j < 256; j++) {
                differ += (run[j] ^ fill) != guess[j];
            }
            if (differ <= 32) {
                for (size_t j = 0; j < 256; j++) {
                    votes[j][run[j] ^ fill]++;
                }
                used++;
                break;
            }
        }
    }
    int margin = 1 << 30;
    for (size_t j = 0; j < 256; j++) {
        const auto &v = votes[j];
        const auto top = std::max_element(v.begin(), v.end());
        table[j] = static_cast<uint8_t>(top - v.begin());
        int second = 0;
        for (size_t b = 0; b < 256; b++) {
            if (b != table[j]) {
                second = std::max<int>(second, v[b]);
            }
        }
        margin = std::min(margin, *top - second);
    }
    char text[160];
    snprintf(text, sizeof text, "%zu runs, the first guess %zu times, %zu runs voted, the narrowest byte won by %d", runs.size(), bestCount,
             used, margin);
    how = text;
    return margin > 0;
}

} // namespace

bool PgmDecipher(const std::string &scheme, uint16_t *words, size_t count, const std::string &check, std::string &error) {
    const auto &all = Ciphers();
    const auto cipher = std::find_if(all.begin(), all.end(), [&](const Cipher &c) { return scheme == c.name; });
    if (cipher == all.end()) {
        error = "game.cfg names a cipher this build does not know: " + scheme;
        return false;
    }
    Table table{};
    if (cipher->table) {
        std::string how;
        const bool solved = SolveTable(words, count, table, how);
        std::string sha = Sha1(table.data(), table.size());
        if (solved && sha != check) {
            // (a program with more runs of ones than of plain low values: the guess was the table's complement)
            Table other = table;
            for (uint8_t &b : other) {
                b ^= 0xFF;
            }
            if (Sha1(other.data(), other.size()) == check) {
                table = other, sha = check;
            }
        }
        if (getenv("SPELUNK_KEY_LOG")) {
            fprintf(stderr, "cipher %s: %s; table SHA-1 %s (%s)\n", scheme.c_str(), how.c_str(), sha.c_str(),
                    sha == check ? "as game.cfg has it" : "NOT as game.cfg has it");
        }
        if (check.empty()) {
            error = "game.cfg gives no cipher_sha1 for the " + scheme + " cipher's table (the one worked out from this set has SHA-1 " + sha + ")";
            return false;
        }
        if (!solved || sha != check) {
            error = "the program's cipher table could not be worked out from this set (game.cfg's cipher_sha1 is " + check + ", the table found " +
                    (solved ? "has " + sha : "was no clear winner") + "): is the program ROM the right one and whole?";
            return false;
        }
    }
    // (a pass for each rule: the compiler makes each a few vector instructions)
    for (uint32_t i = 0; i < count; i++) {
        words[i] ^= static_cast<uint16_t>(table[i & 0xFF] << 8);
    }
    for (const Flip &f : cipher->flips) {
        const Test a = f.test, b = f.also;
        const uint16_t bit = f.bit;
        for (uint32_t i = 0; i < count; i++) {
            words[i] ^= (a.Holds(i) && b.Holds(i)) ? bit : 0;
        }
    }
    return true;
}

} // namespace spelunk

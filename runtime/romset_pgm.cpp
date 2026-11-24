// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A PGM game's set: each of the board's regions (the program, the tiles, the sprites' pixels and masks, the samples)
// made of files at offsets in it, as MAME's pgm.cpp lays them out; the program then deciphered; the battery-backed
// RAM as the factory left it, where the set has it.
#include "../machine/pgm.h"
#include "../machine/pgm_crypt.h"
#include "romset.h"
#include "romset_source.h"

#include <sstream>

namespace spelunk {

using namespace romset;

bool LoadSet(const std::string &where, const GameConfig &config, Pgm &board, std::string &error) {
    auto value = [&](const char *key) {
        const auto it = config.find(key);
        return it == config.end() ? std::string() : it->second;
    };
    auto number = [&](const char *key, uint32_t otherwise) {
        const std::string v = value(key);
        return v.empty() ? otherwise : static_cast<uint32_t>(strtoul(v.c_str(), nullptr, 16));
    };
    const std::string set = value("rom_set");
    const std::vector<std::string> places = Places(where, set, value("rom_parent"));
    std::string missing, first;
    for (const std::string &place : places) {
        Source source;
        if (!source.Open(place)) {
            continue;
        }
        source.OpenParents(place, where, value("rom_parent"));
        // (the message tells of the first place that has some of the set: later ones are tried in case)
        if (first.empty()) {
            first = missing;
        }
        missing.clear();
        // A region: `key` lists name:crc@offset (hex), each file put at its offset
        auto region = [&](const char *key, std::vector<uint8_t> &into) {
            std::istringstream files(value(key));
            std::string one, name;
            uint32_t crc = 0;
            while (missing.empty() && files >> one) {
                const size_t at = one.find('@');
                const uint32_t offset = at == std::string::npos ? 0 : static_cast<uint32_t>(strtoul(one.c_str() + at + 1, nullptr, 16));
                std::vector<uint8_t> data;
                if (!ParseFile(one.substr(0, at), name, crc) || !source.FindAny(crc, data)) {
                    missing = name;
                    break;
                }
                const size_t size = std::min(data.size(), into.size() > offset ? into.size() - offset : 0);
                std::copy(data.begin(), data.begin() + static_cast<ptrdiff_t>(size), into.begin() + offset);
            }
            return missing.empty();
        };
        // The program: the 68000's words, which the dumps hold low byte first; deciphered from where the game begins
        // (its table, for the games that take one, worked out from these words: machine/pgm_crypt.h)
        std::vector<uint8_t> program(0x600000, 0);
        if (!region("program", program)) {
            continue;
        }
        {
            const std::string cipher = value("cipher");
            const size_t at = cipher.find('@');
            const uint32_t from = at == std::string::npos ? 0 : static_cast<uint32_t>(strtoul(cipher.c_str() + at + 1, nullptr, 16));
            std::vector<uint16_t> words(0x200000);
            for (size_t n = 0; n < words.size(); n++) {
                words[n] = static_cast<uint16_t>(program[from + n * 2] | program[from + n * 2 + 1] << 8);
            }
            if (!cipher.empty() && !PgmDecipher(cipher.substr(0, at), words.data(), words.size(), value("cipher_sha1"), error)) {
                error = "the " + set + " set: " + error;
                return false;
            }
            for (size_t n = 0; n < words.size(); n++) {
                program[from + n * 2] = static_cast<uint8_t>(words[n]);
                program[from + n * 2 + 1] = static_cast<uint8_t>(words[n] >> 8);
            }
            // (the board's memory is big-endian; only the first 4 MB is seen)
            SwapWords(program.data(), program.size());
            board.program.assign(program.begin(), program.begin() + 0x400000);
        }
        // The tiles (bytes); the sprites' pixels and masks (16-bit words, low byte first), each as large as its region
        std::vector<uint8_t> tiles(number("tiles_size", 0xA00000), 0);
        std::vector<uint8_t> pixels(number("sprite_pixels_size", 0x1000000), 0), masks(number("sprite_masks_size", 0x1000000), 0);
        if (!region("tiles", tiles) || !region("sprite_pixels", pixels) || !region("sprite_masks", masks)) {
            continue;
        }
        board.video.tiles = std::move(tiles);
        auto words = [](const std::vector<uint8_t> &bytes, std::vector<uint16_t> &into) {
            into.resize(bytes.size() / 2);
            for (size_t n = 0; n < into.size(); n++) {
                into[n] = static_cast<uint16_t>(bytes[n * 2] | bytes[n * 2 + 1] << 8);
            }
        };
        words(pixels, board.video.spritePixels);
        words(masks, board.video.spriteMasks);
        std::fill(board.soundRom.begin(), board.soundRom.end(), 0);
        if (!region("samples", board.soundRom)) {
            continue;
        }
        // The battery-backed work RAM as the factory left it (16-bit words, low byte first): the settings, and for some
        // sets values the game will not start without
        std::fill(board.ram.begin(), board.ram.end(), 0);
        if (!value("nvram").empty()) {
            if (!region("nvram", board.ram)) {
                continue;
            }
            SwapWords(board.ram.data(), board.ram.size());
        }
        // Words of it set otherwise for this game's entry (`nvram_set = ADDRESS:WORD ...`, hex)
        {
            std::istringstream sets(value("nvram_set"));
            std::string one;
            while (sets >> one) {
                const uint32_t address = static_cast<uint32_t>(strtoul(one.c_str(), nullptr, 16)) & 0x1FFFE;
                const size_t colon = one.find(':');
                const uint32_t word = colon == std::string::npos ? 0 : static_cast<uint32_t>(strtoul(one.c_str() + colon + 1, nullptr, 16));
                board.ram[address] = static_cast<uint8_t>(word >> 8);
                board.ram[address + 1] = static_cast<uint8_t>(word);
            }
        }
        board.protection = number("protection", 0x500000);
        // An IGS027A whose ROM has been read (DoDonPachi II): the chip's own ROM and the cartridge's ROM of ARM code,
        // the second deciphered by the fixed rules; the ARM itself is then run (machine/pgm.h) and nothing is answered
        // in its place
        if (!value("arm_internal").empty()) {
            std::vector<uint8_t> internal(0x4000, 0), external(number("arm_external_size", 0x400000), 0);
            if (!region("arm_internal", internal) || !region("arm_external", external)) {
                continue;
            }
            std::vector<uint16_t> code(external.size() / 2);
            for (size_t n = 0; n < code.size(); n++) {
                code[n] = static_cast<uint16_t>(external[n * 2] | external[n * 2 + 1] << 8);
            }
            const std::string cipher = value("arm_cipher");
            if (!cipher.empty() && !PgmDecipher(cipher, code.data(), code.size(), "", error)) {
                error = "the " + set + " set: " + error;
                return false;
            }
            for (size_t n = 0; n < code.size(); n++) {
                external[n * 2] = static_cast<uint8_t>(code[n]);
                external[n * 2 + 1] = static_cast<uint8_t>(code[n] >> 8);
            }
            board.armRom = std::move(internal);
            board.armExternal = std::move(external);
            board.hasArm = true;
            board.protection = 0xFF000000; // (no address of the 68000's)
        }
        board.dips = static_cast<uint16_t>(number("dips", 0xFF));
        board.Reset();
        return true;
    }
    missing = first.empty() ? missing : first;
    error = missing.empty() ? "no " + set + " set in " + where : "the " + set + " set in " + where + " has no " + missing + " with the checksum expected" + ParentsNeeded(value("rom_parent"));
    return false;
}

} // namespace spelunk

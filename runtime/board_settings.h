// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The board's own settings (the Board page) and autofire (the Controls page). These boards have no DIP switches: a
// game keeps its settings (difficulty, lives, extends) where its test menu writes them, the board's EEPROM (the
// CV1000's RTC-9701, the 93C46 of Cave's 68000 board) or the PGM's battery-backed work RAM. game.cfg declares them,
// as cps2recomp's board options are declared (docs/TOOLKIT.md):
//
//   board_option_N = name | Title | where | description | Choice=value | Choice=value ...
//
// `where` is `eeprom W:low-high` (the EEPROM's 16-bit word W, bits low to high; `eeprom W` the whole word) or
// `ram ADDRESS.b:low-high` / `ram ADDRESS.w:low-high` (the battery RAM's byte or word at the 68000's address, hex).
// Values are as the bits hold them (decimal, or 0x hex), shifted into place. The first choice, "As the board is set",
// writes nothing. A game that checksums its settings says how: `board_checksum = eeprom W = sum A-B` (word W is the
// 16-bit sum of words A to B), recomputed the game's own way after a write.
//
// The settings are put in as the board is made (power on, Reset), before the game reads them; the saves folder's copy
// keeps the board's own (the test menu's) values, so "As the board is set" is the board again.
#pragma once

#include "../recomp-common/settings.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spelunk {

struct BoardSetting {
    std::string name, title, description;
    bool ram = false;      // (false: the EEPROM's word `at`; true: the battery RAM at 68000 address `at`)
    uint32_t at = 0;
    unsigned bytes = 2;    // (ram: 1 for .b, 2 for .w)
    int low = 0, high = 15;
    std::vector<std::string> names;
    std::vector<uint32_t> values;
};
struct BoardChecksum {
    uint32_t word = 0, from = 0, to = 0;
};

// game.cfg's (the built-in one); with `problem` saying what is wrong with a declaration that could not be read
const std::vector<BoardSetting> &BoardSettings();
const std::vector<BoardChecksum> &BoardChecksums();
const std::string &BoardSettingsProblem();
// ... read from a game.cfg's text (the checks: tools/check_board_settings.cpp)
bool ParseBoardSettings(const std::string &configText, std::vector<BoardSetting> &settings, std::vector<BoardChecksum> &checksums,
                        std::string &problem);

// The features: the Board page's (one per setting, choice 0 "As the board is set") and the Controls page's
// autofire, one per button game.cfg names
std::vector<recomp::FeatureInfo> BoardSettingFeatures();
std::vector<recomp::FeatureInfo> AutofireFeatures();

// Where a board keeps its settings: the EEPROM's words, or the battery RAM's bytes (big-endian, from `ramBase`)
struct SettingsStore {
    uint16_t *words = nullptr;
    size_t wordCount = 0;
    uint8_t *bytes = nullptr;
    size_t byteCount = 0;
    uint32_t ramBase = 0;
};
uint32_t SettingValue(const SettingsStore &store, const BoardSetting &setting);
void SetSettingValue(SettingsStore &store, const BoardSetting &setting, uint32_t value);
void FixBoardChecksums(SettingsStore &store);
// Nothing written yet (a CV1000's EEPROM before the game's first power on: all FFFF; a PGM's RAM with no factory
// copy: all 0): the game will write its own there first
bool StoreBlank(const SettingsStore &store);

// The board's settings in play: what the board itself holds (asSet, the test menu's) and what was put in over it at the
// board's last start (`applied`: each setting's choice then, 0 for none)
class BoardSettingsInPlay {
public:
    // At power on, the store as loaded (the saves folder's or the set's)
    void Loaded(const SettingsStore &store);
    // As the board is made: the choices put in (`features`: every feature's value, by AllFeatures' order)
    void Apply(SettingsStore &store, const std::vector<int> &features);
    // What the saves folder keeps: the store with the board's own values where a choice was put in (and its checksum)
    std::vector<uint8_t> Kept(const SettingsStore &store);
    // Whether any setting is chosen now
    static bool AnyChosen(const std::vector<int> &features);

private:
    std::vector<uint32_t> m_asSet;
    std::vector<int> m_applied;
    void Learn(const SettingsStore &store); // (the store's values of the settings not put in, as the board's own)
};

// Autofire: the buttons pressed and let go by turns while held, by the frame's number (so the same in rollback,
// replay and run-ahead). `bits`: a player's stick and buttons (machine/inputs.h)
uint32_t Autofire(uint32_t bits, uint64_t frame, const std::vector<int> &features);

} // namespace spelunk

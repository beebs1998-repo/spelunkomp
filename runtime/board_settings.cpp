// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The board's own settings from game.cfg, and autofire (runtime/board_settings.h)
#include "board_settings.h"

#include "../machine/inputs.h"
#include "game.h"
#include "romset.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace spelunk {

namespace {
    constexpr int kMostSettings = 24;
    constexpr int kMostButtons = 4;
    constexpr const char *kAsSet = "As the board is set";

    std::string Trimmed(const std::string &s) {
        const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }
    std::vector<std::string> Fields(const std::string &line, char bar) {
        std::vector<std::string> fields;
        for (size_t at = 0; at <= line.size();) {
            const size_t end = std::min(line.find(bar, at), line.size());
            fields.push_back(Trimmed(line.substr(at, end - at)));
            at = end + 1;
        }
        return fields;
    }
    // A whole number, decimal or 0x hex, and nothing after it
    bool Number(const std::string &text, uint32_t &value, int base = 0) {
        if (text.empty()) {
            return false;
        }
        char *end = nullptr;
        const unsigned long v = strtoul(text.c_str(), &end, base);
        if (*end || v > 0xFFFFFFFFul) {
            return false;
        }
        value = static_cast<uint32_t>(v);
        return true;
    }
    // "low-high" or "bit" after the colon (none: every bit of the word or byte)
    bool Bits(const std::string &text, int width, int &low, int &high) {
        if (text.empty()) {
            low = 0;
            high = width - 1;
            return true;
        }
        const size_t dash = text.find('-');
        uint32_t a = 0, b = 0;
        if (!Number(text.substr(0, dash), a, 10) || (dash != std::string::npos && !Number(text.substr(dash + 1), b, 10))) {
            return false;
        }
        low = static_cast<int>(a);
        high = dash == std::string::npos ? low : static_cast<int>(b);
        return low <= high && high < width;
    }

    bool ParseSetting(const std::string &text, BoardSetting &setting, std::string &problem) {
        const std::vector<std::string> fields = Fields(text, '|');
        setting.name = fields[0];
        const std::string what = "game.cfg's board option " + (setting.name.empty() ? "\"" + text + "\"" : setting.name);
        if (fields.size() < 5 || setting.name.empty() || fields[1].empty()) {
            problem = what + ": name | title | where | description | at least one choice=value";
            return false;
        }
        setting.title = fields[1];
        setting.description = fields[3];
        // where: "eeprom W[:bits]" or "ram ADDRESS.b|.w[:bits]"
        const std::string &where = fields[2];
        const size_t space = where.find(' ');
        const std::string kind = where.substr(0, space), place = space == std::string::npos ? "" : Trimmed(where.substr(space + 1));
        const size_t colon = place.find(':');
        const std::string at = place.substr(0, colon), bits = colon == std::string::npos ? "" : place.substr(colon + 1);
        bool ok = false;
        if (kind == "eeprom") {
            setting.ram = false;
            setting.bytes = 2;
            ok = Number(at, setting.at, 10) && setting.at < 256 && Bits(bits, 16, setting.low, setting.high);
        } else if (kind == "ram") {
            setting.ram = true;
            const size_t dot = at.find('.');
            const std::string size = dot == std::string::npos ? "" : at.substr(dot + 1);
            setting.bytes = size == "b" ? 1 : size == "w" ? 2 : 0;
            ok = setting.bytes && Number(at.substr(0, dot), setting.at, 16) && setting.at % setting.bytes == 0 &&
                 Bits(bits, static_cast<int>(setting.bytes) * 8, setting.low, setting.high);
        }
        if (!ok) {
            problem = what + ": where is \"eeprom W:low-high\" (a word 0 to 255) or \"ram ADDRESS.b:low-high\" (or .w; the address in hex), "
                             "not \"" + where + "\"";
            return false;
        }
        const uint64_t limit = 1ull << (setting.high - setting.low + 1);
        for (size_t n = 4; n < fields.size(); n++) {
            const size_t eq = fields[n].rfind('=');
            uint32_t value = 0;
            if (eq == std::string::npos || eq == 0 || !Number(Trimmed(fields[n].substr(eq + 1)), value) || value >= limit) {
                problem = what + ": the choice \"" + fields[n] + "\" is not name=value, with a value its bits can hold";
                return false;
            }
            setting.names.push_back(Trimmed(fields[n].substr(0, eq)));
            setting.values.push_back(value);
        }
        return true;
    }

    bool ParseChecksum(const std::string &text, BoardChecksum &sum, std::string &problem) {
        // "eeprom W = sum A-B"
        uint32_t w = 0, a = 0, b = 0;
        char tail = 0;
        if (sscanf(text.c_str(), " eeprom %u = sum %u - %u %c", &w, &a, &b, &tail) != 3 || w > 255 || b > 255 || a > b) {
            problem = "game.cfg's board_checksum \"" + text + "\": eeprom W = sum A-B (words 0 to 255)";
            return false;
        }
        sum = {w, a, b};
        return true;
    }

    struct Parsed {
        std::vector<BoardSetting> settings;
        std::vector<BoardChecksum> checksums;
        std::string problem;
    };
    const Parsed &BuiltIn() {
        static const Parsed parsed = [] {
            Parsed p;
            if (!ParseBoardSettings(BuiltInGameConfigText(), p.settings, p.checksums, p.problem)) {
                p.settings.clear();
                p.checksums.clear();
            }
            return p;
        }();
        return parsed;
    }

    uint16_t *WordAt(SettingsStore &store, uint32_t word) {
        return word < store.wordCount ? &store.words[word] : nullptr;
    }
    // (the battery RAM's bytes at a 68000 address: a byte, or a word high byte first)
    bool RamAt(const SettingsStore &store, uint32_t address, unsigned bytes, size_t &offset) {
        if (!store.bytes || address < store.ramBase) {
            return false;
        }
        offset = address - store.ramBase;
        return offset + bytes <= store.byteCount;
    }

    // The autofire features' places in AllFeatures (none past its end), button by button
    const std::vector<size_t> &AutofireAt() {
        static const std::vector<size_t> at = [] {
            std::vector<size_t> list;
            for (int b = 0; b < kMostButtons; b++) {
                list.push_back(FeatureIndex("autofire_" + std::to_string(b + 1)));
            }
            return list;
        }();
        return at;
    }
    const std::vector<size_t> &SettingAt() {
        static const std::vector<size_t> at = [] {
            std::vector<size_t> list;
            for (const BoardSetting &setting : BoardSettings()) {
                list.push_back(FeatureIndex(setting.name));
            }
            return list;
        }();
        return at;
    }
    int Choice(const std::vector<int> &features, size_t at, size_t choices) {
        const int v = at < features.size() ? features[at] : 0;
        return v > 0 && static_cast<size_t>(v) <= choices ? v : 0;
    }
} // namespace

bool ParseBoardSettings(const std::string &configText, std::vector<BoardSetting> &settings, std::vector<BoardChecksum> &checksums,
                        std::string &problem) {
    settings.clear();
    checksums.clear();
    GameConfig config;
    ParseGameConfig(configText, config);
    for (int n = 1; n <= kMostSettings; n++) {
        const auto it = config.find("board_option_" + std::to_string(n));
        if (it == config.end()) {
            // (consecutive from 1: one past a gap is a mistake)
            for (int m = n + 1; m <= kMostSettings; m++) {
                if (config.count("board_option_" + std::to_string(m))) {
                    problem = "game.cfg: board_option_" + std::to_string(m) + " with no board_option_" + std::to_string(n);
                    return false;
                }
            }
            break;
        }
        BoardSetting setting;
        if (!ParseSetting(it->second, setting, problem)) {
            return false;
        }
        for (const BoardSetting &other : settings) {
            if (other.name == setting.name) {
                problem = "game.cfg: two board options named " + setting.name;
                return false;
            }
        }
        settings.push_back(setting);
    }
    if (config.count("board_checksum")) {
        for (const std::string &one : Fields(config["board_checksum"], ',')) {
            BoardChecksum sum;
            if (!ParseChecksum(one, sum, problem)) {
                return false;
            }
            checksums.push_back(sum);
        }
    }
    return true;
}

const std::vector<BoardSetting> &BoardSettings() {
    return BuiltIn().settings;
}
const std::vector<BoardChecksum> &BoardChecksums() {
    return BuiltIn().checksums;
}
const std::string &BoardSettingsProblem() {
    return BuiltIn().problem;
}

std::vector<recomp::FeatureInfo> BoardSettingFeatures() {
    std::vector<recomp::FeatureInfo> features;
    for (const BoardSetting &setting : BoardSettings()) {
        recomp::FeatureInfo f{setting.name, "Board", setting.title,
                              setting.description + (setting.description.empty() ? "" : " ") +
                                  "As the board is set: the board's own, as its test menu (F2) sets it. A choice here is put in "
                                  "as the board starts (and at Reset), in place of the test menu's, and the board keeps its own. "
                                  "Online, the host's.",
                              recomp::kFeatureOwn, {kAsSet}};
        f.choices.insert(f.choices.end(), setting.names.begin(), setting.names.end());
        features.push_back(f);
    }
    return features;
}

std::vector<recomp::FeatureInfo> AutofireFeatures() {
    std::vector<recomp::FeatureInfo> features;
    const std::vector<std::string> buttons = [] {
        std::vector<std::string> names;
        const std::string text = GameConfigValue("buttons");
        if (!text.empty()) {
            names = Fields(text, '|');
        }
        return names;
    }();
    for (size_t b = 0; b < buttons.size() && b < static_cast<size_t>(kMostButtons); b++) {
        features.push_back({"autofire_" + std::to_string(b + 1), "Controls", "Autofire: " + buttons[b],
                            "While " + buttons[b] + " is held, it is pressed and let go again and again, this many times a second (on "
                            "the board's frames: 30 is every other frame), as a cabinet's autofire board did. Off: as held. The "
                            "same in a replay, rewind and run-ahead; online, each player's own, sent as pressed.",
                            0, {"Off", "30 a second", "20 a second", "15 a second"}});
    }
    return features;
}

uint32_t SettingValue(const SettingsStore &store, const BoardSetting &setting) {
    uint32_t raw = 0;
    if (setting.ram) {
        size_t offset = 0;
        if (!RamAt(store, setting.at, setting.bytes, offset)) {
            return 0;
        }
        raw = setting.bytes == 1 ? store.bytes[offset] : static_cast<uint32_t>(store.bytes[offset] << 8 | store.bytes[offset + 1]);
    } else {
        if (setting.at >= store.wordCount) {
            return 0;
        }
        raw = store.words[setting.at];
    }
    return (raw >> setting.low) & ((1u << (setting.high - setting.low + 1)) - 1);
}

void SetSettingValue(SettingsStore &store, const BoardSetting &setting, uint32_t value) {
    const uint32_t mask = ((1u << (setting.high - setting.low + 1)) - 1) << setting.low;
    auto put = [&](uint32_t raw) { return (raw & ~mask) | ((value << setting.low) & mask); };
    if (setting.ram) {
        size_t offset = 0;
        if (!RamAt(store, setting.at, setting.bytes, offset)) {
            return;
        }
        if (setting.bytes == 1) {
            store.bytes[offset] = static_cast<uint8_t>(put(store.bytes[offset]));
        } else {
            const uint32_t word = put(static_cast<uint32_t>(store.bytes[offset] << 8 | store.bytes[offset + 1]));
            store.bytes[offset] = static_cast<uint8_t>(word >> 8);
            store.bytes[offset + 1] = static_cast<uint8_t>(word);
        }
    } else if (uint16_t *word = WordAt(store, setting.at)) {
        *word = static_cast<uint16_t>(put(*word));
    }
}

void FixBoardChecksums(SettingsStore &store) {
    for (const BoardChecksum &sum : BoardChecksums()) {
        if (sum.word >= store.wordCount || sum.to >= store.wordCount) {
            continue;
        }
        uint32_t total = 0;
        for (uint32_t w = sum.from; w <= sum.to; w++) {
            total += store.words[w];
        }
        store.words[sum.word] = static_cast<uint16_t>(total);
    }
}

bool StoreBlank(const SettingsStore &store) {
    for (size_t w = 0; w < store.wordCount; w++) {
        if (store.words[w] != 0xFFFF) {
            return false;
        }
    }
    for (size_t b = 0; b < store.byteCount; b++) {
        if (store.bytes[b] != 0) {
            return false;
        }
    }
    return true;
}

void BoardSettingsInPlay::Learn(const SettingsStore &store) {
    const std::vector<BoardSetting> &settings = BoardSettings();
    m_asSet.resize(settings.size(), 0);
    m_applied.resize(settings.size(), 0);
    for (size_t i = 0; i < settings.size(); i++) {
        if (!m_applied[i]) {
            m_asSet[i] = SettingValue(store, settings[i]);
        }
    }
}

void BoardSettingsInPlay::Loaded(const SettingsStore &store) {
    m_applied.assign(BoardSettings().size(), 0);
    Learn(store);
}

void BoardSettingsInPlay::Apply(SettingsStore &store, const std::vector<int> &features) {
    const std::vector<BoardSetting> &settings = BoardSettings();
    Learn(store);
    bool wrote = false;
    for (size_t i = 0; i < settings.size(); i++) {
        const int choice = Choice(features, SettingAt()[i], settings[i].values.size());
        const uint32_t value = choice ? settings[i].values[choice - 1] : m_asSet[i];
        if ((choice || m_applied[i]) && SettingValue(store, settings[i]) != value) {
            SetSettingValue(store, settings[i], value);
            wrote = true;
        }
        m_applied[i] = choice;
    }
    if (wrote) {
        FixBoardChecksums(store);
    }
}

std::vector<uint8_t> BoardSettingsInPlay::Kept(const SettingsStore &store) {
    const std::vector<BoardSetting> &settings = BoardSettings();
    Learn(store);
    std::vector<uint8_t> out;
    const uint8_t *from = store.words ? reinterpret_cast<const uint8_t *>(store.words) : store.bytes;
    const size_t size = store.words ? store.wordCount * 2 : store.byteCount;
    out.assign(from, from + size);
    SettingsStore copy = store;
    if (store.words) {
        copy.words = reinterpret_cast<uint16_t *>(out.data());
    } else {
        copy.bytes = out.data();
    }
    bool wrote = false;
    for (size_t i = 0; i < settings.size(); i++) {
        if (m_applied[i] && SettingValue(copy, settings[i]) != m_asSet[i]) {
            SetSettingValue(copy, settings[i], m_asSet[i]);
            wrote = true;
        }
    }
    if (wrote) {
        FixBoardChecksums(copy);
    }
    return out;
}

bool BoardSettingsInPlay::AnyChosen(const std::vector<int> &features) {
    const std::vector<BoardSetting> &settings = BoardSettings();
    for (size_t i = 0; i < settings.size(); i++) {
        if (Choice(features, SettingAt()[i], settings[i].values.size())) {
            return true;
        }
    }
    return false;
}

uint32_t Autofire(uint32_t bits, uint64_t frame, const std::vector<int> &features) {
    // (a choice's frames a round: pressed for the first half of them, rounded up, let go for the rest. 30 a second is
    // pressed one frame and let go the next; 20, pressed two and let go one; 15, two and two. The board's own frame
    // rate: the CV1000's 60, the PGM's 59.2, Cave's 68000 board's 57.4)
    static constexpr int kRound[] = {0, 2, 3, 4};
    for (size_t b = 0; b < AutofireAt().size(); b++) {
        const uint32_t button = kInButton1 << b;
        const int choice = Choice(features, AutofireAt()[b], 3);
        if (!choice || !(bits & button)) {
            continue;
        }
        const uint64_t round = static_cast<uint64_t>(kRound[choice]);
        if (frame % round >= (round + 1) / 2) {
            bits &= ~button;
        }
    }
    return bits;
}

} // namespace spelunk

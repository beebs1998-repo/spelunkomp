// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The game.cfg of the game this program is built for, and every game's (put in by the build from games/*/game.cfg)
#include "romset.h"

#include "../machine/cave68k.h"
#include "../machine/cv1000.h"
#include "../machine/pgm.h"

#include <memory>

namespace spelunk {

const char *BuiltInGameConfig() {
    return
#include "game_cfg.inc"
        ;
}

std::string GameConfigValue(const char *key) {
    static const GameConfig config = [] {
        GameConfig c;
        ParseGameConfig(BuiltInGameConfig(), c);
        return c;
    }();
    const auto it = config.find(key);
    return it == config.end() ? std::string() : it->second;
}

const char *BuiltInGameConfigText() {
    return BuiltInGameConfig();
}

std::string GameConfigValueIn(const char *text, const char *key) {
    GameConfig config;
    ParseGameConfig(text, config);
    const auto it = config.find(key);
    return it == config.end() ? std::string() : it->second;
}

bool CheckSetFor(const char *text, const std::string &path, std::string &message) {
    GameConfig config;
    ParseGameConfig(text, config);
    // (the collection's launcher checks every game's set, whatever its hardware)
    if (config["hardware"] == "pgm") {
        auto board = std::make_unique<Pgm>();
        return LoadSet(path, config, *board, message);
    }
    if (config["hardware"] == "cave68k") {
        auto board = std::make_unique<Cave68k>();
        return LoadSet(path, config, *board, message);
    }
    auto board = std::make_unique<Cv1000>();
    return LoadSet(path, config, *board, message);
}

const std::vector<GameEntry> &AllGames() {
    static const std::vector<GameEntry> games = {
#include "all_games.inc"
    };
    return games;
}

} // namespace spelunk

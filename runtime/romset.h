// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The player's files: a game's ROM set, in a folder or a zip, each file found by its checksum (whatever it is called).
// Only read, never changed.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace spelunk {

class Cv1000;
class Cave68k;
class Pgm;

// game.cfg: key = value lines
using GameConfig = std::map<std::string, std::string>;
bool ReadGameConfig(const std::string &path, GameConfig &config, std::string &error);
void ParseGameConfig(const std::string &text, GameConfig &config);
// The game.cfg the program was built for (the build puts it in)
const char *BuiltInGameConfig();
std::string GameConfigValue(const char *key);
// Every game's id and game.cfg (the collection's launcher)
struct GameEntry {
    const char *id;
    const char *config;
};
const std::vector<GameEntry> &AllGames();

std::string GameConfigValueIn(const char *text, const char *key);
const char *BuiltInGameConfigText();
// Whether the set at `path` is the one the game.cfg `text` asks for (read and checked as the game does)
bool CheckSetFor(const char *text, const std::string &path, std::string &message);

// Loads the set from `where`: a folder holding the set's files, a folder holding <rom_set>.zip or a <rom_set>
// folder, or the zip itself
bool LoadSet(const std::string &where, const GameConfig &config, Cv1000 &board, std::string &error);
bool LoadSet(const std::string &where, const GameConfig &config, Cave68k &board, std::string &error);
// (runtime/romset_pgm.cpp)
bool LoadSet(const std::string &where, const GameConfig &config, Pgm &board, std::string &error);

} // namespace spelunk

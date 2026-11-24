// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Whether a game's set at a path is usable, checked as the launcher checks it (runtime/romset.h CheckSetFor):
//   romset-test games/<id>/game.cfg PATH   (exit 0: usable; the message either way)
// tools/check_split_sets.sh runs it on split and merged sets made from the player's own.
#include "../runtime/romset.h"

#include <cstdio>
#include <fstream>
#include <sstream>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "romset-test games/<id>/game.cfg PATH\n");
        return 2;
    }
    std::ifstream in(argv[1]);
    std::stringstream text;
    text << in.rdbuf();
    std::string message;
    const bool ok = spelunk::CheckSetFor(text.str().c_str(), argv[2], message);
    printf("%s\n", ok ? "usable" : message.c_str());
    return ok ? 0 : 1;
}

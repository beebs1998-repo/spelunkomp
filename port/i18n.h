// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
//
// The port's words in the player's language (recomp-common/i18n.h), beside recomp-common's own tables (the shared
// menu's and launcher's): for now the Display page's shader rows, the Alternative modes row's words and Reset all
// options' note. A text neither has stays in English. The games' own words (on the board's screen) are never changed.
#pragma once

#include "../recomp-common/i18n.h"
#include "i18n_es.h"
#include "i18n_pt_br.h"

namespace port {

inline void AddTranslations() {
    static const bool added = [] {
        recomp::i18n::Add("pt-BR", kPortuguese);
        recomp::i18n::Add("es", kSpanish);
        return true;
    }();
    (void)added;
}

} // namespace port

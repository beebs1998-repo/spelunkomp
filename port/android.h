// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's port/port.h.
//
// Android (port/android.cpp): the game is a library its activity loads (android/, GameActivity.java), which says the
// game's folder and its log as variables; this takes them, before anything else.
#pragma once

namespace port {

void StartOnAndroid();

} // namespace port

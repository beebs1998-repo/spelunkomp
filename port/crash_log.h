// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's port/main.cpp.
//
// A crash leaves its log. The launcher sends a game's output to logs/game.log; buffered in blocks, a crash lost the
// block and the launcher's report pointed at an empty log. KeepLogOnCrash writes the output as it goes (the
// standard output a line at a time, its errors at once; Windows' C library has no line buffering, its _IOLBF is
// full buffering, so there both are unbuffered), and on Windows an unhandled exception writes the exception, its
// address and, for an access violation, the address read or written, then the calls that led there as
// program+offset: the same build's unstripped program names them (build/release-windows/symbols,
// tools/release/crash_where.sh). Android sets its own output up (port/android.cpp).
#pragma once

namespace port {

void KeepLogOnCrash();
// A crash on purpose, for checking the crash report on a release build: with SPELUNKOMP_TEST_CRASH_AT_FRAME=<n> set,
// the game writes through a null pointer after its frame n. Nothing happens unless it is set (docs/RELEASING.md).
void TestCrashAt(unsigned long long frame);

} // namespace port

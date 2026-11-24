#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The memory check's start of a game on Linux (linux.sh runs it by systemd-run): writes its own PID to PIDFILE, then
# becomes the program (exec), so the PID is the program's. A script of its own: systemd expands $ (and %) in the
# command it is given, so no $ may be written there (a `sh -c 'echo $$ ...'` wrote "$").
#   pidwrap.sh PIDFILE PROGRAM [ARGS...]
echo $$ > "$1.new" && mv "$1.new" "$1" || exit 127
shift
exec "$@"

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""Merges block recordings (--record-blocks files) into one.

    merge_blocks.py OUT.txt IN.txt [IN.txt...]

Each version of a block (an address and its bytes) once; a block seen running anywhere is kept as run, one only found
by following the code (`found`) stays marked so. OUT may be one of the INs (it is read before it is written).
"""
import sys


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    blocks = {}
    for path in sys.argv[2:]:
        try:
            lines = open(path).read().split("\n")
        except FileNotFoundError:
            continue
        for line in lines:
            parts = line.split()
            if len(parts) < 2:
                continue
            key = (int(parts[0], 16), parts[1].upper())
            ran = len(parts) == 2
            blocks[key] = blocks.get(key, False) or ran
    with open(sys.argv[1], "w") as out:
        for (pc, code), ran in sorted(blocks.items()):
            out.write("%08X %s%s\n" % (pc, code, "" if ran else " found"))
    print("%s: %d blocks (%d run, %d found), %d addresses" % (sys.argv[1], len(blocks), sum(blocks.values()),
                                                              len(blocks) - sum(blocks.values()),
                                                              len({pc for pc, _ in blocks})))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""The PGM against a real board: the PGM MiSTer core's test ROMs (MiSTer-devel/Arcade-IGSPGM_MiSTer, testroms/,
PAGE=vram_bench and PAGE=sdr_stress, built with m68k-elf-gcc: testroms/build/pgm_test/pgm/pgm_p02s.u20) run on the
port's PGM headless with the board's waits on, against the results the core's author measured on a board
(util/vram_bench/results/hw_full.jsonl, hw_gap.jsonl; util/sdr_stress/results/sdr_hw_postfix.jsonl). docs/PGM.md,
"Against a board".

  tools/pgm_board.py CORE_REPO VRAM_BENCH.u20 SDR_STRESS.u20 PGM_ZIP [HEADLESS]

PGM_ZIP: a set holding pgm_t01s.rom and pgm_m01s.rom (the BIOS's tiles and samples: ddp2.zip has them) and the
sprite ROMs pgm_a1300.u1, pgm_b1300.u7. HEADLESS: a PGM game's build/<id>/spelunk-headless (build/ddp2 by default).
The ROM is talked to through its debug link's ring in work RAM (headless_pgm --link)."""
import json, os, statistics, struct, subprocess, sys, tempfile, zipfile, zlib

VARIANTS = ["nomem", "word_r", "word_w", "long_r", "long_w", "byte_r_hi", "byte_r_lo", "byte_w_hi", "byte_w_lo",
            "movem_r", "movem_w", "rmw", "word_r_p1", "word_r_p2", "word_r_p4", "word_r_p8", "alt_rw"]
TARGETS = ["work_ram", "bg_vram", "fg_vram"]


def make_set(folder, name, rom, zip_path):
    data = open(rom, "rb").read()
    with zipfile.ZipFile(zip_path) as z, zipfile.ZipFile(os.path.join(folder, name + ".zip"), "w") as out:
        out.writestr(name + ".u20", data)
        for f in ("pgm_t01s.rom", "pgm_m01s.rom", "pgm_a1300.u1", "pgm_b1300.u7"):
            out.writestr(f, z.read(f))
    crc = lambda b: "%08x" % zlib.crc32(b)
    with zipfile.ZipFile(zip_path) as z:
        c = {f: crc(z.read(f)) for f in ("pgm_t01s.rom", "pgm_m01s.rom", "pgm_a1300.u1", "pgm_b1300.u7")}
    cfg = os.path.join(folder, name + ".cfg")
    open(cfg, "w").write(
        "name = PGM test ROM\nid = %s\nrom_set = %s\nhardware = pgm\nprogram = %s.u20:%s@0\n"
        "tiles = pgm_t01s.rom:%s@0\ntiles_size = A00000\nsprite_pixels = pgm_a1300.u1:%s@0\n"
        "sprite_pixels_size = 1000000\nsprite_masks = pgm_b1300.u7:%s@0\nsprite_masks_size = 800000\n"
        "samples = pgm_m01s.rom:%s@0\nrotate = 0\n" % (name, name, name, crc(data), c["pgm_t01s.rom"],
                                                     c["pgm_a1300.u1"], c["pgm_b1300.u7"], c["pgm_m01s.rom"]))
    return cfg


def run(headless, folder, cfg, magic, requests):
    seq, lines = 0, []
    for cmd, payload in requests:
        seq = (seq + 1) & 0xFF
        lines.append((magic + bytes([1, seq, cmd, len(payload)]) + payload).hex())
    req, rsp = os.path.join(folder, "req.txt"), os.path.join(folder, "rsp.txt")
    open(req, "w").write("\n".join(lines) + "\n")
    subprocess.run([headless, "--game", cfg, "--roms", folder, "--frames", "100000000", "--cpu", "interpreter",
                    "--timing", "--link", req + ":" + rsp], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return [bytes.fromhex(l.strip())[6:] for l in open(rsp)]


def main():
    core, vb_rom, ss_rom, zip_path = sys.argv[1:5]
    headless = os.path.abspath(sys.argv[5] if len(sys.argv) > 5 else "build/ddp2/spelunk-headless")
    folder = tempfile.mkdtemp()
    res = lambda *p: os.path.join(core, "util", *p)

    # vram_bench: run.py's matrix, the windows the board was given
    hw = [json.loads(l) for l in open(res("vram_bench", "results", "hw_full.jsonl"))]
    windows = {k: tuple(v) for k, v in hw[0]["windows"].items()}
    board = {}
    for r in hw[1:]:
        board.setdefault((r["variant"], r["target"], r["logical_window"], r["flags"]), []).append(r["accesses_per_frame"])
    tests = [(v, t, w, f) for v in VARIANTS[1:12] for t in TARGETS for w in ("all", "vblank", "active", "hline")
             for f in (0, 1)] + [("nomem", "work_ram", w, 0) for w in ("all", "vblank", "active", "hline")]
    gaps = [(v, t, f) for v in ("word_r", "word_r_p1", "word_r_p2", "word_r_p4", "word_r_p8", "alt_rw", "rmw")
            for t, f in ((1, 0), (1, 1), (0, 0))]
    cfg = make_set(folder, "vram_bench", vb_rom, zip_path)
    reqs = [(1, b"")]
    for v, t, w, f in tests:
        wid, lo, hi = windows[w]
        reqs.append((0x10, struct.pack(">BBBBHHH", VARIANTS.index(v), TARGETS.index(t), wid, f, 60, lo, hi)))
    for v, t, f in gaps:
        reqs.append((0x10, struct.pack(">BBBBHHH", VARIANTS.index(v), t, 1, f, 60, 16, 200)))
    out = run(headless, folder, cfg, b"VB", reqs)[1:]
    apf = lambda p: struct.unpack(">BBBBHHHHIIII", p)
    deltas = []
    for (v, t, w, f), p in zip(tests, out):
        r = apf(p)
        a, b = r[8] * r[7] / r[4], statistics.median(board[(v, t, w, f)])
        deltas.append(((a - b) / b * 100, v, t, w, f, a, b))
    deltas.sort(key=lambda d: -abs(d[0]))
    for tol in (0.5, 1, 2):
        print("vram_bench: %d of %d results within %.1f%% of the board" % (sum(abs(d[0]) <= tol for d in deltas), len(deltas), tol))
    for d in deltas[:12]:
        print("  %+6.2f%%  %-9s %-8s %-6s %d  port %9.1f  board %9.1f" % d)
    gap_board = {r["variant"]: r for r in map(json.loads, open(res("vram_bench", "results", "hw_gap.jsonl"))) if r["type"] == "probe"}
    print("gap probes, lines 16-199 (port / board): contended, CPU first, work RAM")
    for n, v in enumerate(dict.fromkeys(g[0] for g in gaps)):
        vals = [apf(p) for p in out[len(tests) + 3 * n:len(tests) + 3 * n + 3]]
        print("  %-10s" % v + "".join("  %8.1f / %8.1f" % (r[8] * r[7] / r[4], gap_board[v][k])
                                    for r, k in zip(vals, ("lock_apf", "free_apf", "work_apf"))))

    # sdr_stress: 4 loops x 0..224 sprites, 120 frames, seed 0x1234
    cfg = make_set(folder, "sdr_stress", ss_rom, zip_path)
    loops = ["nomem", "rom_hot", "rom_miss", "rom_code"]
    sdr = [(v, c) for v in range(4) for c in (0, 32, 64, 128, 224)]
    out = run(headless, folder, cfg, b"SS", [(1, b"")] + [(0x10, struct.pack(">BBHHH", v, 0, 120, c, 0x1234)) for v, c in sdr])[1:]
    board = {(r["variant"], r["sprite_count"]): r["chunks"] for r in map(json.loads, open(res("sdr_stress", "results", "sdr_hw_postfix.jsonl"))) if "variant" in r}
    print("sdr_stress (chunks in 120 frames, port / board)")
    for (v, c), p in zip(sdr, out):
        a, b = struct.unpack(">BBHHHIII", p)[5], board[(loops[v], c)]
        print("  %-9s %3d sprites  %7d / %7d  %+.3f%%" % (loops[v], c, a, b, (a - b) / b * 100))


main()

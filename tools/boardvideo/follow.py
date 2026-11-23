#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""Lines a video of a board's PLAY up against a port run of the same stages played another way, by the background
(docs/BOARD-VIDEO.md, "Futari 1.5").

boardvideo.py matches whole pictures, which needs the same play on both sides (an attract demo). In a player's run
the ship, the shots and the bullets differ from any run the port can make; the background does not: it scrolls with
the game's frames. So each picture of the video is compared with the port's pictures by the pixels that agree best
(the closest 55% of them, after both are brought to the same levels): sprites that differ fall in the other 45%.

The port's pictures are counted as the game's frames: a frame the port was late for (SPELUNK_LATE_LOG) shows the
picture before again, so it is left out, and picture g is the game's g-th frame. The video's pictures are its
distinct frames (repeats.py scan: a repeated frame is the same picture). Pictures in scenes that differ too much
(big explosions, a laser over half the screen) or whose background stands still (bosses) give no sure match and are
passed over: the counts are between sure matches. Over any stretch the board was late R dt - dg frames (R: board refreshes a second of
video), and the port, run to the same g, late (port frames - dg).

    follow.py thumbs VIDEO SCAN.npz OUT.npz --warp OX,OY,SX,SY [--div 2] [--from S] [--to S]
        the distinct frames' pictures on the port's grid (SX, SY: video pixels a board pixel; the port's dump
        divided by --div, as SPELUNK_FRAME_DUMP's :DIV)
    follow.py track BOARD.npz PORT.thm LATE OUT.npz [--anchor-every N] [--mask-top PX] [--mask-bottom PX]
        every Nth picture: the whole port run searched small, the best few refined (8 frames each way, the
        background moved across by up to --max-shift), each kept with its score and sharpness
    follow.py report OUT.npz [OUT2.npz...] [--rate R] [--split S,S...] [--every S | --cuts S,S...] [--plot FILE]
        the sure matches chained (the game frame never back, never faster than the board can run), and board
        and port late frames per stretch between them

Needs numpy and ffmpeg.
"""
import argparse
import functools
import subprocess
import sys

import numpy as np

print = functools.partial(print, flush=True)

KEEP = 0.55  # the share of pixels counted (the closest)


def load_port(path):
    with open(path, 'rb') as f:
        head = f.read(16)
    if head[:8] != b'SPLKFRM1':
        sys.exit('%s: not a SPELUNK_FRAME_DUMP file' % path)
    w, h = np.frombuffer(head[8:], '<u4')
    import os
    n = (os.path.getsize(path) - 16) // (w * h)
    return np.memmap(path, np.uint8, 'r', 16, (n, h, w))


def load_late(path, count):
    late = np.zeros(count, bool)
    for line in open(path):
        p = line.split()
        if len(p) >= 2:
            i = int(p[0]) - 1
            if 0 <= i < count:
                late[i] = int(p[1]) != 0
    return late


def thumbs(a):
    d = np.load(a.scan)
    block = d['block']
    thr = a.threshold
    new = block >= thr
    new[0] = True
    start = float(d['start'])
    t_all = start + np.arange(len(block)) / 60.0
    out = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height',
                          '-of', 'csv=p=0', a.video], capture_output=True, text=True, check=True).stdout
    vw, vh = [int(x) for x in out.strip().split(',')[:2]]
    ox, oy, sx, sy = a.warp
    tw, th = 240 // a.div, 320 // a.div
    if a.size:
        tw, th = a.size
    # each thumbnail pixel: the mean of the video's pixels over its area (a box sum on an integral image)
    k = a.div
    x0 = np.clip(np.round(ox + sx * k * np.arange(tw)).astype(int), 0, vw - 1)
    x1 = np.clip(np.round(ox + sx * k * (np.arange(tw) + 1)).astype(int), 1, vw)
    y0 = np.clip(np.round(oy + sy * k * np.arange(th)).astype(int), 0, vh - 1)
    y1 = np.clip(np.round(oy + sy * k * (np.arange(th) + 1)).astype(int), 1, vh)
    area = ((y1 - y0)[:, None] * (x1 - x0)[None, :]).astype(np.float32)
    cmd = ['ffmpeg', '-v', 'error']
    lo = a.start or 0.0
    if a.start:
        cmd += ['-ss', str(a.start)]
    cmd += ['-i', a.video]
    if a.end:
        cmd += ['-t', str(a.end - lo)]
    cmd += ['-f', 'rawvideo', '-pix_fmt', 'gray', '-']
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    first = int(round((lo - start) * 60))
    pics, times = [], []
    i = first
    size = vw * vh
    while True:
        b = p.stdout.read(size)
        if len(b) < size:
            break
        if 0 <= i < len(new) and new[i]:
            f = np.frombuffer(b, np.uint8).reshape(vh, vw).astype(np.float64)
            c = np.pad(f.cumsum(0).cumsum(1), ((1, 0), (1, 0)))
            s = c[y1][:, x1] - c[y0][:, x1] - c[y1][:, x0] + c[y0][:, x0]
            pics.append(np.clip(s / area, 0, 255).astype(np.uint8))
            times.append(t_all[i])
        i += 1
        if (i - first) % 12000 == 0:
            print('%.0f s' % (i / 60))
    p.wait()
    np.savez_compressed(a.out, pics=np.array(pics), t=np.array(times), warp=np.array(a.warp), div=a.div)
    print('%d pictures' % len(pics))


def norm(x, mask):
    """levels made comparable: the masked pixels' median to 0, their spread (the middle half) to 1"""
    x = x.astype(np.float32)
    v = x[..., mask]
    q1, q2, q3 = np.percentile(v, [25, 50, 75], axis=-1)
    return (x - q2[..., None, None]) / (q3 - q1 + 1.0)[..., None, None]


def score(b, ps, mask):
    """trimmed mean difference of picture b (h, w) against pictures ps (n, h, w), on the mask"""
    d = np.abs(ps[:, mask] - b[mask][None])
    k = int(KEEP * d.shape[1])
    return np.partition(d, k, axis=1)[:, :k].mean(1)


def track(a):
    port = load_port(a.port)
    n, th, tw = port.shape
    late = load_late(a.late, n)
    G = np.where(~late)[0]  # the port frame showing game frame g
    B = np.load(a.board)
    pics, t = B['pics'], B['t']
    mask = np.zeros((th, tw), bool)
    mask[a.mask_top:th - a.mask_bottom, :] = True
    X = a.max_shift
    R = a.rate
    print('%d board pictures, %d port frames, %d game frames (%d late)' % (len(pics), n, len(G), int(late.sum())))

    # the whole run, small, for the first look
    step = 4
    sm = mask[::step, ::step]

    def small(x):
        x = np.asarray(x, np.float32)[..., :th // step * step, :tw // step * step]
        return x.reshape(x.shape[:-2] + (th // step, step, tw // step, step)).mean((-3, -1))

    ps = np.empty((len(G), th // step, tw // step), np.float32)
    for s in range(0, len(G), 4000):
        ps[s:s + 4000] = norm(small(port[G[s:s + 4000]]), sm)

    def refine(b, lo, hi):
        """the best game frame in lo..hi-1, the board picture moved across by up to X: (score, g, dx, sharpness)
        where sharpness is how much worse the best frame 3 or more away is"""
        lo, hi = max(0, lo), min(len(G), hi)
        if hi - lo < 1:
            return 9.0, lo, 0, 0.0
        cand = norm(np.asarray(port[G[lo:hi]]), mask)
        best = None
        for dx in range(-X, X + 1):
            sc = score(np.roll(b, dx, 1), cand, mask)
            k = int(np.argmin(sc))
            if best is None or sc[k] < best[0]:
                best = (float(sc[k]), lo + k, dx, sc)
        sc, g, dx, row = best
        far = np.abs(np.arange(lo, hi) - g) >= 3
        return sc, g, dx, float(row[far].min() - sc) if far.any() else 0.0

    idx = np.arange(a.first, min(len(pics), a.last or len(pics)), a.anchor_every)
    rows = []  # (picture, g, dx, score, sharpness) for each candidate
    last = None  # (time, game frame) of the last good match
    for j, i in enumerate(idx):
        b = norm(pics[i], mask)
        starts = []
        if last is not None:
            # where the board can be: between a third of full speed and full speed since the last good match
            dt = t[i] - last[0]
            starts.append((int(last[1] + R * dt / 3) - 4, int(last[1] + R * dt) + 5))
        if last is None or j % a.global_every == 0 or dt > 20:
            sc = score(norm(small(pics[i]), sm), ps, sm)
            top = []
            for g in np.argsort(sc):
                if all(abs(int(g) - k) > 12 for k in top):
                    top.append(int(g))
                if len(top) == a.candidates:
                    break
            starts += [(g0 - 8, g0 + 9) for g0 in top]
        mine = []
        for lo, hi in starts:
            r = refine(b, lo, hi)
            mine.append(r)
            rows.append((i, r[1], r[2], r[0], r[3]))
        r = min(mine)
        if r[0] <= a.good:
            last = (t[i], r[1])
        if j % 100 == 0:
            print('picture %d (%.0f s): game frame %d, across %+d, score %.3f, sharp %.3f' % (
                i, t[i], r[1], r[2], r[0], r[3]))
    rows = np.array(rows, float)
    np.savez_compressed(a.out, t=t, rows=rows, G=G, late=late, every=a.anchor_every)


def chain(d, R, max_score, min_sharp, t_from=None, t_to=None):
    """the longest run of candidates (one per picture at most) the board could have run through: the game frame
    never back, never more than R dt + 2 on, and not less than a third of that (a long stall is not a scroll); the
    sure ones only (score at most max_score, sharpness at least min_sharp)"""
    t, rows = d['t'], d['rows']
    ok = (rows[:, 3] <= max_score) & (rows[:, 4] >= min_sharp)
    tr = t[rows[:, 0].astype(int)]
    if t_from is not None:
        ok &= (tr >= t_from) & (tr < t_to)
    r = rows[ok]
    r = r[np.lexsort((r[:, 3], r[:, 0]))]
    m = len(r)
    tt = t[r[:, 0].astype(int)]
    best = np.ones(m)
    back = np.full(m, -1)
    for k in range(m):
        lo = np.searchsorted(tt, tt[k] - 60.0)  # (links reach a minute back)
        for j in range(lo, k):
            if r[j, 0] >= r[k, 0]:
                continue
            dg = r[k, 1] - r[j, 1]
            dt = tt[k] - tt[j]
            if 0 <= dg <= R * dt + 2 and dg >= R * dt / 3 - 2 and best[j] + 1 > best[k]:
                best[k] = best[j] + 1
                back[k] = j
    k = int(np.argmax(best)) if m else -1
    out = []
    while k >= 0:
        out.append(k)
        k = back[k]
    out = r[out[::-1]]
    return t[out[:, 0].astype(int)], out[:, 1].astype(int), out


def report(a):
    parts = [np.load(p) for p in a.npz]
    d = dict(parts[0])
    d['rows'] = np.concatenate([q['rows'] for q in parts])
    G = d['G']
    R = a.rate
    # each part (between --split times: the stages, whose game frames the port reaches at other times than the
    # board after a boss) chained on its own; no step is counted across a split
    edges = [-1e9] + ([float(x) for x in a.split.split(',')] if a.split else []) + [1e9]
    parts = [chain(d, R, a.max_score, a.min_sharp, e0, e1) for e0, e1 in zip(edges[:-1], edges[1:])]
    parts = [q for q in parts if len(q[0])]
    t = np.concatenate([q[0] for q in parts])
    g = np.concatenate([q[1] for q in parts])
    rows = np.concatenate([q[2] for q in parts])
    brk = np.cumsum([len(q[0]) for q in parts])[:-1]  # the first anchor of each part after the first
    print('%d sure anchors on the chain (of %d pictures looked at)' % (len(t), len(np.unique(d['rows'][:, 0]))))
    if a.cuts:
        cuts = [float(x) for x in a.cuts.split(',')]
    else:
        cuts = list(np.arange(t[0], t[-1] + a.every, a.every))
    print('| video s | game frames | board late | port late | board - port | anchors |')
    print('|---|---|---|---|---|---|')
    # each step between two anchors on the chain goes to the stretch its end is in, so nothing is lost between
    # stretches (a step across a cut counts in the later one)
    db = R * np.diff(t) - np.diff(g)
    dp = (np.diff(G[g]) - np.diff(g)).astype(float)
    db[brk - 1] = np.nan  # (the steps across a split)
    dp[brk - 1] = np.nan
    tb = tp = 0
    for c0, c1 in zip(cuts[:-1], cuts[1:]):
        k = np.where((t[1:] > c0) & (t[1:] <= c1) & ~np.isnan(db))[0]
        if not len(k):
            continue
        board, port = float(db[k].sum()), int(dp[k].sum())
        tb += board
        tp += port
        print('| %.1f-%.1f | %d-%d | %.1f | %d | %+.1f | %d |' % (t[k[0]], t[k[-1] + 1], g[k[0]], g[k[-1] + 1],
                                                              board, port, board - port, len(k) + 1))
    print('| all | | %.1f | %d | %+.1f | |' % (tb, tp, tb - tp))
    if a.plot:
        np.savetxt(a.plot, np.column_stack([t, g, R * t - g, G[g] - g, rows[:, 2:]]), fmt='%.4f',
                   header='t g board_late(cum, +const) port_late(cum) dx score sharp')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('thumbs')
    s.add_argument('video')
    s.add_argument('scan')
    s.add_argument('out')
    s.add_argument('--warp', required=True, type=lambda v: [float(x) for x in v.split(',')])
    s.add_argument('--div', type=int, default=2)
    s.add_argument('--size', type=lambda v: [int(x) for x in v.split('x')])
    s.add_argument('--threshold', type=float, default=13.8, help='repeats.py runs\' threshold')
    s.add_argument('--from', dest='start', type=float)
    s.add_argument('--to', dest='end', type=float)
    r = sub.add_parser('track')
    r.add_argument('board')
    r.add_argument('port')
    r.add_argument('late')
    r.add_argument('out')
    r.add_argument('--anchor-every', type=int, default=15, help='a picture looked up in every N')
    r.add_argument('--candidates', type=int, default=4)
    r.add_argument('--first', type=int, default=0, help='the first picture (to split a long video over processes)')
    r.add_argument('--last', type=int, help='the picture to stop before')
    r.add_argument('--global-every', type=int, default=10, help='the whole run searched at every Nth lookup')
    r.add_argument('--good', type=float, default=0.2, help='a match this close is followed from')
    r.add_argument('--rate', type=float, default=60.0175, help='board refreshes a second of video')
    r.add_argument('--max-shift', type=int, default=6, help='the background\'s largest move across (thumbnail pixels)')
    r.add_argument('--mask-top', type=int, default=14)
    r.add_argument('--mask-bottom', type=int, default=6)
    p = sub.add_parser('report')
    p.add_argument('npz', nargs='+', help='track\'s output (several: the parts of one video, merged)')
    p.add_argument('--rate', type=float, default=60.0175)
    p.add_argument('--every', type=float, default=30.0)
    p.add_argument('--cuts')
    p.add_argument('--split', help='video times (s) where the chain starts again (stage ends)')
    p.add_argument('--max-score', type=float, default=0.15)
    p.add_argument('--min-sharp', type=float, default=0.03)
    p.add_argument('--plot')
    a = ap.parse_args()
    {'thumbs': thumbs, 'track': track, 'report': report}[a.cmd](a)


if __name__ == '__main__':
    main()

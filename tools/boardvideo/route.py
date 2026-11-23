#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""A player's route read off a board's video, to be replayed on the port (docs/BOARD-VIDEO.md, "Black Label: the
player's route replayed").

follow.py lines a board's video up with a port run by the background, but the port's ship goes another way than
the player's, so the two screens hold different things (the enemies killed where, the gems, the counter). This reads
where the player's ship is in each frame of the video and writes it as a route by game frame for the port
(port/headless.cpp's SPELUNK_ROUTE): the ship put where the player's was each game frame, and the laser or the shot.
The laser is told by the ship's speed: it flies at half speed while the laser is out (Futari: 1.5 board pixels a
frame against 3). The beam itself is not told apart from the shot reliably in a 720p capture.

    route.py cut VIDEO TIME X,Y,W,H OUT.png --warp OX,OY,SX,SY [--whole FILE]
        a piece of a frame on the board's grid (240 x 320, stood up): the ship's picture, for track
    route.py track VIDEO OUT.npz --warp ... --template T.png[,T2.png...] [--from S] [--to S] [--start X,Y]
        the ship found in each frame by its pictures (normalised correlation, near where it was, the whole picture
        searched when unsure; a second pass with each picture taken again as the mean of its sure finds). Frames
        where it is not found are marked (score -1) and filled in by make
    route.py make TRACK.npz CHAIN.txt OUT.route [--bits-laser B] [--bits-shot B] [--still keep|laser|shot]
        [--x-at A --y-at A] [--dx D --dy D] [--from G] [--to G]
        the track turned into game frames by follow.py's anchors (report --plot's table: video time, game frame),
        a line a game frame: the buttons, and the ship's place poked (16.16, across and up from the bottom, as
        Futari holds it at 0C383E98/0C383E9C)

Needs numpy and ffmpeg.
"""
import argparse
import functools
import subprocess
import sys

import numpy as np

print = functools.partial(print, flush=True)

W, H = 240, 320  # the board's picture, stood up


def frames(video, warp, start=None, end=None):
    """the video's frames from start to end (s), each the game's area brought to W x H (RGB, float32)"""
    ox, oy, sx, sy = warp
    cw, ch = W * sx, H * sy
    cmd = ['ffmpeg', '-v', 'error']
    if start:
        cmd += ['-ss', '%.3f' % start]
    cmd += ['-i', video]
    if end:
        cmd += ['-t', '%.3f' % (end - (start or 0))]
    cmd += ['-vf', 'crop=%d:%d:%d:%d,scale=%d:%d:flags=area' % (round(cw), round(ch), round(ox), round(oy), W, H),
            '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    n = W * H * 3
    while True:
        b = p.stdout.read(n)
        if len(b) < n:
            break
        yield np.frombuffer(b, np.uint8).reshape(H, W, 3).astype(np.float32)
    p.stdout.close()
    p.kill()
    p.wait()


def read_png(path):
    out = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', path],
                         capture_output=True, text=True, check=True).stdout
    w, h = [int(x) for x in out.strip().split(',')[:2]]
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'rgba', '-'],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(h, w, 4).astype(np.float32)


def write_png(path, rgb):
    h, w = rgb.shape[:2]
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (w, h), '-i',
                    '-', path], input=np.clip(rgb, 0, 255).astype(np.uint8).tobytes(), check=True)


def cut(a):
    x, y, w, h = a.box
    for f in frames(a.video, a.warp, a.time, a.time + 0.02):
        write_png(a.out, f[y:y + h, x:x + w])
        if a.whole:
            write_png(a.whole, f)
        return


def ncc_map(area, T):
    """the template's normalised correlation (its mean taken off) at each place in area: (ny, nx)"""
    th, tw = T.shape[:2]
    win = np.lib.stride_tricks.sliding_window_view(area, (th, tw), axis=(0, 1))  # (ny, nx, 3, th, tw)
    z = win - win.mean((3, 4), keepdims=True)
    num = np.einsum('yxcij,cij->yx', z, T.transpose(2, 0, 1), optimize=True)
    return num / (np.sqrt((z * z).sum((2, 3, 4))) + 1e-6)


def match(f, T, cx, cy, reach):
    """the template's best place near (cx, cy) (its centre, within reach): (x, y, correlation)"""
    th, tw = T.shape[:2]
    x0, y0 = max(0, cx - reach - tw // 2), max(0, cy - reach - th // 2)
    x1, y1 = min(W, cx + reach + tw - tw // 2), min(H, cy + reach + th - th // 2)
    area = f[y0:y1, x0:x1]
    if area.shape[0] < th or area.shape[1] < tw:
        return cx, cy, -1.0
    c = ncc_map(area, T)
    k = np.unravel_index(int(np.argmax(c)), c.shape)
    return x0 + k[1] + tw // 2, y0 + k[0] + th // 2, float(c[k])


def unit(tpl):
    T = tpl - tpl.mean((0, 1))
    return T / np.sqrt((T * T).sum())


def best(f, Ts, cx, cy, reach):
    """the best of the templates: (x, y, correlation, which)"""
    r = [match(f, T, cx, cy, reach) + (k,) for k, T in enumerate(Ts)]
    return max(r, key=lambda v: v[2])


def track_pass(a, Ts, keep=None):
    th, tw = Ts[0].shape[:2]
    cx, cy = a.start if a.start else (W // 2, 250)
    xs, ys, sc, beam, light = [], [], [], [], []
    acc = [np.zeros((th, tw, 3), np.float64) for _ in Ts]
    nacc = [0 for _ in Ts]
    lost = 0
    for i, f in enumerate(frames(a.video, a.warp, a.start_s, a.end_s)):
        x, y, s, w = best(f, Ts, cx, cy, a.reach)
        found = s >= a.near
        if s < a.sure:
            # unsure or lost: the whole picture searched too (a coarse look first, then refined); a clearly better
            # place elsewhere wins
            x2, y2, s2, w2 = best(f[::2, ::2], [unit(T[::2, ::2]) for T in Ts], W // 4, H // 4, max(W, H))
            x3, y3, s3, w3 = best(f, Ts, 2 * x2, 2 * y2, 3)
            if s3 >= a.good and (not found or s3 >= s + a.margin):
                x, y, s, w, found = x3, y3, s3, w3, True
            lost += not found
        if found:
            cx, cy = x, y
            if keep is not None and s >= a.good + 0.1:
                y0, x0 = y - th // 2, x - tw // 2
                if y0 >= 0 and x0 >= 0 and y0 + th <= H and x0 + tw <= W:
                    acc[w] += f[y0:y0 + th, x0:x0 + tw]
                    nacc[w] += 1
        else:
            x, y, s = cx, cy, -1.0  # (not found: where it was last; make passes over these)
        xs.append(x)
        ys.append(y)
        sc.append(s)
        # the laser: a narrow strip above the ship against the strips beside it (blue, and all light)
        top, bot = max(0, y - 90), max(0, y - 22)
        mid = f[top:bot, max(0, x - 3):x + 4]
        side = np.concatenate([f[top:bot, max(0, x - 24):max(0, x - 14)], f[top:bot, x + 15:x + 25]], 1)
        if mid.size and side.size:
            beam.append(float(mid[..., 2].mean() - side[..., 2].mean()))
            light.append(float(mid.mean() - side.mean()))
        else:
            beam.append(0.0)
            light.append(0.0)
        if i % 1200 == 0:
            print('%.1f s: ship %d,%d (%.2f), beam %.0f, lost %d' % ((a.start_s or 0) + i / 60, x, y, s, beam[-1], lost))
    if keep is not None:
        keep.extend(acc[k] / nacc[k] if nacc[k] > 20 else None for k in range(len(Ts)))
    return xs, ys, sc, beam, light


def track(a):
    Ts = [unit(read_png(p)[..., :3]) for p in a.template.split(',')]
    for k in range(a.passes - 1):
        # (each template taken again as the mean of the ship where it was found surely: less of one frame's background)
        keep = []
        track_pass(a, Ts, keep)
        Ts = [unit(m) if m is not None else T for m, T in zip(keep, Ts)]
    xs, ys, sc, beam, light = track_pass(a, Ts)
    for k, T in enumerate(Ts):
        write_png(a.out.replace('.npz', '.tpl%d.png' % k), (T - T.min()) / (T.max() - T.min()) * 255)
    np.savez_compressed(a.out, t=(a.start_s or 0) + np.arange(len(xs)) / 60.0, x=np.array(xs), y=np.array(ys),
                        score=np.array(sc), beam=np.array(beam), light=np.array(light))


def make(a):
    d = np.load(a.track)
    c = np.loadtxt(a.chain)
    tc, gc = c[:, 0], c[:, 1]
    t, x, y = d['t'], d['x'].astype(float), d['y'].astype(float)
    ok = d['score'] >= 0
    # (where the ship was not found: between the places before and after)
    x, y = np.interp(t, t[ok], x[ok]), np.interp(t, t[ok], y[ok])
    sel = (t >= tc[0]) & (t <= tc[-1])
    g = np.interp(t, tc, gc)
    gi = np.round(g).astype(int)
    g0 = max(int(gi[sel][0]), a.first or 0)
    g1 = min(int(gi[sel][-1]), a.last or 1 << 30)
    Gs = np.arange(g0, g1 + 1)
    # the ship's place each game frame: the mean of the video frames that showed it (the frames between
    # interpolated)
    px, py = np.full(len(Gs), np.nan), np.full(len(Gs), np.nan)
    for j, G in enumerate(Gs):
        k = np.where((gi == G) & sel)[0]
        if len(k):
            px[j], py[j] = x[k].mean(), y[k].mean()
    have = ~np.isnan(px)
    px, py = np.interp(Gs, Gs[have], px[have]), np.interp(Gs, Gs[have], py[have])
    # the laser: the ship slows to half speed while it is out (1.5 board pixels a frame against 3), so the speed
    # over a few frames tells which the player held; standing still keeps what was held before
    n = a.span
    v = np.zeros(len(Gs))
    v[n:-n] = np.hypot(px[2 * n:] - px[:-2 * n], py[2 * n:] - py[:-2 * n]) / (2 * n)
    on = np.zeros(len(Gs), bool)
    state = False
    for j in range(len(Gs)):
        if v[j] >= a.fast:
            state = False
        elif v[j] >= a.slow:
            state = True
        elif a.still != 'keep':
            state = a.still == 'laser'
        on[j] = state
    lines = [(int(G), a.bits_laser if on[j] else a.bits_shot, px[j] + a.dx, H - py[j] + a.dy) for j, G in enumerate(Gs)]
    with open(a.out, 'w') as f:
        for G, bits, px, py in lines:
            f.write('%d %d 0x%08X:0x%08X 0x%08X:0x%08X\n' % (G, bits, a.x_at, int(round(px * 65536)) & 0xFFFFFFFF,
                                                            a.y_at, int(round(py * 65536)) & 0xFFFFFFFF))
    n = len(lines)
    print('%d game frames (%d-%d), laser in %d (%.0f%%), ship found in %.0f%% of the video frames' % (
        n, g0, g1, on.sum(), 100.0 * on.mean(), 100.0 * ok[sel].mean()))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    warp = lambda v: [float(x) for x in v.split(',')]
    ints = lambda v: [int(x) for x in v.split(',')]
    s = sub.add_parser('cut')
    s.add_argument('video')
    s.add_argument('time', type=float)
    s.add_argument('box', type=ints)
    s.add_argument('out')
    s.add_argument('--warp', required=True, type=warp)
    s.add_argument('--whole', help='the whole frame too, to this file')
    s = sub.add_parser('track')
    s.add_argument('video')
    s.add_argument('out')
    s.add_argument('--warp', required=True, type=warp)
    s.add_argument('--template', required=True)
    s.add_argument('--from', dest='start_s', type=float)
    s.add_argument('--to', dest='end_s', type=float)
    s.add_argument('--start', type=ints, help='where the ship is in the first frame')
    s.add_argument('--reach', type=int, default=10, help='the most the ship moves a frame (board pixels)')
    s.add_argument('--good', type=float, default=0.65, help='a correlation this high is the ship')
    s.add_argument('--near', type=float, default=0.5, help='this much is enough within reach of the last place')
    s.add_argument('--margin', type=float, default=0.1, help='a place found far off must be this much better')
    s.add_argument('--sure', type=float, default=0.6, help='below this, the whole picture is searched as well')
    s.add_argument('--passes', type=int, default=2)
    s = sub.add_parser('make')
    s.add_argument('track')
    s.add_argument('chain', help='follow.py report --plot\'s table')
    s.add_argument('out')
    s.add_argument('--span', type=int, default=4, help='the speed measured over this many game frames each way')
    s.add_argument('--fast', type=float, default=2.2, help='a speed (board pixels a frame) only the shot moves at')
    s.add_argument('--slow', type=float, default=0.6, help='a slower speed from this: the laser')
    s.add_argument('--still', choices=['keep', 'laser', 'shot'], default='keep', help='standing still: what was held before, or this')
    s.add_argument('--from', dest='first', type=int)
    s.add_argument('--to', dest='last', type=int)
    s.add_argument('--x-at', type=lambda v: int(v, 0), default=0x0C383E98, help='the ship\'s place across (16.16)')
    s.add_argument('--y-at', type=lambda v: int(v, 0), default=0x0C383E9C, help='the ship\'s place up (16.16)')
    s.add_argument('--dx', type=float, default=0.0, help='added to the place across (the picture\'s centre to the game\'s)')
    s.add_argument('--dy', type=float, default=0.0)
    s.add_argument('--bits-laser', type=int, default=0x10)
    s.add_argument('--bits-shot', type=int, default=0x80)
    a = ap.parse_args()
    {'cut': cut, 'track': track, 'make': make}[a.cmd](a)


if __name__ == '__main__':
    main()

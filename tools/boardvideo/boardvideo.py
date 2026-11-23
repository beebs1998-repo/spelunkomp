#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""Lines a video of a real board up against the port, frame by frame, and counts the board's slowdown against the
port's (docs/BOARD-VIDEO.md).

The port's side: the headless runner run from power-on with SPELUNK_FRAME_DUMP (every frame's picture, small and
grey: port/frame_dump.h) and its late frames (SPELUNK_LATE_LOG on the CV1000, --frame-log on the 68000 boards and the
PGM). The video's side: any file ffmpeg reads, the picture stood up as the port's.

Each video frame is matched to the port's frame showing the same picture (the first of a run of equal ones), so the
video's capture rate, its dropped and doubled frames and its 30 fps do not matter: a frame of the video sampled late
or twice just maps to the same port frame. With R the board's refresh rate and t the video's time, the board is
R (t - t0) frames into its run where the port is n; D = R (t - t0) - n grows by one for each frame the board is
late and the port is not. t0 (and, as a check, R) are fitted where the port is never late (the board is assumed not
to be either; the fit says how well that holds).

    boardvideo.py register VIDEO PORT.thm OUT.npz [--late LOG | --frame-log LOG] [--rate HZ] [--from S] [--to S]
    boardvideo.py report OUT.npz [--rate HZ] [--calm K,K... | --chain X] [--split S,S...] [--plot FILE]
    boardvideo.py sheet OUT.npz VIDEO PNG [--at S,S,...]   side by side (video | port) at those times: the eye check

Needs numpy and ffmpeg.
"""
import argparse
import functools
import os
import subprocess
import sys

import numpy as np

print = functools.partial(print, flush=True)  # (progress shows through a pipe)


# --- the port's frames -------------------------------------------------------------------------------------------

def load_port(path):
    with open(path, 'rb') as f:
        head = f.read(16)
    if head[:8] != b'SPLKFRM1':
        sys.exit('%s: not a SPELUNK_FRAME_DUMP file' % path)
    w, h = np.frombuffer(head[8:], '<u4')
    n = (os.path.getsize(path) - 16) // (w * h)
    return np.memmap(path, np.uint8, 'r', 16, (n, h, w))


def load_late(late_log=None, frame_log=None, count=None):
    """1 for each port frame the game was late for (no new picture), by frame index 0.. (frame 1 of the log is 0)"""
    late = np.zeros(count, np.uint8)
    path, flip = (late_log, False) if late_log else (frame_log, True)
    if not path:
        return None
    for line in open(path):
        p = line.split()
        if len(p) < 2:
            continue
        i = int(p[0]) - 1
        if 0 <= i < count:
            v = int(p[1])
            late[i] = (v == 0) if flip else (v != 0)
    return late


# --- the video's frames ------------------------------------------------------------------------------------------

def probe(video):
    out = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
                          'stream=width,height,r_frame_rate,avg_frame_rate', '-of', 'default=nw=1', video],
                         capture_output=True, text=True, check=True).stdout
    d = dict(l.split('=', 1) for l in out.split())
    num, den = map(int, d['avg_frame_rate'].split('/'))
    return int(d['width']), int(d['height']), num / den, d


def frame_times(video):
    """each frame's presentation time (s), from the container (variable frame rates included)"""
    out = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'packet=pts_time',
                          '-of', 'csv=p=0', video], capture_output=True, text=True, check=True).stdout
    return np.array(sorted(float(x.strip(',')) for x in out.split() if x.strip(',') not in ('', 'N/A')))


def read_video(video, w, h, start=None, dur=None):
    cmd = ['ffmpeg', '-v', 'error']
    if start is not None:
        cmd += ['-ss', str(start)]
    cmd += ['-i', video]
    if dur is not None:
        cmd += ['-t', str(dur)]
    cmd += ['-f', 'rawvideo', '-pix_fmt', 'gray', '-']
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    size = w * h
    while True:
        b = p.stdout.read(size)
        if len(b) < size:
            break
        yield np.frombuffer(b, np.uint8).reshape(h, w)
    p.wait()


class Warp:
    """the port's thumbnail grid (tw x th) on the video's frame: x' = ox + sx x, y' = oy + sy y (pixel centres)"""

    def __init__(self, ox, oy, sx, sy, tw, th, vw, vh):
        self.p = (ox, oy, sx, sy)
        xs = ox + sx * (np.arange(tw) + 0.5) - 0.5
        ys = oy + sy * (np.arange(th) + 0.5) - 0.5
        self.x0 = np.clip(np.floor(xs).astype(int), 0, vw - 2)
        self.y0 = np.clip(np.floor(ys).astype(int), 0, vh - 2)
        self.wx = np.clip(xs - self.x0, 0, 1).astype(np.float32)
        self.wy = np.clip(ys - self.y0, 0, 1).astype(np.float32)
        self.k = max(1, int(round(min(sx, sy))))

    def __call__(self, frame):
        f = frame.astype(np.float32)
        if self.k > 1:  # a box as wide as a thumbnail pixel, so the video is averaged as the port's dump is
            c = np.cumsum(np.cumsum(np.pad(f, ((1, 0), (1, 0))), 0), 1)
            k = self.k
            b = (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)
            f = np.pad(b, ((k // 2, k - 1 - k // 2), (k // 2, k - 1 - k // 2)), mode='edge')
        r = f[self.y0] * (1 - self.wy[:, None]) + f[self.y0 + 1] * self.wy[:, None]
        return r[:, self.x0] * (1 - self.wx) + r[:, self.x0 + 1] * self.wx


def prep(imgs, step=1):
    """thumbnails (n, h, w) to zero-mean unit rows for correlation, averaged down by `step`, softened a little"""
    a = np.asarray(imgs, np.float32)
    if a.ndim == 2:
        a = a[None]
    if step > 1:
        n, h, w = a.shape
        a = a[:, :h // step * step, :w // step * step].reshape(n, h // step, step, w // step, step).mean((2, 4))
    a = (a + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 3
    a = (a + np.roll(a, 1, 2) + np.roll(a, -1, 2)) / 3
    a = a[:, 1:-1, 1:-1].reshape(len(a), -1)
    a = a - a.mean(1, keepdims=True)
    a /= np.linalg.norm(a, axis=1, keepdims=True) + 1e-6
    return a


def port_prepped(port, step, chunk=2000):
    out = []
    for s in range(0, len(port), chunk):
        out.append(prep(port[s:s + chunk], step))
    return np.concatenate(out)


# --- registering -------------------------------------------------------------------------------------------------

def initial_warp(video, vw, vh, tw, th, start, dur):
    """from the picture's lit area over the whole video"""
    acc = np.zeros((vh, vw), np.float32)
    n = 0
    for i, f in enumerate(read_video(video, vw, vh, start, dur)):
        if i % 15 == 0:
            acc = np.maximum(acc, f)
            n += 1
    lit = acc > 40
    ys, xs = np.where(lit)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    return (float(x0), float(y0), (x1 - x0) / tw, (y1 - y0) / th)


def best_path(cands, times, rate):
    """a run through the anchors' candidates (port frame, score) that never goes back and moves at most 2.5 x the
    board's rate: the highest total score (Viterbi)"""
    m = len(cands)
    score = [np.array([s for _, s in cands[0]])]
    back = [None]
    for j in range(1, m):
        dt = times[j] - times[j - 1]
        prev = np.array([n for n, _ in cands[j - 1]])
        cur = []
        bk = []
        for n, s in cands[j]:
            ok = (n >= prev) & (n - prev <= 2.5 * rate * dt + 4)
            tot = np.where(ok, score[-1], score[-1] - 1.0)  # a jump costs as much as a whole bad match
            k = int(np.argmax(tot))
            cur.append(tot[k] + s)
            bk.append(k)
        score.append(np.array(cur))
        back.append(bk)
    k = int(np.argmax(score[-1]))
    path = [0] * m
    for j in range(m - 1, -1, -1):
        path[j] = cands[j][k][0]
        if back[j] is not None:
            k = back[j][k]
    return np.array(path)


def match_window(vp, pp, centre, half):
    lo = max(0, centre - half)
    hi = min(len(pp), centre + half + 1)
    s = pp[lo:hi] @ vp
    k = int(np.argmax(s))
    # how far the best stands out from the best match that is not one of its neighbours
    away = np.abs(np.arange(lo, hi) - (lo + k)) > 3
    second = s[away].max() if away.any() else -1.0
    return lo + k, float(s[k]), float(s[k] - second)


def register(a):
    port = load_port(a.port)
    nport, th, tw = port.shape
    vw, vh, fps, info = probe(a.video)
    times = frame_times(a.video)
    t_from = a.start or 0.0
    sel = (times >= t_from) & ((times < a.end) if a.end else True)
    times = times[sel]
    print('video %dx%d, %s fps (avg %s), %d frames; port %d frames of %dx%d' % (vw, vh, info['r_frame_rate'],
          info['avg_frame_rate'], len(times), nport, tw, th))
    dur = (a.end - t_from) if a.end else None
    warp = Warp(*(a.warp or initial_warp(a.video, vw, vh, tw, th, a.start, dur)), tw, th, vw, vh)
    print('warp (ox, oy, sx, sy) start: %.1f %.1f %.3f %.3f' % warp.p)

    def video_thumbs(w):
        out = np.empty((len(times), th, tw), np.uint8)
        i = 0
        for f in read_video(a.video, vw, vh, a.start, dur):
            if i >= len(out):
                break
            out[i] = np.clip(w(f), 0, 255)
            i += 1
        return out[:i]

    vt = video_thumbs(warp)
    times = times[:len(vt)]
    late = load_late(a.late, a.frame_log, nport)
    coarse = port_prepped(port, 4)

    def anchors(vt):
        idx = np.arange(0, len(vt), a.anchor_every)
        vc = prep(vt[idx], 4)
        s = coarse @ vc.T  # (nport, anchors)
        cands = []
        for j in range(len(idx)):
            col = s[:, j]
            top = np.argpartition(-col, 40)[:40]
            top = top[np.argsort(-col[top])]
            keep = []
            for n in top:  # the best few, not neighbours of each other
                if all(abs(int(n) - k) > 6 for k, _ in keep):
                    keep.append((int(n), float(col[n])))
                if len(keep) == 8:
                    break
            cands.append(keep)
        return idx, best_path(cands, times[idx], a.rate)

    idx, path = anchors(vt)

    def fine(vt, idx, path):
        pred = np.interp(np.arange(len(vt)), idx, path).round().astype(int)
        blocks = {}  # the port's frames prepared at full size, 512 at a time (all of them would not fit)

        def rows(lo, hi):
            out = []
            for b in range(lo // 512, (hi - 1) // 512 + 1):
                if b not in blocks:
                    if len(blocks) > 6:
                        blocks.pop(min(blocks))
                    blocks[b] = prep(port[b * 512:(b + 1) * 512], 1)
                out.append(blocks[b])
            return np.concatenate(out)[lo - lo // 512 * 512:][:hi - lo]

        n = np.zeros(len(vt), int)
        sc = np.zeros(len(vt))
        un = np.zeros(len(vt))
        for i in range(len(vt)):
            lo = max(0, pred[i] - a.window)
            hi = min(nport, pred[i] + a.window + 1)
            k, sc[i], un[i] = match_window(prep(vt[i], 1)[0], rows(lo, hi), pred[i] - lo, a.window)
            n[i] = lo + k
        return n, sc, un

    n, sc, un = fine(vt, idx, path)
    if not a.warp:
        # the warp fitted on the frames matched surely: a few rounds of coordinate search
        good = np.where((sc > np.percentile(sc, 60)) & (un > 0.02))[0]
        pick = good[np.linspace(0, len(good) - 1, min(80, len(good))).astype(int)]
        raw = [None] * len(pick)
        want = dict((int(i), k) for k, i in enumerate(pick))
        for i, f in enumerate(read_video(a.video, vw, vh, a.start, dur)):
            if i in want:
                raw[want[i]] = f.copy()
            if i > pick.max():
                break
        target = port_prepped(port[n[pick]], 1) if len(pick) else None

        def fit(p):
            w = Warp(*p, tw, th, vw, vh)
            return float(np.mean(np.sum(prep(np.array([w(f) for f in raw]), 1) * target, 1)))

        p = list(warp.p)
        best = fit(p)
        for size in (2.0, 1.0, 0.5, 0.25):
            for _ in range(3):
                moved = False
                for k in range(4):
                    d = size if k < 2 else size / tw * (1 if k == 2 else tw / th)
                    for sgn in (1, -1):
                        q = list(p)
                        q[k] += sgn * d
                        v = fit(q)
                        if v > best:
                            best, p, moved = v, q, True
                if not moved:
                    break
        warp = Warp(*p, tw, th, vw, vh)
        print('warp fitted: %.2f %.2f %.4f %.4f (mean correlation %.3f)' % (*warp.p, best))
        vt = video_thumbs(warp)
        idx, path = anchors(vt)
        n, sc, un = fine(vt, idx, path)
    np.savez_compressed(a.out, t=times, n=n, score=sc, unique=un, late=late if late is not None else np.zeros(0),
                        rate=a.rate, warp=np.array(warp.p), nport=nport)
    print('matched %d frames: median correlation %.3f, %d sure (unique by > 0.02)' % (len(n), np.median(sc),
          int((un > 0.02).sum())))


# --- the count ---------------------------------------------------------------------------------------------------

def sure_mask(d):
    return (d['unique'] > 0.02) & (d['score'] > max(0.5, np.percentile(d['score'], 10)))


def segments(t, n, ok, rate, min_len=5.0):
    """runs of sure frames where the port's frame moves on as the board's could: never back, at most 2.2 x the rate
    (a cut, a static screen or the attract's next loop starts another)"""
    ii = np.where(ok)[0]
    out = []
    if not len(ii):
        return out
    s = p = ii[0]
    for i in ii[1:]:
        dn = n[i] - n[p]
        if dn < -2 or dn > 2.2 * rate * (t[i] - t[p]) + 6:
            out.append((s, p))
            s = i
        p = i
    out.append((s, p))
    return [(s, e) for s, e in out if t[e] - t[s] >= min_len]


def report(a):
    d = dict(np.load(a.npz))
    t, n, late = d['t'], d['n'], d['late']
    rate = float(a.rate or d['rate'])
    ok = sure_mask(d)
    lc = np.concatenate([[0], np.cumsum(late)]) if len(late) else None
    idx = np.arange(len(t))
    rows = []
    segs = []
    cuts = [float(x) for x in a.split.split(',')] if a.split else []
    for s, e in segments(t, n, ok, rate, a.min_len):
        for c in cuts:  # (a segment cut where the scene changes: the title from the demo)
            if t[s] < c < t[e]:
                k = int(np.searchsorted(t, c))
                segs.append((s, k - 1))
                s = k
        segs.append((s, e))
    for s, e in segs:
        if not (ok & (idx >= s) & (idx <= e)).sum() >= 16:
            continue
        m = ok & (idx >= s) & (idx <= e)
        ii = np.where(m)[0]
        a0, a1 = ii[:8], ii[-8:]  # each end: the median over 8 sure frames (one is +-1 frame)
        D = rate * t - n
        dD = float(np.median(D[a1]) - np.median(D[a0]))
        ta, tb = float(np.median(t[a0])), float(np.median(t[a1]))
        na, nb = int(np.median(n[a0])), int(np.median(n[a1]))
        pl = int(lc[nb] - lc[na]) if lc is not None else 0
        rows.append(dict(t0=t[s], t1=t[e], T=tb - ta, na=na, nb=nb, pl=pl, dD=dD, sure=int(m.sum()),
                         slope=float(np.polyfit(t[m], n[m], 1)[0])))
    # the video's clock: R + x board frames a second of video, x making board = port over the calm segments (least
    # squares); those are the segments named by --calm (their numbers), or else those the port is never late in
    if a.calm:
        calm = [int(k) for k in a.calm.split(',')]
    else:
        calm = [k for k, r in enumerate(rows) if r['pl'] == 0]
    x = 0.0
    if a.chain:
        x = rate * (a.chain - 1)
    elif calm:
        T = np.array([rows[k]['T'] for k in calm])
        dD = np.array([rows[k]['dD'] for k in calm])
        x = float(-(dD * T).sum() / (T * T).sum())
        res = dD + x * T
        print('video clock from segments %s: %.4f board frames a second of video (x %.5f of %.4f); their residuals %s'
              % (','.join(map(str, calm)), rate + x, (rate + x) / rate, rate, ' '.join('%+.1f' % r for r in res)))
    print('| # | video s | port frames | port late | board late | board - port | sure |')
    print('|---|---|---|---|---|---|---|')
    tp = tb = 0.0
    for k, r in enumerate(rows):
        diff = r['dD'] + x * r['T']
        tp += r['pl']
        tb += r['pl'] + diff
        print('| %d%s | %.1f-%.1f | %d-%d | %d | %.1f | %+.1f | %d |' % (k, '*' if k in calm and not a.chain else '',
              r['t0'], r['t1'], r['na'] + 1, r['nb'] + 1, r['pl'], r['pl'] + diff, diff, r['sure']))
    print('| all | | | %d | %.1f | %+.1f | |' % (tp, tb, tb - tp))
    if a.plot:
        np.savetxt(a.plot, np.column_stack([t, n, (rate + x) * t - n, d['score'], d['unique'], ok]), fmt='%.4f',
                   header='t n D score unique sure')


def sheet(a):
    d = dict(np.load(a.npz))
    port = load_port(a.port)
    nport, th, tw = port.shape
    vw, vh, _, _ = probe(a.video)
    warp = Warp(*d['warp'], tw, th, vw, vh)
    t, n = d['t'], d['n']
    tiles = []
    for s in [float(x) for x in a.at.split(',')]:
        i = int(np.argmin(np.abs(t - s)))
        f = next(read_video(a.video, vw, vh, t[i], 1))
        v = np.clip(warp(f), 0, 255).astype(np.uint8)
        p = np.asarray(port[n[i]])
        tiles.append(np.hstack([v, np.full((th, 2), 255, np.uint8), p, np.full((th, 6), 0, np.uint8)]))
        print('%.2f s: video frame %d -> port frame %d (score %.3f, unique %.3f)' % (t[i], i, n[i] + 1,
              d['score'][i], d['unique'][i]))
    img = np.hstack(tiles)
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'gray', '-s',
                    '%dx%d' % (img.shape[1], img.shape[0]), '-i', '-', a.png], input=img.tobytes(), check=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('register')
    r.add_argument('video')
    r.add_argument('port')
    r.add_argument('out')
    r.add_argument('--late', help='SPELUNK_LATE_LOG (CV1000)')
    r.add_argument('--frame-log', help='--frame-log (68000 boards, PGM)')
    r.add_argument('--rate', type=float, default=60.0184, help='the board\'s refresh (Hz)')
    r.add_argument('--from', dest='start', type=float)
    r.add_argument('--to', dest='end', type=float)
    r.add_argument('--warp', type=lambda s: [float(x) for x in s.split(',')], help='ox,oy,sx,sy (no fit)')
    r.add_argument('--anchor-every', type=int, default=10)
    r.add_argument('--window', type=int, default=24)
    p = sub.add_parser('report')
    p.add_argument('npz')
    p.add_argument('--rate', type=float)
    p.add_argument('--calm', help='the segments (numbers, as listed) the video\'s clock is fitted on')
    p.add_argument('--split', help='video times (s) to cut segments at, e.g. where a demo begins')
    p.add_argument('--min-len', type=float, default=5.0, help='the shortest segment counted (s)')
    p.add_argument('--chain', type=float, help='the video\'s clock against real time (from a board of known rate)')
    p.add_argument('--plot', help='a text file: t n D score unique sure, a line per video frame')
    s = sub.add_parser('sheet')
    s.add_argument('npz')
    s.add_argument('port')
    s.add_argument('video')
    s.add_argument('png')
    s.add_argument('--at', required=True)
    a = ap.parse_args()
    {'register': register, 'report': report, 'sheet': sheet}[a.cmd](a)


if __name__ == '__main__':
    main()

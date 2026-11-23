#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""Repeated frames in a 60 fps video of a board: the board's slowdown, read straight off the video
(docs/BOARD-VIDEO.md, "Futari 1.5").

On a CV1000 (60.018 Hz) captured cleanly at 60.000 the chain drops a frame about every 54 s and never doubles one,
so a video frame equal to the one before is a frame the board showed twice: a frame the game was late for. Check
that first: in calm play `runs` must find no repeats. If instead every other frame repeats (a chain that passes 30
pictures a second, doubled to 60), single late frames cannot be seen; then a repeated picture (a run of 3 or more
equal frames) is a picture the board held 3 refreshes or more, and the slowdown is counted by lining the pictures up
with the port instead (follow.py). The places where the doubling changes phase (two new frames in a row) come once
a beat of the board's refresh against the capture's: their period gives the board's rate in video seconds.
`scan` measures how far each frame moved from the one before; `runs` sorts the frames into new and repeated and
gives the run lengths; `late` turns the port's late-frame log into the same shape.

Some captures do not repeat a picture exactly: through a lossy chain the second copy of a picture the board showed
twice still changes a little (owlnonymous's Futari Black Label: a largest block change of 2-18 grey levels, where a
new picture changes 35-200). `held` counts them: frames whose largest block change is under the gap between the two
(found from the histogram, or --threshold), and it says how clean the gap is. Checked on the port's own pictures
encoded as that video was (docs/BOARD-VIDEO.md, "Checking the counters"): `held` finds every late frame and nothing
else in play, from 0.8 to 3.8 Mbit/s. `dips` (a frame under --ratio of both neighbours' change) finds the same late
frames but also 1-4 frames a 10 s that are not late (busy play whose change alternates big/small), more at low
bitrates: use `held`. Neither counts still screens (title, menus, fades), where nothing moves.

    repeats.py scan VIDEO OUT.npz [--crop X,Y,W,H] [--from S] [--to S]
        per video frame: the mean change from the frame before (the picture, grey, halved) and the largest change of
        any 16 x 16 block of it; and a small thumbnail (each 8 x 8 block of the crop averaged), for lining scenes up
    repeats.py runs OUT.npz [--threshold V] [--from S] [--to S] [--every S]
        repeated frames (the largest block change below the threshold: chosen from the histogram if not given),
        counted per stretch of --every seconds, with the run lengths of the new and the repeated frames
    repeats.py beat OUT.npz [--threshold V] [--nominal HZ]
        where a doubled video changes phase (two new frames in a row, in bursts while the phase crosses), the
        bursts' period, and the board's rate in video seconds (the capture's rate + 1 / period)
    repeats.py held OUT.npz [--threshold V] [--from S] [--to S] [--every S] [--min-dips N] [--list]
        repeated frames: the largest block change under the gap between repeats and new pictures (chosen from the
        histogram of --from..--to if not given: play only, no still screens), with the frames near the gap (an
        unclean gap: the count is not to be trusted) and the half-speed stretches
    repeats.py dips OUT.npz [--ratio 0.6] [--min-dips N] [--from S] [--to S] [--every S] [--list]
        repeated frames found as dips in the change (for captures whose repeats are not exact), and the stretches
        of half speed (a dip every other frame, at least --min-dips of them): start, frames, dips
    repeats.py late LATE_LOG [--from N] [--to N] [--every N]
        the same counts for the port's SPELUNK_LATE_LOG (a line a frame: the frame, 1 if late)

Needs numpy and ffmpeg.
"""
import argparse
import functools
import subprocess
import sys

import numpy as np

print = functools.partial(print, flush=True)


def probe_size(video):
    out = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height',
                          '-of', 'csv=p=0', video], capture_output=True, text=True, check=True).stdout
    w, h = out.strip().split(',')[:2]
    return int(w), int(h)


def scan(a):
    vw, vh = probe_size(a.video)
    x, y, w, h = a.crop if a.crop else (0, 0, vw, vh)
    w2, h2 = w // 2, h // 2
    cmd = ['ffmpeg', '-v', 'error']
    if a.start:
        cmd += ['-ss', str(a.start)]
    cmd += ['-i', a.video]
    if a.end:
        cmd += ['-t', str(a.end - (a.start or 0))]
    cmd += ['-vf', 'crop=%d:%d:%d:%d,scale=%d:%d:flags=area' % (w, h, x, y, w2, h2), '-f', 'rawvideo', '-pix_fmt',
            'gray', '-']
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    size = w2 * h2
    B = 16
    bh, bw = h2 // B, w2 // B
    tb = 4  # thumbnail: 4 x 4 of the halved picture
    th, tw = h2 // tb, w2 // tb
    mean, block, thumbs = [], [], []
    prev = None
    i = 0
    while True:
        b = p.stdout.read(size)
        if len(b) < size:
            break
        f = np.frombuffer(b, np.uint8).reshape(h2, w2).astype(np.int16)
        thumbs.append(f[:th * tb, :tw * tb].reshape(th, tb, tw, tb).mean((1, 3)).astype(np.uint8))
        if prev is None:
            mean.append(255.0)
            block.append(255.0)
        else:
            d = np.abs(f - prev)
            mean.append(float(d.mean()))
            block.append(float(d[:bh * B, :bw * B].reshape(bh, B, bw, B).mean((1, 3)).max()))
        prev = f
        i += 1
        if i % 6000 == 0:
            print('%d frames (%.0f s)' % (i, i / 60))
    p.wait()
    np.savez_compressed(a.out, mean=np.array(mean, np.float32), block=np.array(block, np.float32),
                        thumbs=np.array(thumbs), start=a.start or 0.0, crop=np.array([x, y, w, h]))
    print('%d frames scanned' % i)


def run_lengths(rep):
    """lengths of the runs of repeated frames and of new frames, in order"""
    out = {True: [], False: []}
    if not len(rep):
        return out
    k = 1
    for j in range(1, len(rep)):
        if rep[j] == rep[j - 1]:
            k += 1
        else:
            out[bool(rep[j - 1])].append(k)
            k = 1
    out[bool(rep[-1])].append(k)
    return out


def histo(runs, top=6):
    if not runs:
        return '-'
    v, c = np.unique(runs, return_counts=True)
    parts = ['%dx%d' % (vv, cc) for vv, cc in zip(v[:top], c[:top])]
    if len(v) > top:
        parts.append('>%d:%d' % (v[top - 1], c[top:].sum()))
    return ' '.join(parts)


def pick_threshold(block):
    """the valley between the repeated frames' compression noise and real change, on a log histogram"""
    lb = np.log10(np.clip(block[1:], 0.05, 255))
    hist, edges = np.histogram(lb, 120, (np.log10(0.05), np.log10(255)))
    sm = np.convolve(hist, np.ones(5) / 5, 'same')
    lo = np.argmax(sm[:60])                      # the noise's peak (below ~2)
    hi = 60 + np.argmax(sm[60:]) if sm[60:].max() > 0 else 119
    v = lo + np.argmin(sm[lo:hi + 1])
    return float(10 ** edges[v])


def stats(rep, label, every, offset=0.0, unit='s', rate=60.0):
    n = len(rep)
    print('%s: %d frames, %d repeated (%.2f%%)' % (label, n, int(rep.sum()), 100.0 * rep.sum() / max(n, 1)))
    r = run_lengths(rep)
    print('  repeated runs: %s' % histo(r[True]))
    print('  new runs between repeats: %s' % histo(r[False], 8))
    if every:
        step = int(round(every * rate)) if unit == 's' else int(every)
        print('| from %s | frames | repeated | runs of 1 | of 2 | of 3+ | alternating (1 new, 1 repeat) |' % unit)
        print('|---|---|---|---|---|---|---|')
        for s in range(0, n, step):
            q = rep[s:s + step]
            rr = run_lengths(q)[True]
            alt = int(sum(1 for j in range(2, len(q)) if q[j] and not q[j - 1] and q[j - 2]))
            if q.sum():
                at = offset + (s / rate if unit == 's' else s)
                print('| %s | %d | %d | %d | %d | %d | %d |' % (('%.1f' % at) if unit == 's' else '%d' % at, len(q),
                      int(q.sum()), rr.count(1), rr.count(2), sum(1 for x in rr if x >= 3), alt))


def runs(a):
    d = np.load(a.npz)
    block, mean = d['block'], d['mean']
    start = float(d['start'])
    thr = a.threshold or pick_threshold(block)
    t = start + np.arange(len(block)) / 60.0
    sel = np.ones(len(block), bool)
    if a.start is not None:
        sel &= t >= a.start
    if a.end is not None:
        sel &= t < a.end
    rep = (block < thr)[sel]
    rep[0] = rep[0] and sel.argmax() > 0
    print('threshold %.2f (largest 16x16 block change, grey levels); median block change of new frames %.1f, of '
          'repeats %.2f' % (thr, float(np.median(block[sel][~rep])) if (~rep).any() else 0,
                            float(np.median(block[sel][rep])) if rep.any() else 0))
    near = ((block > thr / 2) & (block < thr * 2))[sel]
    print('frames near the threshold (x0.5-x2): %d' % int(near.sum()))
    stats(rep, 'video %.1f-%.1f s' % (t[sel][0], t[sel][-1]), a.every, float(t[sel][0]))
    if a.list:
        idx = np.where(rep)[0]
        for k in idx:
            print('%.3f %d %.2f' % (t[sel][k], k, block[sel][k]))


def beat(a):
    d = np.load(a.npz)
    block = d['block']
    thr = a.threshold or pick_threshold(block)
    rep = block < thr
    t0 = float(d['start'])
    nn = np.where(~rep[1:] & ~rep[:-1])[0] + 1
    if not len(nn):
        print('no two new frames in a row: the video is not doubled')
        return
    t = t0 + nn / 60.0
    bursts = [[t[0]]]
    for x in t[1:]:
        if x - bursts[-1][-1] < 5:
            bursts[-1].append(x)
        else:
            bursts.append([x])
    c = np.array([np.median(b) for b in bursts])
    # the period first from the gaps between bursts (the most common), then each burst numbered by it and the
    # whole fitted; bursts off the beat (heavy slowdown, still screens) are dropped
    gaps = np.diff(c)
    period = float(np.median(gaps[gaps > 10])) if (gaps > 10).any() else float(c[-1] - c[0])
    keep = np.ones(len(c), bool)
    p = (period, c[0])
    for _ in range(4):
        k = np.round((c - p[1]) / p[0]).astype(int)
        res = c - (p[1] + p[0] * k)
        keep = np.abs(res) < 1.5
        if keep.sum() < 2:
            break
        p = np.polyfit(k[keep], c[keep], 1)
    if keep.sum() < 2:
        print('%d bursts of two new frames in a row, too few on a beat to fit (a 60-picture chain, or a short '
              'sample): %s' % (len(c), ' '.join('%.1f' % x for x in c[:20])))
        return
    p = np.polyfit(k[keep], c[keep], 1)
    res = c - np.polyval(p, k)
    print('%d bursts, %d on the beat: period %.3f s (residuals %.2f s rms)' % (len(c), int(keep.sum()), p[0],
          float(np.sqrt(np.mean(res[keep] ** 2)))))
    print('off the beat: %s' % (' '.join('%.1f' % x for x in c[~keep]) or 'none'))
    print('board refreshes a second of video: %.4f (capture %.3f + 1/period)' % (a.nominal + 1 / p[0], a.nominal))


def find_dips(block, ratio, exact):
    """repeated frames: an exact repeat (block change under `exact`), or a dip under `ratio` of both neighbours"""
    b = block.astype(np.float64)
    rep = b < exact
    d = np.zeros(len(b), bool)
    d[1:-1] = (b[1:-1] < ratio * b[:-2]) & (b[1:-1] < ratio * b[2:])
    rep |= d
    rep[0] = False
    return rep


def half_speed(rep, min_dips):
    """stretches with a repeat every other frame: (first repeat, last repeat, repeats) for each of min_dips+"""
    idx = np.where(rep)[0]
    out = []
    if not len(idx):
        return out
    s = p = idx[0]
    n = 1
    for k in idx[1:]:
        if k - p == 2:
            n += 1
        else:
            if n >= min_dips:
                out.append((s, p, n))
            s, n = k, 1
        p = k
    if n >= min_dips:
        out.append((s, p, n))
    return out


def pick_gap(block):
    """the threshold between repeated frames (noise, under ~20) and new pictures (35+): the middle of the emptiest
    stretch of a log histogram between the repeats' peak and the new pictures' peak"""
    lb = np.log10(np.clip(block, 0.05, 255))
    edges = np.linspace(np.log10(0.05), np.log10(255), 121)
    hist = np.histogram(lb, edges)[0].astype(float)
    sm = np.convolve(hist, np.ones(3) / 3, 'same')
    centre = (edges[:-1] + edges[1:]) / 2
    hi = int(np.argmax(np.where(centre > np.log10(20), sm, -1)))   # new pictures
    lo = int(np.argmax(np.where(centre < np.log10(12), sm, -1)))   # repeats
    seg = sm[lo:hi + 1]
    m = seg.min()
    low = np.where(seg <= m + 0.5)[0]
    # the longest run of bins at the minimum; its middle
    runs, s0 = [], low[0]
    for x, y in zip(low[:-1], low[1:]):
        if y != x + 1:
            runs.append((s0, x))
            s0 = y
    runs.append((s0, low[-1]))
    r0, r1 = max(runs, key=lambda r: r[1] - r[0])
    return float(10 ** centre[lo + (r0 + r1) // 2])


def held(a):
    d = np.load(a.npz)
    block = d['block'].astype(np.float64)
    start = float(d['start'])
    t = start + np.arange(len(block)) / 60.0
    sel = np.ones(len(block), bool)
    if a.start is not None:
        sel &= t >= a.start
    if a.end is not None:
        sel &= t < a.end
    sel[0] = False
    thr = a.threshold if a.threshold is not None else pick_gap(block[sel])
    rep = (block < thr) & sel
    near = ((block > thr / 1.5) & (block < thr * 1.5) & sel).sum()
    print('held frames: largest block change under %.1f; median %.1f held, %.1f new; frames near the gap '
          '(x0.67-x1.5): %d of %d' % (thr, float(np.median(block[rep])) if rep.any() else 0,
                                     float(np.median(block[sel & ~rep])), int(near), int(sel.sum())))
    stats(rep[sel], 'video %.1f-%.1f s' % (t[sel][0], t[sel][-1]), a.every, float(t[sel][0]))
    hs = half_speed(rep, a.min_dips)
    print('half-speed stretches (a repeat every other frame, %d+ repeats): %d, %d repeats in them' % (
        a.min_dips, len(hs), sum(x[2] for x in hs)))
    if a.list:
        for k in np.where(rep)[0]:
            print('%.3f %d %.2f' % (t[k], k, block[k]))


def dips(a):
    d = np.load(a.npz)
    block = d['block']
    start = float(d['start'])
    exact = a.threshold if a.threshold is not None else min(pick_threshold(block), 1.0)
    rep = find_dips(block, a.ratio, exact)
    t = start + np.arange(len(block)) / 60.0
    sel = np.ones(len(block), bool)
    if a.start is not None:
        sel &= t >= a.start
    if a.end is not None:
        sel &= t < a.end
    print('dips under %.2f of both neighbours, or an exact repeat (block change under %.2f)' % (a.ratio, exact))
    stats(rep[sel], 'video %.1f-%.1f s' % (t[sel][0], t[sel][-1]), a.every, float(t[sel][0]))
    hs = [x for x in half_speed(rep & sel, a.min_dips)]
    print('half-speed stretches (a repeat every other frame, %d+ repeats): %d, %d repeats in them' % (
        a.min_dips, len(hs), sum(x[2] for x in hs)))
    print('| starts (video s) | frames | repeats |')
    print('|---|---|---|')
    for s0, s1, n in hs:
        print('| %.2f | %d | %d |' % (t[s0 - 1], s1 - s0 + 2, n))
    if a.list:
        for k in np.where(rep & sel)[0]:
            print('%.3f %d %.2f' % (t[k], k, block[k]))


def late(a):
    fr, lt = [], []
    for line in open(a.log):
        p = line.split()
        if len(p) >= 2:
            fr.append(int(p[0]))
            lt.append(int(p[1]) != 0)
    fr, lt = np.array(fr), np.array(lt)
    sel = np.ones(len(fr), bool)
    if a.start is not None:
        sel &= fr >= a.start
    if a.end is not None:
        sel &= fr < a.end
    stats(lt[sel], 'port frames %d-%d' % (fr[sel][0], fr[sel][-1]), a.every, float(fr[sel][0]), unit='frames')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('scan')
    s.add_argument('video')
    s.add_argument('out')
    s.add_argument('--crop', type=lambda v: [int(x) for x in v.split(',')], help='x,y,w,h of the picture')
    s.add_argument('--from', dest='start', type=float)
    s.add_argument('--to', dest='end', type=float)
    r = sub.add_parser('runs')
    r.add_argument('npz')
    r.add_argument('--threshold', type=float)
    r.add_argument('--from', dest='start', type=float)
    r.add_argument('--to', dest='end', type=float)
    r.add_argument('--every', type=float, help='a table row per this many seconds')
    r.add_argument('--list', action='store_true', help='each repeated frame: time, index, block change')
    b = sub.add_parser('beat')
    b.add_argument('npz')
    b.add_argument('--threshold', type=float)
    b.add_argument('--nominal', type=float, default=60.0, help='the video\'s frame rate')
    dp = sub.add_parser('dips')
    dp.add_argument('npz')
    dp.add_argument('--ratio', type=float, default=0.6, help='a dip: under this share of both neighbours\' change')
    dp.add_argument('--threshold', type=float, help='an exact repeat: block change under this (default: the '
                    'histogram\'s valley, at most 1)')
    dp.add_argument('--min-dips', type=int, default=25, help='the shortest half-speed stretch listed')
    dp.add_argument('--from', dest='start', type=float)
    dp.add_argument('--to', dest='end', type=float)
    dp.add_argument('--every', type=float, help='a table row per this many seconds')
    dp.add_argument('--list', action='store_true', help='each repeated frame: time, index, block change')
    hd = sub.add_parser('held')
    hd.add_argument('npz')
    hd.add_argument('--threshold', type=float, help='a repeat: block change under this (default: the gap in the '
                    'histogram)')
    hd.add_argument('--min-dips', type=int, default=25, help='the shortest half-speed stretch listed')
    hd.add_argument('--from', dest='start', type=float)
    hd.add_argument('--to', dest='end', type=float)
    hd.add_argument('--every', type=float, help='a table row per this many seconds')
    hd.add_argument('--list', action='store_true', help='each repeated frame: time, index, block change')
    l = sub.add_parser('late')
    l.add_argument('log')
    l.add_argument('--from', dest='start', type=int)
    l.add_argument('--to', dest='end', type=int)
    l.add_argument('--every', type=int, help='a table row per this many frames')
    a = ap.parse_args()
    {'scan': scan, 'runs': runs, 'beat': beat, 'held': held, 'dips': dips, 'late': late}[a.cmd](a)


if __name__ == '__main__':
    main()

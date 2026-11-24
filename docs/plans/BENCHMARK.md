# The CV1000 timing against a board: buffi's benchmark

Current results: [The results now (2026-11-24)](#the-results-now-2026-11-24). First measured 2026-11-19, before the model was fitted to it ([below](#done-2026-11-20) for what changed). [docs/SLOWDOWN.md](../SLOWDOWN.md) says what the port models; this is
how far that is from a real board, measured with buffi's CV1000 benchmark (homebrew, October 2026), which buffi ran on
a real Ibara PCB, on the MiSTer core (ikacore_CV1k) and in MAME:
[Benchmarking CV1000: PCB vs MAME and MiSTer](https://buffis.com/arcade/benchmarking-cv1000-pcb-vs-mame-and-mister/).
The benchmark is buffi's and not part of this repository; nothing of it is committed here. Get it from buffi.

The MAME columns below are buffi's own numbers (from his article). MAME git 8fd33e3 (2026-11-22), built and run here, gives the same 20 numbers to the digit; for shmupmame, MAME 0.275 and the community's CPU settings, see
[Against shmupmame](#against-shmupmame-and-mame-2026-11-23).

## Method

- The autorun build (`mame/autorun/cv1kbench.zip`: Ibara's file names ibara_u4, u2, u23, u24) boots straight into
  RUN ALL and stays on a results screen. `tools/cv1kbench.sh ZIP` unpacks it into `work/cv1kbench/`, writes a copy of
  `games/ibara/game.cfg` with the benchmark's checksums beside it, and runs it on the headless runner twice at once,
  with **Slowdown as the board** off and on (`--timing`), in the board's interpreter (`--cpu interpreter`: Ibara's
  translated code is not this program). Both runs reach the results screen before frame 10,000; the script runs
  12,000 (3.5 minutes on the mini: the timed run is the slower).
- The results screen's lines are kept in main RAM as text, 32 bytes a line (16 of name, 16 of value), at 0C22EA28 in
  this build; the script finds them by the first name and prints the table below. The picture is kept as well
  (`work/cv1kbench/on.png`, `off.png`).
- The benchmark times everything with the SH-3's timer 2 (Pφ/4), so it measures the port in the board's own
  clocks: the tests that only the blitter decides agree with the PCB within 0.4% (GAME FRAME, the sprite tests,
  SCREEN COPY), so the timer and the CPU clock are right.
- To tell the CPU's part from the blitter's, the timer reads were logged with their callers (a local change, not
  kept). The blitter tests run each round as: timer, build the list (CPU), timer, start it and wait (blitter),
  timer; the CPU tests time each kernel on its own. Cycles below are the CPU's (102.4 MHz) and come from those logs,
  or from the rates (102.4 MHz / rate) where a kernel is one loop.

## The results now (2026-11-24)

The port as it ships, with Slowdown as the board on, from `tools/cv1kbench.sh` on current code, after the fitting
and fixes under [Done (2026-11-20)](#done-2026-11-20) and the sections after it. The error is the port's against the
PCB, + where the port is faster. `first run` is the port's column from the 2026-11-19 table below, kept to show what
the fitting changed; MiSTer and MAME are buffi's.

| test | PCB | port now | now vs PCB | port, first run | MiSTer | MAME |
|---|---|---|---|---|---|---|
| RAMP 8X8 SPR (@60) | 4800 | 4850 | +1.0% | 5700 | 5650 | 4875 |
| RAMP 8X8 STILL (@60) | 9085 | 9085 | 0 | 9123 | 9085 | 9123 |
| RAMP 32X32 SPR (@60) | 1170 | 1170 | 0 | 1175 | 1170 | 1175 |
| RAMP CPU LOGIC (@60) | 1503 | 1512 | +0.6% | 2346 | 1769 | 2135 |
| RAMP GAME (@60) | 1694 | 1694 | 0 | 1701 | 1715 | 1701 |
| BLIT LATENCY (µs avg) | 5 | 5 | 0 | 4 (4.45) | 4 | 5 |
| 8X8 SPRITES (kdraw/s) | 603 | 603 | 0 | 605 | 604 | 604 |
| 32X32 SPRITES (kdraw/s) | 77 | 77 | 0 | 77 | 77 | 77 |
| SCREEN COPY (Mpix/s) | 90.6 | 90.7 | +0.1% | 91.0 | 90.6 | 91.0 |
| ALPHA 32X32 (kdraw/s) | 77 | 77 | 0 | 77 | 77 | 77 |
| UPLOAD TO VRAM (kpix/s) | 5632 | 5653 | +0.4% | 7841 | 8198 | 5665 |
| GAME FRAME (fps) | 171.3 | 171.4 | +0.1% | 171.9 | 171.2 | 171.8 |
| BLITTER PAIN (ms/round) | 19.7 | 20.0 | -1.5% (slower) | 19.9 | 19.7 | 19.9 |
| CPU INTEGER (kiter/s) | 3656 | 3656 | 0 | 4653 | 3937 | 4653 |
| CPU DIVIDE (kdiv/s) | 1176 | 1190 | +1.2% | 1365 | 1256 | 1304 |
| CPU MEMORY (MB/s) | 20.1 | 19.9 | -1.0% | 17.6 | 19.7 | 14.7 |
| CPU CACHE HIT (Mload/s) | 17.0 | 17.0 | 0 | 20.4 | 20.4 | 17.0 |
| CPU FLUSH (µs/flush) | 886 | 885 | +0.1% (faster) | 743 | 734 | 604 |
| CPU LIST BUILD (kdraw/s) | 352 | 355 | +0.9% | 433 | 431 | 368 |
| CPU QUICKSORT (sorts/s) | 140 | 149 | **+6.4%** | 150 | 155 | 142 |

Within 1.5% of the PCB on 19 of the 20 (BLITTER PAIN 1.52% slower), the same as the PCB on 9; mean error 0.7%
(MiSTer's 9.5%, MAME's 7.5%). CPU QUICKSORT's +6.4% is looked at under
[CPU QUICKSORT, looked at again](#cpu-quicksort-looked-at-again-2026-11-21).

## The first results (2026-11-19, before the fitting)

`on` is the port with Slowdown as the board, `off` without; the error is `on` against the PCB. For the rows in time
(µs, ms) a smaller number is faster.

| test | PCB | port (on) | port (off) | MiSTer | MAME | on vs PCB |
|---|---|---|---|---|---|---|
| RAMP 8X8 SPR (@60) | 4800 | 5700 | 8150 | 5650 | 4875 | **+18.8%** |
| RAMP 8X8 STILL (@60) | 9085 | 9123 | 9123 | 9085 | 9123 | +0.4% |
| RAMP 32X32 SPR (@60) | 1170 | 1175 | 1175 | 1170 | 1175 | +0.4% |
| RAMP CPU LOGIC (@60) | 1503 | 2346 | 2713 | 1769 | 2135 | **+56.1%** |
| RAMP GAME (@60) | 1694 | 1701 | 1694 | 1715 | 1701 | +0.4% |
| BLIT LATENCY (µs avg) | 5 | 4 (4.45) | 2 (2.49) | 4 | 5 | **about -0.5 to -1 µs** |
| 8X8 SPRITES (kdraw/s) | 603 | 605 | 606 | 604 | 604 | +0.3% |
| 32X32 SPRITES (kdraw/s) | 77 | 77 | 77 | 77 | 77 | 0 |
| SCREEN COPY (Mpix/s) | 90.6 | 91.0 | 91.0 | 90.6 | 91.0 | +0.4% |
| ALPHA 32X32 (kdraw/s) | 77 | 77 | 77 | 77 | 77 | 0 |
| UPLOAD TO VRAM (kpix/s) | 5632 | 7841 | 9335 | 8198 | 5665 | **+39.2%** |
| GAME FRAME (fps) | 171.3 | 171.9 | 171.9 | 171.2 | 171.8 | +0.4% |
| BLITTER PAIN (ms/round) | 19.7 | 19.9 | 19.9 | 19.7 | 19.9 | +1.0% (slower) |
| CPU INTEGER (kiter/s) | 3656 | 4653 | 4876 | 3937 | 4653 | **+27.3%** |
| CPU DIVIDE (kdiv/s) | 1176 | 1365 | 1365 | 1256 | 1304 | **+16.1%** |
| CPU MEMORY (MB/s) | 20.1 | 17.6 | 51.1 | 19.7 | 14.7 | **-12.4%** |
| CPU CACHE HIT (Mload/s) | 17.0 | 20.4 | 25.5 | 20.4 | 17.0 | **+20.0%** |
| CPU FLUSH (µs/flush) | 886 | 743 | 40 | 734 | 604 | **-16.1% (faster)** |
| CPU LIST BUILD (kdraw/s) | 352 | 433 | 613 | 431 | 368 | **+23.0%** |
| CPU QUICKSORT (sorts/s) | 140 | 150 | 181 | 155 | 142 | **+7.1%** |

BLIT LATENCY is shown in whole µs; the port's exact average from the cycle logs is 4.45 µs (on) and 2.49 (off).

In short: **the blitter's model is right** (every test the blitter alone decides is within 1% of the PCB, and
MAME's PR #16274 is in it); **the CPU's model is the MiSTer core's (HS3's)** to within 1-2% on most CPU tests, as
it was fitted to be, and so it is too fast where HS3 is (7-28%), plus a few places where it is faster than HS3 too.
The feature on is much nearer the board than off on every test that moves (off, the CPU is 1.5 to 22 times too fast).

## Where each difference comes from

Cycles a loop (102.4 MHz) on each: PCB / HS3 (MiSTer) / the port with the feature on.

**CPU INTEGER: +27%.** One 18-instruction loop (`mul.l`, `sts macl` at once, `shld`, five `rotl`, `dt`, `bf/s`), no
data access: 28.0 / 26.0 / 22.0. Two causes. (1) The model charges code only on the way to a data access, so a loop
with none gets no fetch, branch or multiplier costs at all: on and off differ by one cycle. That is the known gap in
SLOWDOWN.md ("code with no data access at all gets no fetch or branch costs"), and is the 4 cycles to HS3. Fix: charge
the pipeline at taken branches too (or at least backward ones: the walk the model already does, run at the branch
instead of the next access), and give `mul.l`/`dmul`'s result to `sts mach/macl` the multiplier's latency. (2) The 2
cycles from HS3 to the PCB are HS3's; the cache-hit test below gives one of them a probable place.

**CPU DIVIDE: +16%.** A call per division to a `div1` routine (jsr, rts, no data access): 87.1 / 81.5 / 75.0 (75.0
off too). Same as above: the model sees none of it, HS3 6.5 cycles more, the PCB 12 more. Fix as (1); the rest is
HS3's.

**RAMP CPU LOGIC: +56% (+33% against MiSTer).** Each object runs a 40-round loop of 9 instructions (`mul.l` then
`sts macl` at once, `shld`, `dt`, `bf/s`) with no data access, then writes one draw. It is CPU INTEGER's loop again,
40 times an object, so it shows the same gap most: the model leaves the whole loop at its plain count. Fix as (1).

**CPU CACHE HIT: +20%.** A pointer chase in a 4-instruction loop: `shll2 r0; dt; bf/s` with `mov.l @(r0,r1),r0` in
the delay slot, whose result the loop's first instruction reads: 6.02 / 5.02 / 5.02 (MAME 6.02). Every load hits.
The model and HS3 agree; the PCB takes exactly one cycle more a round. So the board has a stall neither has: most
likely the load-use cycle when the load is a delayed branch's slot and the use is the branch target's first
instruction (the model's load-use rule only looks at the next instruction in the straight line), or a taken branch
costing one more than HS3 says. A loop each way on the PCB would tell which; buffi's benchmark has the first. Fix: the
extra cycle in the pipeline model for that case, then check against CPU INTEGER (one taken branch a round: the PCB's
2 cycles over HS3 there).

**CPU QUICKSORT: +7%.** Cached data, branchy code: 731,000 / 661,000 / 683,000 cycles a sort.
The port is a little slower than HS3 here; the gap to the PCB is the branch and load-use cycles above, spread out.

**CPU MEMORY: -12% (the port too slow).** The rate follows the first of the test's three timed kernels alone (its on/off
ratio is the rate's; the other two kernels' are not): 32,768 rounds of a load from P1 (cached, a new line every 4 rounds) and a store to P1
(write-through), 8 instructions: 20.4 / 20.8 / 23.0. The PCB and HS3 agree; the model is 2.6 cycles a round too slow,
about 10 cycles each time the load misses after a store. SLOWDOWN.md lists back-to-back misses as 2 cycles cheaper on
HS3; this is bigger, and is the line fill waiting for the posted write-through write: the fill is made to wait the
whole 14-cycle spacing of writes before starting its 18. Fix: let a fill begin when the write's bus cycle is done
(the "SDRAM read 12 cycles after a write" rule the model already has for uncached reads), fitted on HS3 with a
store-then-miss loop.

**UPLOAD TO VRAM: +39%.** Each round builds one list of 32 uploads of 32 x 32 pixels (66 KB, written by the CPU
through P2, uncached) and runs it. In the port (on) a round is 235,700 cycles of building and 191,700 of blitter
(33,000 stolen from the CPU, 1.546 ms busy: the 64-byte chunk gaps of PR #16274 are there, `kUploadChunkGapNs`), so
4.17 ms; the PCB's round is 5.82 ms (595,800 cycles). The blitter's part is right: buffi measured a single upload
on the PCB for that PR (58.8 µs against the model's 58), BLITTER PAIN (uploads and draws) is within 1%, and the MiSTer
core's own blitter matches every blitter test. So the PCB spends about 404,000 cycles building where the model
spends 235,700: about 24.5 cycles for each longword written uncached against the model's 14.3 (which is its "next
write 14 cycles after the last" spacing, the loop's own work hidden under it). HS3 is as fast as the port here
(MiSTer: 8198). The likely reason: the model (and HS3) post an uncached write and let the CPU go on, which on the
PCB seems to be true only of P1's write-through writes (the cache's write-through buffer); a P2 write holds the CPU
for its bus cycle, the loop's ten instructions then following it (14 + 10 is the 24.5). Fix: uncached (P2, and P0
with the cache off) writes to the SDRAM not posted: the CPU waits the write's 14 cycles; P1 write-through left as it
is. Check it against CPU LIST BUILD and RAMP 8X8 SPR, which move with it, and against the games' HS3 frames
(SLOWDOWN.md's table), which would no longer be within 1.5% of HS3 where a game writes its lists through P2.

**CPU LIST BUILD: +23%. RAMP 8X8 SPR: +19%.** The benchmark writes its draw lists through P2: 291 / 237.6 / 236.5
cycles a draw. 54 cycles more a 20-byte draw on the PCB is about 10 a longword, the same as UPLOAD's build. The
same fix. RAMP 8X8 SPR is the number of 8 x 8 sprites the CPU can list and the blitter draw in a frame; its blitter
side is right (RAMP 8X8 STILL, the same without the CPU's per-sprite work, is within 0.4%), its CPU side is this.

**CPU FLUSH: -16% (the port faster).** The games' flush: a `braf` into P2, 1,024 writes to the cache's address array
(each writing back the line if it is dirty; the test dirties the cache first) and CCR.CF: 88.6 / 73.4 / 74.3 cycles
an entry. The loop runs from P2, so each instruction pair is an uncached read from the SDRAM; with the dirty lines'
write-backs it is all uncached SDRAM traffic, where the PCB is slower than HS3 above too. Fix: the uncached-write
change may cover part of it (the write-backs are not posted either); then fit the uncached fetch (12 cycles a pair
now) on a P2 loop timed on the PCB.

**BLIT LATENCY: 4.45 µs against the PCB's 5 (shown rounded: 4.5 to 5.5).** Start a one-draw list and poll the
status register until ready: 456 cycles in the port (the blitter's busy time for one small draw, the register writes and the polls at 27 cycles
each). The board takes somewhere between 5 and 107 cycles more.
Likely: the blitter's start (requesting the bus and reading the list's first 64 bytes, as before each upload chunk:
about 0.7-1.1 µs) is not charged. (MAME's blitter model is the port's, so its 5 comes from its CPU's costs or its scheduling.) Fix: a
fixed start time for every list (one chunk read, `kChunkReadNs`, or the bus request's 1.13 µs); it moves the games'
lists by a microsecond each, which is nothing a frame. A finer number needs the menu build's latency test, which may
show more digits, or the benchmark's own count in RAM.

**Not seen:** the SDRAM's refresh (not modelled; a few cycles in 3,072, under 0.5% anywhere) and the blitter taking
the bus to read its draw list while the CPU runs: no test here shows it beyond the 1%.

## What to do, in order of what it buys

1. Uncached SDRAM writes not posted (UPLOAD, LIST BUILD, RAMP 8X8 SPR, perhaps FLUSH): the biggest error the games
   meet, since they all write their draw lists. Check the games' frames on HS3 again after it: HS3 would then be
   wrong there, and the PCB is the reference.
2. Pipeline costs for code without data accesses, and the multiplier's latency (INTEGER, DIVIDE, CPU LOGIC):
   brings the port to HS3 on those.
3. The extra cycle of the cache-hit loop (CACHE HIT, part of INTEGER and QUICKSORT): the first thing past HS3 that
   the PCB shows; needs one more measurement to place.
4. The line fill after a write-through write (MEMORY): the one place the port is slower than both.
5. The blitter's start time (BLIT LATENCY).

After each: `tools/cv1kbench.sh` again, and SLOWDOWN.md's checks (`TIMING=1 tools/check.sh`, the HS3 frames).

## Done (2026-11-20)

All five of the fixes above are in machine/bus_timing.h (and sh3.cpp, sh3.h, dispatch.cpp, translate.py, cv1000.cpp,
blitter.h; save states are now "spelunkomp state 5"); docs/SLOWDOWN.md says what each rule is:

1. Every branch taken is an event of the model (`TimeBranch`, called by the interpreter and by translated code at the
   same point), so the code between events runs straight and is stepped exactly, at its plain cycles.
2. Pipeline: early target fetch (two cycles before decode: gives the slot-load cycle of CPU CACHE HIT), STS MACx
   load-use, the multiplier (manual 10.2.4) one cycle longer than the manual and holding the cache's port while it
   works (fits CPU INTEGER and RAMP CPU LOGIC together).
3. Uncached SDRAM writes held (10 cycles), uncached accesses keep the SDRAM busy 10 cycles longer than HS3 (reads 22,
   writes 24); P1 write-through left posted as before.
4. The write-back buffer (manual 5.3.5): dirty line moved in (4 cycles), fill first, write-back (18) after; the
   address-array flush writes through it. (The benchmark runs with CCR = 5: P1 copy-back, not write-through.)
5. Blitter list start 1.13 µs, with the feature on only (off: MAME's count, hashes unchanged).

And one more, looked for to explain CPU DIVIDE: a PR written right before an instruction that reads it (RTS, STS PR)
holds that one, 3 cycles after LDS.L @Rm+,PR and 1 at a call's target (the manual says LDS.L to PR makes the next PR
reader wait, without the number; the numbers, and the call's case, are HS3's).

The table now (the model on, the board's interpreter, `tools/cv1kbench.sh`; the blitter rows with the VRAM's
76.8 MHz clock and the line fetch on the screen's own cadence since 2026-11-23, docs/SLOWDOWN.md, "The blitter's clock
and line fetch": before that they were the 0.4% fast of every MAME-derived blitter, as in the shmupmame table below):

| test | PCB | port (on) | on vs PCB | before this work (2026-11-19) |
|---|---|---|---|---|
| RAMP 8X8 SPR (@60) | 4800 | 4850 | +1.0% | +18.8% |
| RAMP 8X8 STILL (@60) | 9085 | 9085 | 0 | +0.4% |
| RAMP 32X32 SPR (@60) | 1170 | 1170 | 0 | +0.4% |
| RAMP CPU LOGIC (@60) | 1503 | 1512 | +0.6% | +56.1% |
| RAMP GAME (@60) | 1694 | 1694 | 0 | +0.4% |
| BLIT LATENCY (µs avg) | 5 | 5 | 0 | 4 |
| 8X8 SPRITES (kdraw/s) | 603 | 603 | 0 | +0.3% |
| 32X32 SPRITES (kdraw/s) | 77 | 77 | 0 | 0 |
| SCREEN COPY (Mpix/s) | 90.6 | 90.7 | +0.1% | +0.4% |
| ALPHA 32X32 (kdraw/s) | 77 | 77 | 0 | 0 |
| UPLOAD TO VRAM (kpix/s) | 5632 | 5653 | +0.4% | +39.2% |
| GAME FRAME (fps) | 171.3 | 171.4 | +0.1% | +0.4% |
| BLITTER PAIN (ms/round) | 19.7 | 20.0 | 1.5% slower | 1.0% slower |
| CPU INTEGER (kiter/s) | 3656 | 3656 | 0 | +27.3% |
| CPU DIVIDE (kdiv/s) | 1176 | 1190 | +1.2% | +16.1% |
| CPU MEMORY (MB/s) | 20.1 | 19.9 | -1.0% | -12.4% |
| CPU CACHE HIT (Mload/s) | 17.0 | 17.0 | 0 | +20.0% |
| CPU FLUSH (µs/flush) | 886 | 885 | 0.1% faster | 16.1% faster |
| CPU LIST BUILD (kdraw/s) | 352 | 355 | +0.9% | +23.0% |
| CPU QUICKSORT (sorts/s) | 140 | 149 | +6.4% | +7.1% |

Every test is within 1.5% of the board but CPU QUICKSORT, 6.4% fast. No rule the manual or HS3 gives was found for
it. Tried (with the benchmark's kernels read from its program, locally): a conditional branch not taken costing a
cycle more when the instruction before it set T brings QUICKSORT to +0.7% (and RAMP 8X8 SPR, RAMP CPU LOGIC and LIST
BUILD to the board, nothing else moving), but the manual gives a branch not taken one cycle and lists no such wait,
and HS3 forwards T without one: a fit, not a rule, so it is left out. The same with every branch not taken: the same
result, the same objection. The cache's fill order and replacement (manual 5.2, 10.15) are as the model has them, and
QUICKSORT has too few misses (about 200 fills a sort) for them to matter. What would settle it: a loop on the PCB with
a compare then a branch not taken, against one with an instruction between them. That loop is now written:
[below](#a-board-test-for-the-cpus-rules-2026-11-21).

The model's speed: making it fit cost processor time (the events at every branch taken); the paths it keeps are now
made ready to run once, and kept 8,192 at a time, which brings it back within 15% of origin/main's (SLOWDOWN.md, "What
it costs"), with every count the same (the benchmark's RAM at the end and the per-frame hashes of the checks' play with
the model on, all fourteen CV1000 games, equal before and after).

Checks (2026-11-20, on the MacBook Air): `GAME=<id> TIMING=1 tools/check.sh ROMS 6500` and the same without TIMING
passed on all fourteen CV1000 games; with the model off, each game's per-frame hashes over that play (`--cpu blocks
--hash-log`) are origin/main's; `tools/check_tools.sh ROMS mushisam`, `tools/audit.sh`, `tools/sync-common.sh
--check` and the CI substitute (`make games launcher` with no translations, `tools/ci_smoke.sh`) passed.

## CPU QUICKSORT, looked at again (2026-11-21)

**The kernel** (read from the benchmark's program locally; described here, not copied): a median-of-three quicksort of
4,096 32-bit integers (16 KB, in P1 with the cache copy-back) that leaves runs of 16 or fewer to an insertion sort,
recursing into the smaller half. The driver fills the array from an LCG, reads timer 2, calls the sort, reads the
timer, checks the order; 358 sorts in the run. Per sort, counted over all 358 with the interpreter's model on (a
local profile of every instruction's count and of the model's charges at it, and an instruction trace of one sort):

| part of the sort | instructions | plain count with taken-branch bubbles | the model | data accesses | branches not taken |
|---|---|---|---|---|---|
| partition, scan up (a 6-instruction `bf/s` loop) | 73,600 | 81,510 | 86,882 | 12,267 | 4,356 |
| partition, scan down (a 6-instruction `bt/s` loop) | 74,215 | 82,216 | 91,070 | 12,369 | 4,367 |
| partition, the rest (compares, swaps, bounds) | 222,552 | 252,149 | 255,494 | 33,008 | 16,832 |
| insertion sort, inner loop | 100,764 | 112,103 | 141,345 | 24,429 | 13,091 |
| insertion sort, outer loop | 51,545 | 58,547 | 69,070 | 7,773 | 771 |
| calls, median of three, entry and exit | 26,028 | 28,683 | 35,781 | 9,994 | 1,075 |
| **a sort** | **548,703** | **615,208** | **679,641** | **99,840** (64,631 loads, 35,209 stores) | **40,492** |

Branches taken: 59,700 a sort (6,830 `bt`/`bf`, 43,768 `bt/s`/`bf/s`, 9,076 `bra`/`jsr`/`rts`), 18,159 of them to an
odd halfword. Of the 40,492 not taken, 36,149 come right after an instruction that set T (`cmp`, `tst`, `dt`).
Cache fills: about 200. The model's 679,641 plus the time the frame interrupt takes during a sort (about 7,500) is the
port's 149 sorts a second (687,000 cycles); the board's 140 is 731,000 (728,800 to 734,000, the 140 being rounded). So
the board spends **about 44,000 cycles a sort (41,000 to 47,000) more**, over the model's 64,400 beyond the plain
count, mostly in the partition's code: there the model charges 3,300 cycles for 33,000 accesses and 16,800 branches
not taken (its accesses fall in the cycles the paired fetches leave free).

**Why MAME lands close (142, +1.4%) with no pipeline:** its SH7709S timing (src/devices/cpu/sh/sh7709s.cpp,
`access_penalty`, `update_access_cycles`) charges, at every data access, the instruction fetch at the PC and then the
data, each flipping one "port free" flag; the two flips make every data access cost one cycle (a hit), wherever it
falls, and it charges nothing for load-use or fetch contention. QUICKSORT is 615,208 plain-with-bubbles + 99,840
accesses + misses and the interrupt: about 722,000, near the board's 731,000. That is chance: the same rule makes CPU
INTEGER (no accesses) 27% fast and CPU MEMORY 27% slow. It says only that QUICKSORT costs the board about 1.1 cycles
per data access over the plain count, where the model finds 0.65; it does not say where.

**Candidates**, each run through the model on all 20 tests (`tools/cv1kbench.sh`, a local switch, not kept):

| candidate | evidence | QUICKSORT | the other 19 | verdict |
|---|---|---|---|---|
| 1. A conditional branch not taken costs a cycle more when the instruction just before it set T | None. The SH-3 software manual (REJ09B0317-0400) puts conditional branches under "no contention" (table 10.1, p.438; table 10.2, p.446: "—"); BT/BF are decided in ID with the next instruction "also" fetched (§10.4.5, figs 10.41, 10.43, pp.476-478); its only SR rule stalls STC SR, STC.L SR and TRAPA (§10.2.3, p.432). HS3 forwards T. | +0.7% | all within 1.5% (RAMP 8X8 SPR 0, CPU LOGIC -0.6, DIVIDE +0.6, LIST BUILD -0.9) | fits; no document; not taken in |
| 2. Every conditional branch not taken costs a cycle more | None: the manual's 3/1 and 2/1 (BF p.150, BF/S p.151, BT p.161, BT/S p.162; note 4 to table 10.2, p.457: "One state when there is no branch"; SH7709S hardware manual table 2.10, p.46) | +0.7% | the same as 1 (LIST BUILD -1.1) | fits equally; contradicts the manual's cycle counts |
| 3. A write hit in copy-back holds the cache's port a second cycle | None: the hardware manual (REJ09B0081, §5.3.4, p.111) gives no hit cycles | -2.1% | RAMP 8X8 SPR -8.9%, LIST BUILD -10.5%, CPU LOGIC -3.1% | rejected |
| 4. A branch to an odd halfword costs a cycle | The manual: such a target's IF brings one instruction and the next IF is a bus cycle (§10.2.1, fig 10.7, pp.429-430); it costs only where that IF meets an MA, which the model already does | — | — | already in the model (and more: below) |
| 5. The SDRAM's refresh | Hardware manual §10.3.4, p.297: a refresh waits for a bus cycle in progress | under 0.1%: about 200 fills a sort | — | rejected by arithmetic |
| 6. The cache's fill order or replacement | §5.3.1-5.3.2, table 5.2 (6-bit LRU) as modelled; 200 fills | — | — | rejected (2026-11-20) |
| 7. The frame interrupt costing more on the board | The interrupt takes the same share of every test's time | — | would move every CPU test alike | rejected |

Candidates 1 and 2 cannot be told apart by the benchmark (36,149 of the 40,492 branches not taken follow a T setter),
and nothing in the manuals or HS3 supports either. **No documented rule explains the 6%; the model is left as it
is.** What it would mean for the games: [Mushihimesama's late frames](#what-the-6-would-mean-in-play) below.

### What the 6% would mean in play

Mushihimesama over the same 40,000 frames as SLOWDOWN.md's "What it does" (the check's coin and start, then
`--press 4700:40000:0x11 --features out_of_harm=1`, `--timing`), the frames in which the game started no draw list,
per 2,500 frames from frame 5,000, with the model as it is and with candidate 1 (a local switch):

    the model:            1 0 120 11 0 0 63 28 56 88 147 28 17 93   (652; 1,346 over the 40,000)
    with candidate 1:     1 0 120 11 0 0 63 28 56 87 145 28 18 92   (649; 1,343)

(The first line is SLOWDOWN.md's, the same frame for frame.) The rule makes the CPU's work a little longer, so the
state hashes part from frame 97 on, but which frames are late is the same up to frame 28,503; after that the two
runs play differently (519 frames' lateness differ, mostly the play no longer being the same) and the counts stay
within 3 of each other. In these games' frames, where the time goes to the blitter's waits and the lists written
through P2, QUICKSORT's 6% comes to about 0.2% of the late frames: it matters little in play whichever way a board
settles it.

## A board test for the CPU's rules (2026-11-21)

`tools/cv1ktest/cv1ktest.py` (GPL-3.0, written here from the manuals; nothing of the benchmark in it) writes a program
flash image: it sets the clock, the bus and the SDRAM as the games' boot does (the register values in SLOWDOWN.md),
copies its tests to main RAM, turns the cache on (CCR 0D: flushed, P1 copy-back, as the benchmark runs) and runs
each test from P1 with interrupts blocked: a loop of 16 copies of a short body, 4,096 times (65,536 bodies),
timed with timer 0 at Pφ/4 (16 CPU cycles a tick). The ticks of test k go to main RAM at 0C000800 + 4k, and
C0DEC0DE to 0C0008FC when all are done. `tools/cv1ktest/run.sh ROMS` runs it on the port (the model on) and prints
cycles a body, the empty loop's ticks taken off. It has **no display**: to run it on a board (from the program
flash, as buffi ran the benchmark), the loops are to go into a program that can show numbers (buffi's benchmark is
the obvious home), or the RAM read some other way. Nothing of it has run on a board yet.

The tests, and what the model gives (cycles a body) now and with each candidate:

| # | body (r0 = 1, r1 = 2) | the model | with 1 | with 2 | with 3 |
|---|---|---|---|---|---|
| 1 | `nop; nop` | 2 | 2 | 2 | 2 |
| 2 | `cmp/eq r0,r1; bt` (not taken, T set just before) | 2 | **3** | **3** | 2 |
| 3 | `cmp/eq r0,r1; nop; bt` (not taken, an instruction between) | 3 | 3 | **4** | 3 |
| 4 | `nop; bt` (not taken, T from the loop's `dt` far back) | 2 | 2 | **3** | 2 |
| 5 | `cmp/eq r0,r1; bt/s; nop` (not taken) | 3 | **4** | **4** | 3 |
| 6 | `cmp/eq r0,r1; nop; bt/s; nop` (not taken) | 4 | 4 | **5** | 4 |
| 7 | `cmp/eq r0,r0; bt` (taken, to the next instruction) | 4 | 4 | 4 | 4 |
| 8 | `cmp/eq r0,r0; nop; bt` (taken) | 5.5 | 5.5 | 5.5 | 5.5 |
| 9 | `bra; nop` to an even halfword | 3.06 | 3.06 | 3.06 | 3.06 |
| 10 | `bra; nop` to an odd halfword | 4.06 | 4.06 | 4.06 | 4.06 |
| 11 | `mov.l @r2,r3; nop` (a load hit) | 2 | 2 | 2 | 2 |
| 12 | `mov.l r3,@r2; nop` (a store hit, copy-back) | 2 | 2 | 2 | **3.5** |
| 13 | `mov.l @r2,r3; add r3,r4` (load, used at once) | 4 | 4 | 4 | 4 |
| 14 | `mov.l @r2,r3; add r5,r4` (load, not used) | 2 | 2 | 2 | 2 |

What settles each:
- **Candidate 1 is confirmed** if test 2 takes a cycle more than test 1 (3 against 2) while test 4 stays at 2 and
  test 3 at 3 (the instruction between hides it); and test 5 is 4. It is **rejected** if tests 1, 2 and 4 are alike.
- **Candidate 2 is confirmed** if test 4 is 3 (a branch not taken costs 2 cycles whatever set T) and test 3 is 4.
  Rejected if test 4 is 2.
- If tests 1 to 6 all match the model, neither holds, and QUICKSORT's cycles are elsewhere: then tests 11 to 14
  (and 12 against 11 especially: candidate 3, which the benchmark already rejects) and the alignment tests are next.
- **Tests 9 and 10** check a rule the model has now: a branch to an odd halfword costs a cycle even with no data
  access near it (its target fetch and the next pair's fetch meet at the cache's port). The manual says the next IF is
  a bus cycle but costs only where it meets an MA (§10.2.1, figs 10.7, 10.8). If the board shows 9 and 10 alike, the
  model charges QUICKSORT's 18,159 odd targets a sort too much, and the gap is that much wider.
- **Test 8** gives 5.5 in the model because the bodies alternate between the two halves of a longword; the board's
  number checks the target-fetch rule at both alignments.
- **Test 13** against 14 checks the load-use wait (manual §10.2.2, fig 10.9: the slot holding the load's MA and the
  user's EX splits; the model makes it 2 cycles a body here, its load-use cycle and the fetch it pushes); HS3 has none
  for MOV.L.

## Against shmupmame and MAME (2026-11-23)

Players treat shmupmame's CV1000 build as the best software emulation of these boards, so here it is measured with
the same benchmark, beside stock MAME and the settings the community has posted.

### What shmupmame does for the CV1000

- **Release:** shmupmame 5.4 (March 2026; the download's files are dated 2026-03-15 to 03-18), "Mame 0.275 based"
  ([downloads page](https://shmupmame.wordpress.com/download/): Windows 64-bit, Linux x86-64, AppImage). Each
  download carries the changed source files in `src/` (not a whole tree): for the CV1000, `src/mame/cave/cv1k.cpp`,
  `ep1c12.cpp`, `src/devices/cpu/sh/sh7709s.cpp`/`.h` (new), `sh4.h`, `scripts/src/cpu.lua` and `src/emu/validity.cpp`.
- **No per-game speed settings.** The driver changes no clock and has no speed or blitter setting (the CPU is 102.4
  MHz, its clock scale 100%), and the package has no per-game ini or cfg file: `ini/` holds only `mame.ini` (`speed
  1.0`, nothing per game), `plugin.ini` and `ui.ini`. The players' per-game CPU numbers (below) were never shmupmame's.
- **Its own timing:** the CV1000's CPU is a new `SH7709S` device in place of MAME's `SH3BE`: bio_endio's (Jesus Ramos)
  cache and SDRAM model. The 5.4 readme says: 5.3.1 "Added bio_endio's code for accurate cv1k slowdown emulation"; 5.4
  "Fixed a small bug in the cv1k slowdown code ... And included the latest improvements from bio_endio". The file's
  timing code is line for line upstream MAME's `sh7709s.cpp` as of f808f9a7f8 (2026-03-14, PR #15098, the last of
  the five March PRs that began with #15054), fitted to 0.275's SH core. It models a 16 KB 4-way cache with LRU
  replacement. A hit costs nothing. A miss in the SDRAM (areas 2 and 3) costs 1 + 2 x (3 + 3 + tRCD + WCR2's wait)
  CPU cycles, plus any wait for a dirty line's write-back still in progress or for a precharge in the same page. The
  fetches of instructions in opcode groups 0, 4 and 15 go through the cache as well. Its comments list what it
  leaves out: uncached accesses, pipeline and branch costs, TLB. The blitter is 0.275's (PR #10849's timing,
  without the upload gaps of #16274). The CV1000's other changes are for latency, not timing: `screen_update` draws
  from the clip position (single buffering). The build also has a DFK loop-2 start set, `ddpdfkl2`, and turns on
  `akatana` and `ddpsdoj`.
- **The binary against the source:** the Linux binary keeps its symbols. Its `sh7709s_device` functions are the
  published file's, and `access_penalty` disassembles to the same rule and constants (the miss's 3 + 3 + 1, the
  write-back's 3 + 4 + 1, LRU 3). So the build here (MAME 0.275 with those files over it, cv1k only) is shmupmame's
  CV1000 timing, not a reproduction of it. A forum excerpt says a user once found a shipped source older than its
  binary (a Futari bug). That couldn't be checked: the thread refuses automated fetches.
- **Upstream since then:** MAME has gone further than shmupmame 5.4: cache invalidation and write-back (#15144, June),
  background cycles and bank conflicts (#15685, August), cache port contention (#16063, September), and buffi's
  blitter bus-request gaps between uploads (#16274, October). The last release, 0.289 (July 2026), has only part of
  this. buffi's MAME column has all of it: git 8fd33e3 gives his 20 numbers exactly.

**The community's CPU settings** (MAME's Slider Controls CPU overclock, set per game, often beside the old blitter
delay knob, which is gone by 0.275 (the blitter's timing since PR #10849, MAME 0.254), so only the CPU half can be reproduced). The
pages refuse automated fetches (shmups.system11.org, cave-stg.com and neo-source.com answer 403; the RetroPie forum
shows a browser check). So these values come from search-engine excerpts of those pages, and their posts and dates
could not be read:

| game | CPU | with | where |
|---|---|---|---|
| Mushihimesama | 37.6% | blitter 56% | shmups.system11.org, "Slowdown accuracy for cv1k games on MAME is almost perfect" (t=66312) |
| Mushihimesama (Ultra) | 41.77 MHz (40.8%) | blitter 57% | cave-stg.com, "SH3/CV1000 emulation in MAME: CPU and blitter settings" (topic 1839) |
| DFK Black Label | 41.57 MHz (40.6%) | blitter 57% | the same cave-stg thread |
| Futari Black Label | 37 | blitter 56, lr-mame2016 | retropie.org.uk, "[Pi 4] Cave CV1000 Performance Thread" |
| SaiDaiOuJou | 38 (another post 36) | blitter 57, lr-mame2016 | the same RetroPie thread |
| Espgaluda II | 49.76 MHz (48.6%) | blitter 63% | shmups.system11.org, "SH3 Blitter Delay Discussion Thread" |
| any (after the blitter fix, 0.254) | 50% | no blitter delay | RetroPie ("every CV1k game just needs CPU set to 50% now") |
| Ibara Black Label | 50% | blitter off | shmups.system11.org (the exact post not identified) |

The cave-stg thread also says "newest mame versions need 2 MHz less". Five scales were run: 37%, 37.6%, 38%, 40.6%
and 50% (Ibara's own and the common one); Mushihimesama Ultra's 40.8% lies between two of them. The scale is set the same way the slider sets it
(`device_t::set_clock_scale`), through a local environment switch in the driver. The SH-3's timer keeps real time,
so the CPU tests halve at 50% while the blitter tests don't move.

### How it was run

All builds here are cv1k only (`make SUBTARGET=cv1k SOURCES=src/mame/cave/cv1k.cpp,...`, SDL2 and zlib from the
system). Each run is headless: `ibara -nonvram_save -video none -sound none -nothrottle`. A Lua script
(`-autoboot_script`) reads the results screen's 20 lines from main RAM at 200 s of emulated time, the same way
`tools/cv1kbench.sh` reads them at frame 12,000. Nothing of it is kept in this repository. The port column is a fresh
`tools/cv1kbench.sh` run on origin/main acfb73b (after PR #68's blitter IRQ1): the same 20 numbers as the table
above.

### The results

The error is against the PCB, + where the emulator is faster (for the rows in µs and ms, + is slower). BLIT LATENCY
is shown in whole µs.

| test | PCB | port (on) | MAME 0.275 | shmupmame 5.4 | MAME git 8fd33e3 (= buffi's MAME) |
|---|---|---|---|---|---|
| RAMP 8X8 SPR (@60) | 4800 | 4850 (+1.0%) | 9075 (+89.1%) | 7350 (+53.1%) | 4875 (+1.6%) |
| RAMP 8X8 STILL (@60) | 9085 | 9123 (+0.4%) | 9123 (+0.4%) | 9123 (+0.4%) | 9123 (+0.4%) |
| RAMP 32X32 SPR (@60) | 1170 | 1175 (+0.4%) | 1175 (+0.4%) | 1175 (+0.4%) | 1175 (+0.4%) |
| RAMP CPU LOGIC (@60) | 1503 | 1512 (+0.6%) | 2658 (+76.8%) | 2511 (+67.1%) | 2135 (+42.0%) |
| RAMP GAME (@60) | 1694 | 1701 (+0.4%) | 1694 (0) | 1694 (0) | 1701 (+0.4%) |
| BLIT LATENCY (µs avg) | 5 | 5 | 2 | 2 | 5 |
| 8X8 SPRITES (kdraw/s) | 603 | 604 (+0.2%) | 606 (+0.5%) | 605 (+0.3%) | 604 (+0.2%) |
| 32X32 SPRITES (kdraw/s) | 77 | 77 (0) | 77 (0) | 77 (0) | 77 (0) |
| SCREEN COPY (Mpix/s) | 90.6 | 91.0 (+0.4%) | 91.0 (+0.4%) | 91.0 (+0.4%) | 91.0 (+0.4%) |
| ALPHA 32X32 (kdraw/s) | 77 | 77 (0) | 77 (0) | 77 (0) | 77 (0) |
| UPLOAD TO VRAM (kpix/s) | 5632 | 5655 (+0.4%) | 15309 (+171.8%) | 10136 (+80.0%) | 5665 (+0.6%) |
| GAME FRAME (fps) | 171.3 | 171.8 (+0.3%) | 171.9 (+0.4%) | 171.9 (+0.4%) | 171.8 (+0.3%) |
| BLITTER PAIN (ms/round) | 19.7 | 20.0 (+1.5%) | 15.3 (-22.3%) | 15.3 (-22.3%) | 19.9 (+1.0%) |
| CPU INTEGER (kiter/s) | 3656 | 3656 (0) | 4654 (+27.3%) | 4653 (+27.3%) | 4653 (+27.3%) |
| CPU DIVIDE (kdiv/s) | 1176 | 1190 (+1.2%) | 1383 (+17.6%) | 1383 (+17.6%) | 1304 (+10.9%) |
| CPU MEMORY (MB/s) | 20.1 | 19.9 (-1.0%) | 45.5 (+126.4%) | 18.5 (-8.0%) | 14.7 (-26.9%) |
| CPU CACHE HIT (Mload/s) | 17.0 | 17.0 (0) | 20.4 (+20.0%) | 20.4 (+20.0%) | 17.0 (0) |
| CPU FLUSH (µs/flush) | 886 | 885 (-0.1%) | 50 (-94.4%) | 50 (-94.4%) | 604 (-31.8%) |
| CPU LIST BUILD (kdraw/s) | 352 | 355 (+0.9%) | 678 (+92.6%) | 571 (+62.2%) | 368 (+4.5%) |
| CPU QUICKSORT (sorts/s) | 140 | 149 (+6.4%) | 196 (+40.0%) | 195 (+39.3%) | 142 (+1.4%) |
| **mean error (size)** | | **0.8%** | 42.0% | 27.7% | 7.5% |
| **within 2% of the PCB** | | **19 of 20** | 8 | 8 | 14 |

shmupmame 5.4 and MAME 0.275 under the community's CPU settings (error against the PCB only):

| test | port | sm 50% | sm 40.6% | sm 38% | sm 37.6% | sm 37% | 0.275 50% | 0.275 40.6% | 0.275 38% | 0.275 37.6% | 0.275 37% |
|---|---|---|---|---|---|---|---|---|---|---|---|
| RAMP 8X8 SPR | +1.0% | -23.4% | -38.0% | -42.2% | -42.7% | -43.8% | -5.2% | -22.9% | -27.6% | -27.6% | -29.7% |
| RAMP 8X8 STILL | +0.4% | +0.4% | -1.7% | -8.0% | -9.3% | -10.6% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% |
| RAMP 32X32 SPR | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% |
| RAMP CPU LOGIC | +0.6% | -16.5% | -32.3% | -36.6% | -37.2% | -38.5% | -11.6% | -28.1% | -32.9% | -33.5% | -34.8% |
| RAMP GAME | +0.4% | +0.4% | -21.8% | -29.5% | -30.6% | -32.2% | +0.4% | -6.4% | -14.9% | -16.1% | -18.1% |
| BLIT LATENCY (µs) | 5 | 2 | 3 | 3 | 3 | 3 | 2 | 3 | 3 | 3 | 3 |
| 8X8 SPRITES | +0.2% | +0.3% | +0.2% | +0.2% | +0.2% | +0.2% | +0.3% | +0.3% | +0.3% | +0.3% | +0.3% |
| 32X32 SPRITES | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| SCREEN COPY | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% | +0.4% |
| ALPHA 32X32 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| UPLOAD TO VRAM | +0.4% | -5.0% | -22.1% | -26.8% | -27.6% | -28.7% | +47.6% | +21.8% | +14.6% | +13.5% | +11.8% |
| GAME FRAME | +0.3% | -5.6% | -23.4% | -28.3% | -29.0% | -30.1% | +0.4% | -10.0% | -15.8% | -16.6% | -18.0% |
| BLITTER PAIN (ms) | +1.5% | -22.3% | -22.3% | -22.3% | -22.3% | -22.3% | -22.3% | -22.3% | -22.3% | -22.3% | -22.3% |
| CPU INTEGER | 0 | -36.4% | -48.3% | -51.6% | -52.2% | -52.9% | -36.4% | -48.3% | -51.6% | -52.2% | -52.9% |
| CPU DIVIDE | +1.2% | -41.2% | -52.3% | -55.4% | -55.8% | -56.5% | -41.2% | -52.3% | -55.4% | -55.8% | -56.5% |
| CPU MEMORY | -1.0% | -54.2% | -62.7% | -65.2% | -65.7% | -66.2% | +12.9% | -8.5% | -14.4% | -14.9% | -16.4% |
| CPU CACHE HIT | 0 | -40.0% | -51.2% | -54.7% | -55.3% | -55.9% | -40.0% | -51.2% | -54.7% | -54.7% | -55.9% |
| CPU FLUSH (µs) | -0.1% | -88.6% | -85.9% | -84.9% | -84.8% | -84.5% | -88.7% | -86.1% | -85.1% | -85.0% | -84.8% |
| CPU LIST BUILD | +0.9% | -19.0% | -34.4% | -38.4% | -39.2% | -40.1% | -3.7% | -21.9% | -27.0% | -27.8% | -29.0% |
| CPU QUICKSORT | +6.4% | -30.7% | -43.6% | -47.1% | -47.9% | -48.6% | -30.0% | -43.6% | -47.1% | -47.9% | -48.6% |
| **mean error (size)** | **0.8%** | 22.3% | 29.0% | 31.6% | 32.0% | 32.6% | 20.1% | 23.3% | 25.3% | 25.5% | 26.0% |

(CPU FLUSH stays 85-89% short at every scale: the flush runs uncached from P2, which neither model charges, so it
takes 50 µs at 100% and only grows with the scale.)

### Conclusion

**On this benchmark the port is closer to the board than shmupmame on most tests: 14 of 20 closer, 5 equal, 1
farther. Under every CPU setting the community posted it is closer on 14 or 15 of 20 and farther on none.** The port's mean
error is 0.8%, against shmupmame's 27.7% as it ships and 22-33% with the posted settings. The port is within 2% of
the board on 19 tests; shmupmame is within 2% on 8. shmupmame's CPU model only charges cache misses. It brings CPU
MEMORY close (-8%) and cuts UPLOAD TO VRAM by a third and LIST BUILD by a sixth, but everything without misses stays where
plain MAME has it: INTEGER +27%, CACHE HIT +20%, QUICKSORT +39%, CPU LOGIC +67%. Its uncached accesses (FLUSH 94%
short) and its 0.275 blitter (BLITTER PAIN 22% fast) are untouched. No single clock scale fixes that: the CPU rows
move together while the board's errors point both ways.

The 5 equal are the tests the blitter alone decides (RAMP 8X8 STILL, 32X32 SPR, 32X32 SPRITES, SCREEN COPY, ALPHA),
where every MAME-derived model, the port included, gives the same number.

**The one shmupmame (and MAME 0.275) wins: RAMP GAME, 1694 against the port's 1701.** It is not a better model. It
is the 0.4% that every MAME-derived blitter shows on the blitter-only tests (RAMP 8X8 STILL 9123 against 9085,
32X32 SPR 1175 against 1170, SCREEN COPY 91.0 against 90.6, GAME FRAME 171.8 against 171.3), and 0.275's faster
blitter and CPU land on the board's count by chance. At 50% the same build gives 1701 again, and current MAME gives
1701. The MiSTer core matches the board exactly on those blitter-only tests (9085, 1170, 90.6). What we'd change:
the blitter's draw time 0.4% longer (the same scale on its pixel and line costs, checked against the MiSTer core's
timing), which should bring those, GAME FRAME and RAMP GAME to the board's numbers. Done 2026-11-23, not as a scale
but as the two constants MAME rounds (the VRAM's 76.8 MHz clock, and the line fetch on the screen's own 63.594 µs
cadence: docs/SLOWDOWN.md, "The blitter's clock and line fetch"): RAMP 8X8 STILL 9085, 32X32 SPR 1170, RAMP GAME
1694, 8X8 SPRITES 603, SCREEN COPY 90.7, GAME FRAME 171.4. Small as it is, a blitter-bound game's slowdown is steep
in it: Mushihimesama's stage 3 was late 545 frames with the blitter 3% slower against 303.

**Against current MAME** (git 8fd33e3, the same as buffi's column): the port is closer on 8, equal on 10, and
farther on 2: QUICKSORT (+6.4% against MAME's +1.4%: the gap [looked at above](#cpu-quicksort-looked-at-again-2026-11-21),
where MAME's nearness comes from a rule that puts INTEGER 27% fast and MEMORY 27% slow) and BLITTER PAIN (+1.5%
against +1.0%, half a millisecond a round). Current MAME is far ahead of shmupmame 5.4 (7.5% mean against 27.7%). A
shmupmame built on today's MAME would come close to it, but not to the port: its CPU errors (INTEGER +27%, CPU LOGIC
+42%, MEMORY -27%, FLUSH -32%) are those of the model, not of a setting.

**What the benchmark can't show:** it is 20 kernels timed on one board running Ibara's program and settings (CCR
copy-back, the boot's bus registers). A game spends its frame on a mix of these: lists written through P2, uncached
reads, blitter waits. A model can be off on a kernel no game uses much (QUICKSORT's 6% comes to about 0.2% of
Mushihimesama's late frames: [above](#what-the-6-would-mean-in-play)). A per-game CPU scale, too, can be right for
the scenes it was set by ear in (one stage, one ship, one mode) and wrong on every kernel here: 37.6% for
Mushihimesama slows every CPU test by half while the blitter stays as fast. It matches the board where that game
spends its frames on CPU work, and nowhere else. So these tables say which model is closer to the board's
hardware. They don't say that any one game's slowdown in MAME is wrong everywhere; for that, the board videos
([BOARD-VIDEO.md](../BOARD-VIDEO.md)) and the HS3 frames in SLOWDOWN.md are the checks.

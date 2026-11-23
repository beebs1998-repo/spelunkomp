# The 68000's timings against the FPGA core: status (2026-11-20)

Branch `accuracy-68k` (worktree ../spelunkomp-accuracy). What was found and fixed is in docs/SLOWDOWN.md, "The 68000
against the Cave core running"; the harness and its notes are outside the repository, in ../refs/arcade-cave (local
git, branch `harness`, FINDINGS.md; save states since commit 50ec43d).

## Done

- Instruction times, interpreter and translator alike, all against fx68k: ADDQ/SUBQ to memory; MOVE to -(An);
  MOVEM to registers; BCHG/BCLR/BSET on a register below bit 16; ADD/SUB/AND/OR.L and ADDA/SUBA.L from a register or
  an immediate; EOR Dn,Dn; SUBX Dn,Dn; BTST Dn,#imm; DIVU/DIVS by operands; divide by zero, TRAP, CHK's exception.
  An fx68k bench (fx68k alone on memory with no waits, every instruction form of the 12 games' code with two register
  sets, and the branches, jumps, returns, traps and SR instructions) agrees with the port's interpreter on every case.
- The interrupt: recognised in an instruction's last prefetch, 4 cycles after the line rises (`irqSync`,
  `InstructionTail`), with exact runs (`Run(until, mode, exact)`) to the instruction it comes in; its time 36 + the
  acknowledge, 13 to 22 by the E clock (`InterruptTime`, `eclockPhase`). This was the boot's frame 130: the core was
  ~20 cycles behind the port there, from its interrupt (one instruction later, and the E clock's wait), then the
  ADDA.L #imm in the vertical blank's handler took 2 cycles more a call.
- Measurement switches: headless68k `--start-cycle N`, `SPELUNK_CYCLE_TRACE_WRITES=1` (writes in the cycle trace),
  `--ram-log` taking RAM as the bus had it at the blank (`M68K::RamAt`). The old `SPELUNK_ECLOCK` fitting hook is gone.
- All 12 games retranslated; `tools/check68k.sh` on each (the PGM ones also with `--timing`).

## The comparison (DoDonPachi, the core with the program at zero wait, `--release-lead 51`)

    build/ddonpach/spelunk-headless --game games/ddonpach/game.cfg --roms ../cv1k-roms --frames 10400 \
        $(cat ../refs/arcade-cave/sim/harness/play.txt) --frame-lines 272 --blank-lines 32 --start-cycle 15 --ram-log OUT
    python3 ../refs/arcade-cave/sim/harness/compare.py ../spelunkomp-data/cave/lead_play.ram OUT --skip 10FC00-10FFFF

Before: 141 of 10,400 frames equal (to 129, then the idle count apart). Now: 3,124 equal, all frames to 3,127 but
2,979, 2,981 and 2,986 (the idle count one pass apart); from 3,128 the game parts. Frames 130-300 traced against the
core's bus log: the idle loop's writes within half a cycle of the core's in every frame.

## What is left

1. ~~The YMZ280B's interrupts~~: done in step 2 (below).
2. From frame 5,270 (the blank counter 101270 parts), the rest of the 10,400 frames against the z2 core run (it was
   still running when step 2's PR was made; saves every 500 frames), and a busy
   stretch of stage play with slowdown (in the play script: frames ~8,730-8,750, 8,880-8,930 and 9,345-9,560, where
   the port's idle loop gets one pass or none).
3. The other Cave 68000 games, pictures and sound; unify `M68K::writeTime`/`readTime` (MOVE only, on for every game)
   with the PGM's `accessInto` (every access, on for the PGM games: recomp.py `--access-times`).

## Where the data is

- Core runs: ../spelunkomp-data/cave/lead_play.ram (10,400 frames). Save states of the same run made by
  the savable build (`make fastrom-save`, binary build/cavesim-fastrom-save in /tmp/acc/cave/sim/harness, which only
  can load them) in /tmp/acc/saves/play_<frame>.{vlt,side} for frames 125, 2780, 2960, 3100, 3115, 3120 and every
  thousand from 4,000 (scratch: delete when done).
- fx68k bench: /tmp/acc/fxb (gen.py makes the test image from the games' listings, obj_dir/fxbench runs fx68k,
  /tmp/acc/pb/pb the port's interpreter, cmp.py compares).

## Step 2, the YMZ280B (branch ymz-irq, 2026-11-20)

Fixed in the port (default, board-true; docs/SLOWDOWN.md, "The 68000 against the Cave core running"):

- A voice's end raises the interrupt at its own tick of the chip's crystal (88,200 a second), not at the next
  sixteenth of a frame: `Ymz280b::NextIrq` predicts it and `Cave68k::RunFrame` runs the CPU exactly to it (also while
  the line is already up, so a second voice's end after the game reads the status is not late).
- The chip takes a write at the bus cycle the 68000 makes it in (`M68K::writeTime`: a MOVE's write, 8 cycles before
  its end, a long's high word 12; interpreter and translator), and the board reads the blank's bit at the read's bus
  cycle (`M68K::readTime`: a MOVE's source, after its address words, +2 for -(An) and an index).

Why the crystal stays the default: the board's YMZ280B runs from its own 16.9344 MHz crystal; the core clocks it from
the CPU's (a tick every 181.5 cycles, 88,154 a second), an FPGA shortcut, so the core's sound interrupts drift from
the board's by a tick every ~1,900. Matching the core there would be matching the FPGA, not the board.

Measurement (headless68k): `--ymz-ticks C:O:D:V:E[:Si:Sp:Sf[:K]]`: ticks of C cycles from O; a voice's end raises the
interrupt D + V x its number + the slots of the voices before it (Si idle, Sp playing, Sf fetching) after the tick;
E nibbles past the end address; the scheduler comes to a voice K + the same slots into a tick, and a key-on written
after that starts the voice a tick later. The core's: `181.5:-0.5:158:0:2:2.5:5.5:7:157`, fitted on the
interrupt line in the core's bus log (frames 2,970-3,400; every voice end within -1.5..+0.5 cycles).
`SPELUNK_TRACE_SOUND=1` also prints each voice's tick and the states of the voices before it; `SPELUNK_TRACE_IRQ=file`
each change of the interrupt line.

The reference: refs/arcade-cave harness 8c123dc, `CAVE_SIM_SNDROM` (`make fast2-save`): the YMZ's ROM at zero wait
(the SDRAM path jitters each voice end by ±20 cycles, an FPGA artefact). Its 10,400-frame run from power-on (z2):
RAM in /tmp/acc/z2_play.ram, saves /tmp/acc/saves/z2_* (only the binary /tmp/acc/cave/sim/harness/build/cavesim-fast2-save
loads them). `/tmp/acc/cmpz2.sh PARAMS` compares the port with it so far.

Results (work RAM at every blank, stack left out): with the old core and the core-clock model, equal to 3,229; with
z2: 3,272 before the write time, 3,363 with it and the key-on point, then 5,269 with the read time
(of the 5,967 frames the z2 run had made when the PR was opened; it parts at 5,270, the blank counter 101270).

## Step 3, from 5,270 (branch ddp-5270, 2026-11-21)

The z2 run was resumed from its save at 6,000 (`LOADSTATE=/tmp/acc/saves/z2_6000`, the same binary, `--ram-log
/tmp/acc/z2_b.ram`: its record 0 is frame 6,000's, as z2_play.ram's record 6,000) to 10,400, with saves every 500
frames and at 8,700, 8,850 and 9,300 for the busy stretches. Probes were run from the saves with `--bus-log
--bus-frames` (frame 5,269 from z2_5000, 6,294 from z2_6000).

- 5,270: the core's sound interrupt came 362 cycles (two ticks) sooner. The voice was a tune started again over itself
  at 5,028; the port had kept the old sound's place between samples (`Voice::outputPosition`), the core (not MAME, whose key-on keeps `output_pos`)
  start afresh. Fixed in machine/ymz280b.cpp; equal to 6,294.
- 6,294: the idle counter at the blank only, then lone frames: the core took a sound interrupt one instruction later,
  after a `move.l d0,4(a0)` whose last prefetch began 6 cycles after the rise. A bench was then written (fx68k alone,
  outside the repository, in the session's scratch: every instruction form of the 12 games, 8,176, and 500 flow cases,
  the level-1 line raised at every half cycle after a marker write, the stacked PC telling after which instruction it
  was taken; the same image on the port's M68K): 7,481 forms agreed at first; the rest are now in
  `M68K::InstructionTail` and all agree (one invalid EXG encoding that fx68k traps aside).
- The default frame: 272 lines and a 32-line blank (nullobject's Dangun Feveron probe, docs/CAVE68K.md).

Results (with the chip timed as the core's, `--ymz-ticks 181.5:-0.5:158:0:2:2.5:5.5:7:157`, and `--start-cycle 15`):
equal at every frame for all 10,400 (was 5,269), translated and interpreted; late frames the same on both (docs/SLOWDOWN.md).
After #51 (8,342), three more things in the measurement model, found from the bus log (probes from z2_8850 and
z2_9300): the fetching voice's slot (7 cycles, not 5.5), the slots of the tick the interrupt is raised in (not the tick
before), and a key-on before the last tick's late visit taking that tick (frames 8,908 and 9,442; equal to 8,961, then
9,443 alone, then all). With the shipped timing and the core's reset phase: the first 3,127 frames (2,977 the idle
counter only); exactly as shipped: 130 (the idle counter parts at 131, the game at 2,785). The idle loop's phase at a
blank can differ by an instruction without the work RAM showing it: the E clock's 10-cycle acknowledge hides it, and
a later sound interrupt then lands on another instruction (how 8,908 became 8,961).

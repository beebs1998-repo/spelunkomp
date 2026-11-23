# Plan: checking the boards against references (FPGA cores, MAME)

For an agent picking this up. Started (2026-11-19) for Cave's 68000 board: see "Where it stands" below.

## The aim

Show, frame by frame, that each board's port does what the board does, and fix what it doesn't. The same way it
was done for the CPS-2 and CPS-3 ports: read their write-ups before anything else.

- cps2recomp's docs/TIMING.md and docs/ARCADE_ACCURACY.md (https://github.com/tmcleroy/cps2recomp): jtcps2 (jotego's CPS-2 core) simulated with
  Verilator, fed the same inputs as the port, work RAM compared at every vertical blank (5,639 of 5,671 frames
  equal), the board's bus waits and DMA found and modelled as an option (`arcade_timing`), the game's own counters
  used to check the CPU's share of a frame.
- The CPS-3 port's timing work (not yet public): the CPS-3 core (jtcores, jtsim) with local logging changes kept out of the
  repository; main RAM rebuilt from logged writes; spare-time counters compared (505 against the core's 504); a
  bench of small loops; a scripted fight.

## The references

| Board | Reference | Games | Notes |
| --- | --- | --- | --- |
| Cave 68000 | github.com/MiSTer-devel/Arcade-Cave_MiSTer (Josh "nullobject" Bassett, GPL-3.0; SystemVerilog in rtl/, older Chisel in legacy/) | donpachi, ddonpach, dfeveron, esprade, guwange | Built from an ESP Ra.De. board and rwk's DoDonPachi thesis work (gitlab.com/rwk-source/cave_1st_gen). No full-system simulation in the repository: a Verilator harness was written for it, outside this repository (../refs/arcade-cave, branch `harness`, FINDINGS.md): DDR and SDRAM models, inputs, work RAM at every blank, bus logs, save states (`SAVEAT`/`LOADSTATE`). |
| IGS PGM | github.com/wickerwaka/Arcade-IGSPGM_MiSTer (GPL-2.0) | ddpdoj (ddp3), ket, espgal; not ddpdojblk | Has a C++/Verilator simulation (sim/, sim.f) and test ROMs. Check how it handles the IGS027A (MAME simulates it; the ARM's ROM is not dumped). A second core by Eizo Chiu (community copy github.com/hyp36rmax/PGM-Mister-EZIOCHIU, GPL-3.0) models the IGS027A types itself, but its hardware source does not seem to be published: use only what is published. |
| CV1000 | Raki's CV1000 core for MiSTer (a work in progress since mid-2026, test builds for Patreon supporters only, source not published; CV1000-B games run, CV1000-D did not yet). Usable only as a running core on a MiSTer, compared from outside (picture, sound and slowdown captured with the same inputs), not simulated. Otherwise MAME | the 14 CV1000 sets | MAME's cv1k driver (BSD-3) built for that driver alone, scripted (Lua) to dump main RAM at each vertical blank and to take inputs from a file. This compares with MAME's model, not with a board: worth it for the blitter's picture and timing (buffi's research is MAME's source) (the CPU's slowdown model now follows HS3 instead: below). If the owner has a board, a capture from it beats both. |
| CV1000's CPU | github.com/ika-musume/HS3 (HyperSquid3, BSD-2-Clause: a near cycle-accurate SH7709S, Verilator testbenches) and github.com/ika-musume/ikacore_CV1k (BSD-2-Clause: HS3 on a simulated CV1000-B board, no blitter yet; needs vendor memory models: Micron's MT48LC2M32B2 is downloadable, the Macronix flash model was not found) | the CPU's work of a frame, any CV1000 game | Done for the slowdown model (docs/SLOWDOWN.md, "Checked against HS3"; harness in ../refs/hs3-bench, outside the repository): the CPU's frames within 1.5% of HS3. Not covered: the blitter. |

Keep the cores and harnesses outside this repository (e.g. beside it, in ../refs/<core>), as the CPS-2/CPS-3 work did:
only the rules learnt come into spelunkomp, written fresh, with the core credited in THIRD_PARTY_NOTICES.md and the
doc that describes them. Check each licence before reading code you mean to follow (GPL-2.0 vs this project's
GPL-3.0-only: rules and facts are fine, copied code is not without care).

## Where it stands (2026-11-21)

- **Cave's 68000 board, DoDonPachi**: the 68000's times fixed against the core and against fx68k alone (every
  instruction form of the twelve 68000 and PGM games now takes fx68k's time), the interrupt's recognition and E clock
  modelled (docs/SLOWDOWN.md, "The 68000 against the Cave core running"). Then the YMZ280B: a voice's end interrupts at
  the chip's own tick, and the chip takes a write (and the board its blank bit) at the bus cycle a MOVE makes it in.
  The 10,400-frame play script against the core with its sound ROM at zero wait, the chip timed as the core's
  (`--ymz-ticks`, measurement; the port keeps the board's crystal): work RAM equal at every frame from power-on to
  5,269, 38 seconds into stage play (was 141, then 3,127). Then (branch ddp-5270) a key-on restarts a voice's pitch
  phase, the frame is 272 lines by default (nullobject's Dangun Feveron probe), and where every instruction form looks
  at the interrupt lines was checked on fx68k alone and made the same: equal for all 10,400
  frames of the play script, the late frames of its slowdown the same on both, with the chip timed as the core's. With the port's
  own crystal timing it parts at 3,127, from the core's chip clock and its own end-of-voice rule (docs/SLOWDOWN.md).
  The other Cave 68000 games (2026-11-22): ESP Ra.De. equal at 5,997 of 6,000 frames, Guwange at all 1,600 run, the
  late frames the same, after two fixes (any instruction's read at its bus cycle; the sound interrupt in the picture's
  2 ms). Next: Dangun Feveron and DonPachi, Guwange in play, pictures (`--shot`) and sound.
- **The PGM**: not started; plan below.

## Next: the PGM (Arcade-IGSPGM_MiSTer, DoDonPachi Dai-Ou-Jou)

First look (2026-11-20, a shallow clone: github.com/wickerwaka/Arcade-IGSPGM_MiSTer, now
github.com/MiSTer-devel/Arcade-IGSPGM_MiSTer, at e898860): the core has its own Verilator simulation (sim/: an ImGui
front end, sim_core.cpp, a DDR and SDRAM model, games.cpp loading MRA-style sets and decrypting them as rtl/ does,
save states in the core, a `--server`/`--control-stdio` mode to drive it without a window) and fx68k as its 68000.
Dai-Ou-Jou (`ddp3`, "CAVE type1": the cartridge half of the program encrypted) is in its list, as are Ketsui,
Espgaluda and DoDonPachi II. Its IGS027A is rtl/igs027a.sv with a real ARM7 (gamebub-arm) for the games whose ARM ROM
is dumped; for Cave's type-1 cartridges the internal ROM is not dumped, so check what the core runs there (a
recreation?) before trusting anything that goes through the ARM.

The plan, as for Cave's board:
1. Build sim/ headless (the control mode, no window) on this Mac, or write a small harness around its sim_core like
   ../refs/arcade-cave's tb.cpp: power-on, the port's input script, the 128 KB of work RAM at every blank in the
   port's `--ram-log` format (port/headless_pgm.cpp needs `--ram-log`, `--start-cycle` and the frame's length as the
   core has it), save states every few thousand frames, a bus log of chosen frames.
2. Line up the start (the reset's release against the blank) and the frame (the PGM's 264 lines, the core's
   timing in rtl/video_timing.sv) as was done for Cave's (--start-cycle, --frame-lines).
3. Compare work RAM frame by frame over Dai-Ou-Jou's recipe; where it parts, the core's bus log against the port's
   trace of the frame (drift.py, the write-lag method in FINDINGS.md). The things to expect: the IGS023's waits at
   the video memory (the port's Slowdown as the board: compare with it on), the sprite DMA's hold of the bus, the
   level 4 and 6 interrupts' times, the IGS027A's answers and their time, the ICS2115's interrupts.
4. Then Ketsui, Espgaluda, DoDonPachi II the same way.

## Progress

- Cave 68000: DoDonPachi against Arcade-Cave_MiSTer, 68000 timing fixed (docs/SLOWDOWN.md, "The 68000 against the Cave
  core running"); ESP Ra.De. 5,997/6,000 and Guwange 1,600/1,600 against the same core.
- PGM: Dai-Ou-Jou against Arcade-IGSPGM_MiSTer (harness in ../refs/Arcade-IGSPGM_MiSTer, branch `harness`): the board's
  waits, DMA, level-4 interrupt, sound clock and Z80 reads fixed; play and a stage-5 run equal frame by frame but lone
  frames at the blank's edge (docs/SLOWDOWN.md, "The PGM against its core running"). Ketsui and Espgaluda against
  the same (June 2026) core: 3,381 and 3,372 of 3,400 frames equal. Then (2026-11-21) **against a real board**: the
  core's repository has its test ROMs' results from a board (vram_bench, sdr_stress); run on the port they showed the
  June core's video-memory rules wrong, and the port now follows the board (docs/PGM.md, "Against a board": 231 of
  268 results within 0.5%, sdr_stress within 0.005%). On the newer core (MiSTer-devel main, its arbitration fitted to the
  same board): Dai-Ou-Jou 2,697/2,700, Ketsui 3,396/3,400, Espgaluda 3,396/3,400 of their play scripts equal; Ketsui's
  row-scrolled background equal to the pixel. Then (2026-11-22): Black Label 5,099/5,100; DoDonPachi II equal to 762,
  where the core's simulated IGS027A fails the game's start-up check (open). The busiest stretches (docs/SLOWDOWN.md,
  "Where they slow down"): Ketsui, Espgaluda and Black Label drop the same frames as the core; Dai-Ou-Jou's stage 5
  parts by one frame at 7,359 and another at 9,140: the core's ROM-cache waits (an FPGA's, not a board's) put its
  work just past a blank the port's makes. Next: pictures and sound. (DoDonPachi II: no longer tested, the owner's call.)

- Real boards on video (docs/BOARD-VIDEO.md, tools/boardvideo): four attract videos (Mushihimesama, DoDonPachi,
  Dangun Feveron, Dai-Ou-Jou) lined up against the port frame by frame. Where the port does not slow down, the board
  does not either (within 2 frames a stretch); the demos barely slow down, so heavy slowdown still needs the 60 fps
  gameplay captures. The Mushihimesama board runs another revision (a title of 254 frames, not 1,376).

## Steps, per board (68000 first, then PGM, then CV1000 against MAME)

1. **Build the reference** and run a game's attract mode to a picture. Verilator builds and long simulations are
   CPU-heavy: run them on a spare machine if there is one, never
   more than one simulation per core.
2. **Same inputs, RAM compared at every vertical blank.** The port: the headless programs (port/headless68k.cpp,
   port/headless_pgm.cpp, port/headless.cpp) have --dump-ram, --hash-log, --press/--system scripts. Add a per-frame
   RAM log in the same format as the reference's. Runs: power-on and attract (first 3,000 frames), then a scripted
   coin, Start, the selects and about two minutes of play (each game's recipe.txt is a start). Report the first
   frame that differs and what differs; tell emulation faults (interrupt timing, the sprite chip's latch, the YMZ280B's
   interrupts that DoDonPachi's handler waits on, the PGM's level 4/6 interrupts) from timing ones.
3. **The CPU's share of a frame (slowdown).** Find what the reference takes from the 68000 (wait states on ROM, RAM,
   video and sound chip accesses; refresh; the sprite chip's DMA if any) and what the board's 68000 clock really is.
   Check it with the games' own counters (an idle loop's spins per frame: Dangun Feveron counts them at 0x1015F3 in its
   idle loop 0x0772F2..0x077302; find the like in the others), as the CPS-3 work did. Then write the rules as the port's own
   (machine/cave68k.*, machine/pgm.*), off by default, the translated code charging the same as the interpreter
   (tools/check68k.sh must still pass with the option on), and the result in docs/SLOWDOWN.md.
4. **Pictures.** The reference's frames against the port's software picture (the GPU path is already pixel-exact
   to the software one: tools/check_video.sh). On the 68000 board this settles what docs/CAVE68K.md lists as
   unchecked: the layers' offsets, flipped screens, row scroll and select, zoomed sprites, the sprite chip's time
   limit.
5. **Sound.** The reference's YMZ280B / OKI / ICS2115 output against the port's (--wav) over the same frames.
6. **Lag.** Frames from a press to the picture on the reference against tools/lag.sh's (2 on the 68000 board).

## CV1000 against MAME

Build MAME for the cv1k driver only (a long build: on the worker), with its experimental SH7709S timing on and off.
Same runs as above; compare main RAM each frame, the blitter's picture, and the frames on which the game is late
(the speed readout's count) in the busiest scenes (Mushihimesama's stage 5, Futari's last boss, Dai-Fukkatsu's
hypers) with the port's Slowdown as the board on (`slowdown`, on by default). Where MAME and the port part, read which
is right from MAME's source and buffi's notes; neither is a board.

## Deliverables

- docs/ACCURACY.md: what was compared with what, the frames equal, what was found and fixed, what is still open,
  per board and game (as cps2recomp's ARCADE_ACCURACY.md).
- The timing option for the 68000 board and the PGM, checked as above, in docs/SLOWDOWN.md.
- Fixes on their own branches with PRs (the owner merges); the installed app rebuilt after each (only the games
  that changed).

## Rules that apply throughout

At most two game windows (GPU) at once; heavy jobs to a spare machine; every feature off by
default and, with them all off, a game exactly as before (tools/check68k.sh, tools/check.sh, check_video, check_tools);
no added input lag.

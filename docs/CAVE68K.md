# Cave's 68000 boards

The boards Cave's games ran on before the CV1000 (1994 to 2001): a 68000 at 16 MHz with 64 KB of work RAM, a sprite
chip (013) that draws up to 1,024 sprites a frame into a buffer of its own, up to four tile layer chips (038), and
for sound either a YMZ280B alone (DoDonPachi, ESP Ra.De., Dangun Feveron, Guwange, Uo Poko) or a Z80 with FM and
OKI chips (the older ones: Mazinger Z, Air Gallet) or OKI chips alone (DonPachi). Every game's board has the same
parts at its own addresses.

The toolkit's second hardware. A game says which it is on in its game.cfg (`hardware = cave68k`, and `board =` for
its memory map); the build then makes its programs for that board (`Board` in machine/board.h), and the game
program's one loop (port/main.cpp) gets what is the board's own from port/hw_cave68k.h.

## What is where

| Piece | File |
|---|---|
| The 68000: interpreter, and the run loop around translated code | machine/m68k.*, runtime/recomp68k.cpp, runtime/native68k.h |
| The translator (the program's code as C++ routines) | tools/m68k/recomp.py, m68k.py, guess.py; tools/translate68k.sh |
| The board: memory map by `board =`, interrupts, the frame | machine/cave68k.* |
| Sprites and tile layers, priorities, the picture (the reference) | machine/cave_video.* |
| The picture drawn by the GPU, checked against it | port/gpu68k.*, port/shaders/*68k.* (docs/NATIVE.md) |
| YMZ280B, 93C46 | machine/ymz280b.*, machine/eeprom93c46.* |
| DonPachi's two OKI MSM6295s and their NMK112 pager | machine/okim6295.* |
| The Z80 sound side (Mazinger Z, Air Gallet): the Z80, its latches, the FM chip (ymfm's YM2203 and YM2151), the OKIs | machine/cave_z80.*, machine/z80.*, third_party/ymfm* |
| The set (program halves, sprites, layers, samples) | runtime/romset.cpp (the Cave68k LoadSet) |
| No window: runs, screenshots, hashes, entries | port/headless68k.cpp |
| The training tools' memory (a moment is the whole board, kept by changed pages) | runtime/moments.h |

All of it is from cps2recomp (the 68000 side) or written here with MAME's cave.cpp, cave_v.cpp, sprite013.cpp,
tmap038.cpp and ymz280b.cpp as the reference for the hardware's facts (THIRD_PARTY_NOTICES.md). The FM chips are
ymfm's (Aaron Giles; BSD-3-Clause), as published.

## The games

| Game | Set | Board (`board =`) | What differs from DoDonPachi's |
|---|---|---|---|
| DonPachi (1995) | donpachij (Japan, Ver. 1.01); donpachi (US, Ver. 1.12) | donpachi | the parts at other addresses; the palette at A08000 (4 KB); layer 2 only 8 x 8 tiles of 4 bits; two OKI chips (music and effects) and an NMK112 for sound; the program one file of words |
| DoDonPachi (1997) | ddonpach (World) | ddonpach | |
| Dangun Feveron (1998) | dfeveron (Japan) | dfeveron | two layers; the palette at 708000 (4 KB) and 64 KB more RAM at 710000; zoomed sprites, their pixels in byte order with the left one in the low half |
| ESP Ra.De. (1998) | esprade (World) | esprade | zoomed sprites of 8 bits a pixel; layers of 8 bits a pixel; the interrupt 2 ms before the blank |
| Guwange (1999) | guwange (Japan) | guwange | as ESP Ra.De., with RAM at 200000, the sprite chip's registers at 300000, the sound at 800000, both players in one input word at D00010, the coins and the EEPROM at D00012 (written at D00011) |
| Mazinger Z (1994) | mazinger (World) | mazinger | a Z80 for sound (4 MHz, a YM2203, one OKI); the screen 384 across; two layers of 8 x 8 tiles only, one of 4 bits (its colours the sprites' own) and one of 6; the palette at C08000; zoomed sprites, their ROM read through scrambled address lines, the list shown the one chosen a frame before; a second interrupt 2 ms after the first and the picture 2.1 ms after it; a second ROM of data at D00000; a watchdog the game counts on to start |
| Air Gallet (1996) | agallet (Europe) | agallet | Sailor Moon's board: a Z80 for sound (8 MHz, a YM2151, two OKIs); three layers at 800000, 880000 and 900000, the third of 6 bits a pixel from two ROMs; the palette at 408000 with RAM under it; zoomed sprites; the screen shows the chips' picture from its second pixel; the 68000 reset again 1 ms after power on; its memory check tests a word past the end of each memory |

How the set's files make the board's memories is said in each game's game.cfg (`sprites_bits`, `sprites_order`,
`sprites_format`, `sprites_zoom`, the layers' bits and tiles, where their colours sit in the palette, the pen of the
background), as MAME's descriptions of the same sets have it.

## The frame

57.44 frames a second (272 lines at 15,625 Hz): 278,528 CPU cycles, of which the blank is the 32 lines not shown
(the picture is 240 high). The rate is a board's: nullobject probed a Dangun Feveron board at 15.625 kHz and 57.44 Hz
for the Cave MiSTer core (Arcade-Cave_MiSTer commit ddac25d, 2021: "These values were probed from a Dangun Feveron
PCB"), and 272 lines is what those give. MAME runs these boards at 271.5 lines (57.55 Hz), with a note that the
timings are not measured; the port did too until 2026-11. The other boards are taken to share it: the same chips
and the same 28 MHz video clock (448 pixels a line at 7 MHz). Not measured on them: DoDonPachi's or the other games'
boards themselves, and Mazinger Z's 384-wide screen, whose line may be longer. `Cave68k::RunFrame` runs from one
vertical blank's interrupt to the next:

1. The interrupt (level 1) is raised; the sprite chip draws the list on show into its buffer.
2. The CPU runs on, and the picture is made halfway down the lines shown (the 32 lines of the blank, then 120
   of the picture's: 155,648 cycles after the interrupt): the layers as they are then, over and under the sprite
   buffer by priority. (The layer chips and the palette make each line as it is shown, from the blank's end to the
   frame's end; one moment for the whole picture is the nearest a picture made at once comes: [below](#pictures-against-the-core).)
3. The rest of the frame runs in sixteen steps; at each the YMZ280B is brought up to the time, and a voice that
   ended raises the interrupt again. (The chip is run to the CPU's time when written, not when read, so what the
   game reads is the same however its code is run.)

DoDonPachi's interrupt handler tells the blank's interrupt from the sound chip's by a bit that is low for the 32
lines of the blank (2.05 ms) (bit 2 of the word at 800000), not by the cause bits: without it the sound chip's interrupt is never
answered and the game stops at the first sound that ends.

## The Z80 boards' sound side

Mazinger Z and Air Gallet have no sound chip the 68000 can reach: it writes a 16-bit command into a latch among the
sprite chip's registers (at 0x6E), which raises the Z80's NMI, and reads the Z80's answers back at the same address
(Air Gallet also whether one waits, at 0x6C). The answers are kept in the order written, 32 of them, as MAME keeps
them. `CaveZ80` (machine/cave_z80.*) is that side: the Z80 with 16 KB of its ROM always there and a 16 KB bank of the
rest, the FM chip (ymfm's) whose two timers are the Z80's interrupt, the OKIs with their ROMs paged in 128 KB halves,
all timed in the Z80's cycles, and mixed to one channel at 44,100 a second in the shares MAME's driver gives them.

What the 68000 reads from it must be the same however the 68000's code is run, so this side is brought up to the
68000's time only at fixed steps of the frame (64 of them on these boards: a quarter of a millisecond) and when the
68000 writes a command, never when it reads. When a command is written the Z80's side is run 50 microseconds on at
once (as long as MAME holds the 68000 there for the answer): the games read the answer straight after. Run-ahead,
rewind and rollback keep and put back all of it (the Z80, the FM chip's whole state, the OKIs, the latches, the bank).
While run-ahead's frames run, everything but the FM chip's samples still runs: the 68000 reads the answers.

The frame on these boards is the same 278,528 cycles; between its 64 steps come Mazinger Z's second interrupt (as
the blank ends, 32 lines after the first: the game's handler takes it for its scroll) and the picture (2.1 ms after the
first interrupt on Mazinger Z's board, 0.1 ms on Air Gallet's).

Mazinger Z's program marks its RAM at power on and stops, waiting for the watchdog (the sprite chip's register 0x68,
3 seconds: MAME's guess) to reset the board; it then finds the mark and starts. Air Gallet's board resets the 68000 a
second time 1 ms after power on (MAME's `sailormn_startup`).

## Translated code and its checks

`tools/translate68k.sh ROMS RUNS` finds the code (everything fixed branches reach from the vectors, guesses from the
look of the program, and the places made-up play reaches by computed jumps, kept in work/<game>/entries.txt) and
writes it as C++. `tools/check68k.sh ROMS` runs the checks: a run on the translated code that notes any place it
came out at where no block starts (those are added and the code translated again), then every translated block
against the interpreter (registers, flags, cycles, every write; a build with -DCHECK68K=ON), then that a translated
run, a second one and an interpreter run stopping where blocks do give the same state every frame.

DoDonPachi: 78,210 instructions in 1,416 routines; none left to the interpreter in play; equal over 7,000 frames
(41.5 million blocks).

The others (2026-11-11), each equal over 7,000 frames of the check's play, and drawn by the GPU with no pixel
different from the software model's over the same: DonPachi 42,564 instructions in 1,157 routines (its Japanese
set, added 2026-11-23: 41,429 in 1,141, equal over 7,000 frames and its picture too), Dangun Feveron
86,081 in 1,729, ESP Ra.De. 72,293 in 1,419, Guwange 102,858 in 2,098.

Mazinger Z and Air Gallet (2026-11-15), each equal over 7,000 frames of its recipe's play and drawn by the GPU with no
pixel different from the software model's over 7,000 frames: Mazinger Z 47,076 instructions in 1,309 routines (45.8
million blocks checked; the interpreter runs 246,100 instructions of its start-up and none after), Air Gallet 43,569
in 1,202 (60.9 million blocks, none interpreted; the count is after its training hooks and after every stage was
played through out of harm, 45,000 frames each, until none of them left anything to the interpreter). Both games write the EEPROM's lines and read its data line (which is
among the players' controls, at another address) in one block: the board tells the checker the two are one chip
(`SameChip`), and such blocks are counted with those not comparable (7,150 and 2,098).

Two things the check needed for these games. The sound chip can raise its interrupt in the middle of a run (a write
to it); the translated code then stops at its next block (`Cave68k::UpdateIrq` brings `budget` down), where the
interpreter looks for the interrupt too. And a block that writes to a chip and reads it back (DonPachi stopping an OKI
voice and waiting for it to stop; Guwange writing the sprite chip's registers and reading what interrupts are waiting
at the same addresses) cannot be compared on the side, where its writes are only noted: such blocks are run but
counted apart ("not comparable": 265 in DonPachi's run, 6,427 in Guwange's), the whole runs being compared anyway.

### Long bot runs, and three leads from DoDonPachi Recomp (2026-11-23)

[DoDonPachi Recomp](https://github.com/CapsLock4284/DoDonPachi-Recomp) (a decompilation of DoDonPachi into C, checked
against MAME's 68000) reported three things worth checking here, and they were checked against our interpreter and
against fx68k; thanks to that project for the leads.

- **An interrupt over the stack's scratch bytes.** DoDonPachi's number-to-digits routine (057A6E, and its twin
  001F98) builds its digits below the stack pointer (`movea.l a7,a0`, three `move.b` to `-(a0)`, `clr.b -(a0)`)
  and reads them back as a long (`move.l (a0),d2` at 057A8C). A sound interrupt in between pushes its PC and status
  register over them, as on the board; their C kept the bytes in variables and parted from MAME at a game frame
  47,518. Here translated code writes the bytes to the board's RAM as the 68000 does, and an interrupt whose time is
  known is taken at its exact instruction, so the glitch comes out the same. It is rare (none of the 10,945
  interrupts of the 10,400-frame comparison with the core fell in it; the routine runs a few times a minute), so it
  was forced: `--extra-irq FRAME:CYCLE` on the headless runner raises the line once more (measurement only), swept
  across the routine at frame 3,525. Read back: 00000086, 00007A88, 00057A8A or 00057A8C by where it lands (the
  pushed PC's bytes), translated, interpreted and blocks alike over 7,000 frames, and the block checker finds
  nothing. On fx68k alone (tools/fx68kbench) the same instructions with the line raised at each of 81 cycles: the
  same instruction taken after, the same bytes pushed and read back, the same time, at every one.
  What it did find: the blocks run (`--cpu blocks`, the checks' reference) and the block checker went on
  interpreting past an RTE that returned inside a block, where translated code leaves its block and takes an
  interrupt the restored mask now lets in; they took it later than translated code and the interpreter. Fixed
  (`M68K::Returned`); translated code's hashes are unchanged.
- **MOVEM with A7.** They found their MOVEMs with A7 8 cycles short. Ours equal fx68k's for every form:
  tools/fx68kbench runs 310 MOVEM forms on fx68k alone and on the interpreter (A7 as the base in (A7), -(A7),
  (A7)+, d(A7), d(A7,Dn); A7 in the list alone, with others, all 16; words and longs; the same on A0, absolute and
  PC-relative): the same time for all 310, and the same registers and memory after (A7 stored as it was before the
  instruction, and loaded as the incremented address). The translator uses the same tables. The fx68k bench of
  2026-11-20 (every form in the twelve games' code) was lost with /tmp at a reboot; tools/fx68kbench keeps this one.
- **Long play.** A bot (`--auto SEED`: the stick held a while each way, the shot mostly held, a bomb now and then, a
  coin and Start every 240 frames) played each Cave 68000 game and each PGM game but DoDonPachi II three times,
  100,000 frames each (about 29 minutes of play): seed 11 as it comes, seed 12 with the ship kept out of harm from
  frame 4,000 (its safe-frames byte held at 0x60), seed 13 the same with the loop word set to the second loop where
  it is known (DoDonPachi, DonPachi, Dangun Feveron, Mazinger Z). Translated code against the interpreter, the
  frame's state hash (RAM, registers, video memory, EEPROM) every frame; the PGM games with "Slowdown as the board"
  on (`--timing`); then `tools/check68k.sh` on each (the PGM games with `--timing` too). The furthest each got
  (loop/stage, from RAM dumps every 10,000 frames):

  | game | seed 11 | seed 12 | seed 13 |
  |---|---|---|---|
  | DoDonPachi | 1/1 | 1/6 | 2/6 |
  | DonPachi | 2/1 | 1/5 | 2/5 |
  | Dangun Feveron | 1/5 | 1/5 | 2/5 |
  | ESP Ra.De. | 1/6 | 1/5 | 1/6 |
  | Guwange | 1/2 | 1/7 | 1/7 |
  | Mazinger Z | 1/8 | 1/7 | 2/7 |
  | Air Gallet | 1/2 | 1/6 | 1/6 |
  | Ketsui | 1/4 | 1/5 | 1/5 |
  | Espgaluda | 1/1 | 1/5 | 1/6 |
  | Dai-Ou-Jou | 1/1 | 1/5 | 1/5 |
  | Dai-Ou-Jou Black Label | 1/1 | 1/5 | 1/5 |

  The PGM games' second loops were not reached (their loop words are not known yet); which ship each seed chose was
  not looked at. Code not yet translated runs in the interpreter in both runs, so it is compared with itself: a
  short run of seeds 11 and 13 found the computed jumps' places first and the code was translated again with them,
  but DoDonPachi kept its older list, and its seed-13 run (the second loop) had 71 million instructions interpreted
  (the others 15 to 48 million on the Cave boards, 400 to 460 million on the PGM, where the last 8,192 cycles before
  each interrupt are interpreted too). With the fixes below, all 33 runs are the same translated and interpreted at every frame, and
  check68k passes for all eleven.

  Four differences were found on the way, and fixed. Three were translated code running past a time the board has
  an event at, to the end of its block, where the interpreter stops on the instruction (translated code's hashes
  move in those frames only, to the interpreter's; the interpreter's do not move):

  - Dangun Feveron, seed 12, frame 19,479 (147 frames up to 63,526, never parting for good): a frame is run in 16
    steps, and translated code run to a step's end goes on to its next block. In frame 19,478 it ran from 34,816 to
    34,994, past a YMZ280B interrupt at 34,882 that is only looked for in the next step, took it 100 cycles late,
    and the handler's pushes landed on other stack bytes. A step now ends exactly (its last 2,048 cycles an
    instruction at a time) when a voice ends in the step after; so does the run to the picture's time, on the way to
    an interrupt.
  - Air Gallet, every run, frames 1 to 389: the board resets the 68000 a second time 1 ms after power on, and
    translated code ran past the reset, writing more before it. That run now stops at the instruction the reset
    comes in.
  - Espgaluda, seed 11, frames 1-3 and 70,608-70,633: an exact run takes its last 2,048 cycles an instruction at a
    time, translated code stopping at the first block that starts before that; with the board's waits a block can
    take longer (the PGM's video memory holds an access for up to 368 cycles at a line's start), and the block
    149D88-149DE4 ran on past a level-4 interrupt's time. The PGM's exact runs now take their last 8,192 cycles an
    instruction at a time (no speed lost that could be seen). In the boot, with the frame's interrupt off, the
    frame's end was not run exactly; it now always is, and a PGM run also ends exactly when an interrupt or the
    sprite DMA comes within 8,192 cycles after it.

  The fourth was the interpreter's. Ketsui, seed 11, frame 18,457 (489 frames up to 70,007, never parting for good):
  `move d1,sr` (0A738C) lowers the mask just after the level-4 line has risen. The interpreter took the interrupt an
  instruction later than translated code: it had a MOVE to SR or CCR from a register or memory look at the lines 12
  cycles before its end (its internal cycles counted after the look), where fx68k alone looks 4 before, as for the
  immediate forms (tools/fx68kbench mask.py: the line raised at each cycle around MOVE to SR, to CCR and from SR,
  register and memory forms, with the mask 7 and 0 before; 976 cases, each MOVE to SR or CCR an instruction late in
  an 8-cycle window before, all the same now). Hashes move, interpreted and translated alike, where a line rises in
  that window: Ketsui's bot run from frame 1,252. DoDonPachi's 10,400-frame play is unchanged, with the shipped
  timing and with the core's (so still equal to the core at every frame). The PGM games against their core's logs
  again (docs/SLOWDOWN.md), with it: every count as before, so those numbers stand (play: Ketsui 3,396 of 3,400,
  Espgaluda 3,396 of 3,400, Dai-Ou-Jou 2,697 of 2,700, Black Label 5,099 of 5,100; busy stretches: Ketsui 4,047 of
  4,050, Espgaluda 4,228 of 4,250, Black Label 3,541 of 3,550, Dai-Ou-Jou 7,571 of 8,250 with the same slips at
  7,359 and 9,140).

## Pictures against the core

The Cave MiSTer core's pictures (the harness's `--shot`: what the core shows, line by line) against the port's raw
pictures (`--raw-shots`), with the work RAM equal at every frame in each run (2026-11-23).

**The picture's moment.** On the board, and on the core, the layer chips and the palette make each line as it is
shown: the 240 lines from the blank's end (32 lines after the interrupt) to the frame's end. The sprites are drawn
into the sprite chip's own buffer and shown whole. The port makes the whole picture at one moment. It used to make it
90 µs to 2 ms after the interrupt (MAME's times), which is before any line is shown. So a change a game makes after
its interrupt's work showed a frame late:

- Guwange (coin at 100): 8 of 110 pictures, frames 1,160 to 1,240, were the core's a frame later. The title's logo is
  changed 33,000 to 60,000 cycles into the frame; the first line is shown at 32,768.
- Dangun Feveron (coin at 2,500): 35 of 70 differed. The stars are moved during the lines shown.

The port now makes the picture halfway down the lines shown (the 120th: 155,648 cycles after the interrupt) on the
five boards the core runs. The sprites are still taken at the interrupt, as before. Mazinger Z and Air Gallet keep
MAME's moments: the core does not run them. No state hash changes: only what is drawn moves.

| Run | Pictures equal, before | Now |
|---|---|---|
| ESP Ra.De., coin at 300, frames 600 to 2,390, every tenth | 180 of 180 | 180 of 180 |
| Guwange, coin at 100, frames 600 to 1,690, every tenth | 102 of 110 | 110 of 110 |
| Guwange in play (play2), frames 3,475 to 3,515 | 28 of 41 | 41 of 41 |
| Guwange in play (play2), frames 3,215 to 3,255 | 2 of 41 | 9 of 41 |
| Dangun Feveron, coin at 2,500, frames 1,000 to 2,380, every twentieth | 35 of 70 | 68 of 70 |

Left as they are:

- **Feveron's two frames** (1,080 and 1,500: 500 and 252 pixels). The game writes during the lines shown, so the core's
  top lines show the old state and the lower ones the new. That is a tear on the board too. Only drawing line by line
  would match it.
- **Guwange's 32 frames at 3,215 to 3,255.** One to four whole rows of a checkerboard (fake-translucent) effect
  differ: the core draws them and the port does not. It is not the picture's moment and not the sprite chip's time
  limit (taking the limit away changes nothing). Not looked into further: a pro would not notice it.

## Not done yet

- **Against the Cave MiSTer core** (docs/SLOWDOWN.md): DoDonPachi, ESP Ra.De., Guwange and Dangun Feveron equal at
  every frame run, with the same dropped frames. DonPachi parts at 2,507, by its OKI chips' status bit (the random
  numbers only). Mazinger Z and Air Gallet are not in the core. Otherwise no reference emulator has been run beside it: the picture has been looked at
  (boot, title, demonstration, ship select, the first stage) and looks right, the sound has not been listened to,
  and the layer offsets, flipped screens and line scroll are as MAME's sources describe them, unchecked. Zoomed
  sprites are placed as the core's sprite chip places them (SpriteBlitter.sv): the position keeps its 8 bits of
  fraction, source pixel n lands at the position plus n times the zoom (flipped: plus the width less one pixel less n
  times the zoom), on the screen pixel that falls in. MAME's cave.cpp drops the fraction first and moves flipped and
  one-pixel sprites half a pixel, often a pixel off. The chip writes a sprite's source in its own order, row by row
  from the first, whether flipped or not, so where a zoom below 0x100 puts two source pixels on one screen pixel the
  later source pixel stays (the port had kept the later drawn, which for a flipped sprite is the earlier).
- **Lag.** Measured (tools/lag.sh: the board 2 frames, run-ahead 2 takes it to 0, on the GPU path too) but not yet in
  the gate for this board.
- **Training** past the five features and the hit boxes (docs/TRAINING.md): the scoring readouts (rank), modes; and
  the second loop as a start.
- **Training in the four added 2026-11-11** (DonPachi, Dangun Feveron, ESP Ra.De., Guwange): only the tools every
  game has (practice point, replay, rewind, fast forward, input display); their facts (stage, ships, bombs, hit
  boxes) are still to be found.
- **Mazinger Z and Air Gallet** (added 2026-11-15): compared with nothing but themselves either. Their sound has not
  been listened to (it is made, music and effects, at the levels MAME's driver mixes them: Air Gallet's two OKIs
  together can reach the top of the range for a few samples, as they would there). The YM2203's three square-wave
  voices are ymfm's own model of them, not MAME's AY-3-8910. The watchdog's 3 seconds and the OKIs' clocks are MAME's
  guesses. Flipped screens and Air Gallet's layer 2 tile bank (its ROM has only the one bank) are as MAME describes
  them, unchecked. Training: both have the five features and the hit boxes (docs/TRAINING.md,
  games/mazinger/PROGRESS.md).
- **Other companies' games on the board** not here yet: Sailor Moon (Air Gallet's board, with a scrambled sprite ROM
  and more layer 2 tiles), Hotdog Storm, Metamoqester, Power Instinct 2 and its sequel (a Z80 with FM chips too, each
  with its own ports).
- **The board after this one**: the PGM-based boards of 2002 to 2004 (DoDonPachi Dai-Ou-Jou, Ketsui, Espgaluda: a
  68000 again, with an ARM7 protection chip) are a third hardware.

## Adding a game of this hardware

1. Its set in the ROM folder as `<set>.zip`; `games/<id>/game.cfg` with `hardware = cave68k`, `board = <name>` and
   the files by checksum (games/ddonpach/game.cfg is the model).
2. If its board has its parts elsewhere: a line in `kMaps` (machine/cave68k.cpp) from MAME's map for the game, and
   whatever its interrupt handler reads that DoDonPachi's does not.
3. `GAME=<id> tools/translate68k.sh rom 3`, then `GAME=<id> tools/check68k.sh rom` with a recipe that reaches play
   (`games/<id>/recipe.txt`).
4. The credits counter (three coins, the bytes that went 0, 1, 2, 3) for free play: games/ddonpach/src.
5. A board with a Z80 for sound: its `Kind` in machine/cave_z80.* (the ports, the clocks, the mix), `z80 =` in
   game.cfg, and `GAME=<id> tools/translatez80.sh rom`, then `GAME=<id> tools/checkz80.sh rom`.

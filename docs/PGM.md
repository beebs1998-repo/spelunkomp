# IGS's PGM board

The board Cave's games of 2002 and 2003 ran on: **DoDonPachi Dai-Ou-Jou** (`ddpdoj`) and its **Black Label**
(`ddpdojblk`), **Ketsui** (`ket`) and **Espgaluda** (`espgal`); and before them the board's own maker's
**DoDonPachi II: Bee Storm** (`ddp2`, 2001), which IGS made under licence from Cave. IGS's PolyGame Master with a
cartridge:

- a 68000 at 20 MHz, 128 KB of work RAM kept by a battery (the settings and scores are in it);
- the IGS023 video chip: a background of 32 x 32 tiles (5 bits a pixel) with a scroll for every line, a text layer of
  8 x 8 tiles over everything, and up to 256 sprites a frame, in front of or behind the background, zoomed by tables;
- a Z80 at 8.4664 MHz (a quarter of the board's 33.8656 MHz sound clock) running the sound driver the 68000 hands it, with an ICS2115 (32 voices of samples from ROM);
- the IGS027A on the cartridge: an ARM7 with a ROM of its own. On Cave's cartridges it deciphers the program as the
  68000 reads it and answers the game's questions; on DoDonPachi II's it runs a second program from a ROM of the
  cartridge's and does part of the game.

The toolkit's third hardware: `hardware = pgm` in game.cfg. The build then defines `SPELUNK_CAVE68K` (everything of
the 68000's: the translator, its checks, the hooks and training of runtime/shmup.h) and `SPELUNK_PGM` (machine/board.h
picks `Pgm`; port/main.cpp gets port/hw_pgm.h; the headless runner is port/headless_pgm.cpp).

## What is where

| Piece | File |
|---|---|
| The board: memory map, the frame, interrupts, sound latches, the IGS027A, the V3021 clock | machine/pgm.* |
| The ARM7 (DoDonPachi II's IGS027A is run) and its self-test | machine/arm7.*, tests/arm7_test.cpp |
| The program's decipher (by game: `cipher = ddp3`, `ket`, `espgal`, its table found at load and checked by `cipher_sha1`; `arm_cipher = ddp2`) | machine/pgm_crypt.* |
| The video chip (the software reference) | machine/igs023.* |
| The picture drawn by the GPU, checked against it | port/gpu_pgm.*, port/shaders/*_pgm.* |
| The synthesizer; the Z80 | machine/ics2115.*, machine/z80.* |
| The set: regions made of files at offsets (`name:crc@offset`) as MAME lays them out | runtime/romset_pgm.cpp |

All of it written with MAME's sources as the reference for the hardware's facts (THIRD_PARTY_NOTICES.md).

## The IGS027A in Cave's games

Its own ROM has never been read for any of Cave's three games (MAME: NO_DUMP). What it does for them is small and is
done here as MAME does it: the program comes deciphered (once, when the set is loaded: below), and at 500000
(Dai-Ou-Jou) or 400000 (the others) it answers commands behind a key that changes with each: 32 slots of 24 bits set,
added and read back. MAME runs all three games through this and they play; nothing here has been compared with a real
board.

### The program's cipher, and its table worked out at load

Each 16-bit word of the program is enciphered by its place (its index in words from where the game begins: 100000 for
the two Dai-Ou-Jou sets, 0 for Ketsui and Espgaluda). Bits of the low byte flip where the index's bits match a pattern,
eight rules for each game (machine/pgm_crypt.cpp: `ddp3` for both Dai-Ou-Jou sets, `ket`, `espgal`), and the high byte
is exclusive-ored with a game's table of 256 bytes, the byte for the index's low byte. No table is in this project. It
is worked out from the player's own program ROM as the set is loaded:

- The rules touch only the low byte and the table only the high, so a run of 256 words, from an index whose low byte is
  0, whose plain high bytes are all one value F holds the table (each byte exclusive-ored with F) in its high bytes.
  Programs have many: stretches of zeros, of small numbers and of ASCII (F = 0), and of all ones (F = FF).
- The runs whose high bytes are one value throughout are left out: the cartridge left them plain (its blank fill),
  and they say nothing. Of the others, the most repeated is the first guess; then every run that differs from the
  guess (or from its complement) in at most 32 of its 256 bytes votes, and each byte of the table is the value most
  of them have there.
- The table is then checked: its SHA-1 must be game.cfg's `cipher_sha1` (40 hex digits, from which nothing can be
  worked back), or the set does not load and says why (`is the program ROM the right one and whole?`). A table whose
  check fails is tried once as its complement, for a program with more runs of ones than of zeros.

How clearly the table comes out (2026-11-19; `SPELUNK_KEY_LOG=1` prints it at load):

| Set | Runs not all one value | First guess seen | Runs that voted | Narrowest byte's lead |
|---|---|---|---|---|
| ddpdoj | 1,594 | 22 | 33 | 25 votes |
| ddpdojblk | 3,657 | 497 | 518 | 505 |
| ket | 3,653 | 29 | 63 | 43 |
| espgal | 3,622 | 51 | 73 | 61 |

The most repeated run alone is already the table in all four, but in Espgaluda only by 51 to 46 (the next is the same
table where a few bytes of the plain text are not 0); the vote of every run near it is what makes the lead wide. Every
byte of the program as the CPU sees it is the same as with the table written in (checked against the code before, all
four sets, and DoDonPachi II's ARM ROM for its fixed rules), and so are 6,000 frames of each recipe's per-frame hashes.
Working the table out adds about 1 ms to loading a set (the decipher takes 3.5 to 3.8 ms in all, on the Mac mini M4,
against 2.5 to 2.8 ms with the table written in).

## DoDonPachi II's IGS027A: an ARM that is run

IGS's own DoDonPachi II: Bee Storm (`ddp2`, 2001, under licence from Cave) has the IGS027A of another kind (MAME's
"type 2", also Knights of Valour 2's and Martial Masters'). Its ROM has been read (16 KB, one for each region: the
region is a byte of it), the cartridge has a second ROM of program for it (128 KB, enciphered), and it does much of
the game: the 68000 gives it five commands a frame in play and it keeps tables of the game's objects in memory the
two share. The 68000's program is not enciphered at all. So the ARM7 is run here (machine/arm7.*), from the two
ROMs of the player's set, and nothing is imitated.

### How others do it (looked up 2026-11-15, before starting)

- **MAME** (`src/mame/igs/pgmprot_igs027a_type2.cpp`, `pgmcrypt.cpp`; BSD-3-Clause) runs an ARM7 at 20 MHz on the
  two ROMs. The ARM's map: its ROM at 0, the cartridge's at 08000000, RAM at 10000000 (1 KB) and 18000000 (64 KB),
  the latches at 38000000, the shared RAM at 48000000, and at 50000000 a table of 256 bytes the chip's ROM writes as
  it starts: every word of the cartridge's ROM is read exclusive-ored with the table's byte for it (in its two odd
  bytes), on top of fixed rules by address MAME applies when the set is loaded (`pgm_ddp2_decrypt`). The 68000 has
  the shared RAM at D00000 (64 KB) and the latches at D10000. A write to the 68000's latch raises the ARM's fast
  interrupt (FIQ), which drops when the ARM reads the latch ("guess", MAME says of both). The set has the PGM's
  ordinary BIOS at 0 and the game at 100000, no battery RAM of its own (the board starts with it empty) and a ROM
  for each region; MAME patches nothing for this game (it has a region override only for the games whose other
  regions' chips are not read). It has two speed hacks, neither needed for correctness: the ARM's waiting loop
  (08010998) and the 68000's (149DCE) are skipped until an interrupt. How finely MAME runs the two side by side is
  its scheduler's default; nothing in the driver asks for more.
- **Arcade-IGSPGM_MiSTer** (wickerwaka and contributors; now at github.com/MiSTer-devel/Arcade-IGSPGM_MiSTer,
  GPL-2.0, the IGS027A's file GPL-3.0-or-later) lists Bee Storm as supported and runs a real ARM too: Game Bub's
  ARM7TDMI core (Eli Lipsitz) on the two dumped ROMs, loaded by the game's MRA. `rtl/PGM.sv` gives the ARM its own
  clock enable, 20 MHz for this game (22 MHz for Martial Masters and Dragon World 2001), the same rate as the
  68000's; `rtl/igs027a.sv` has the same map as MAME's, the fast interrupt set by any 68000 write to D10000 and
  cleared by the ARM's read of 38000000, the ARM's other interrupt tied off, and the table at 50000000 applied at
  every read of the cartridge's ROM (the fixed rules at load: `rtl/exrom_decrypt.sv`). Where both want the shared
  RAM the 68000 goes first (`rtl/share_cache.sv`). The region is the chip's ROM's own, one MRA a region.
- **The other PGM core for MiSTer** (eziochiu's; only binaries are published: github.com/terminator2k2/PGM-MiSTer
  and mirrors) lists Bee Storm too and loads the same two ROMs, so it runs the ARM as well; its README says the
  IGS027A kinds were written afresh in VHDL. With no source, nothing can be said of its timing. No PGM core was
  found for the Analogue Pocket, nor one in jotego's jtcores.
- Nobody imitates this chip, then, and nobody has published a measurement of the 68000 and the ARM against each
  other on a board. (kuzearcade/ddp2bl, which replaces the chip by 68000 code, measured the ARM's answers in MAME:
  some 160 to 190 of the 68000's cycles for the short commands. Here the shortest, the attract show's, takes 171.)

No code of either core is here: the facts above, and MAME's map.

### The ARM7 (machine/arm7.*)

An interpreter of the ARM7TDMI: both instruction sets (the chip's ROM is ARM code, the cartridge's Thumb), the modes
with their own registers, the two interrupt lines, and time counted as the processor's data sheet has it (a cycle
for each sequential, non-sequential and internal cycle; no wait states are known for the chip's memories, so none
are added). `tests/arm7_test.cpp` runs 43 short programs of the kinds of instruction the code uses against what
ARM's description says they give (registers, flags, for some the cycles), the fast interrupt, and the count below.

Most of the ARM's time is a loop waiting for the 68000's command. `Arm7::Run` sees any such loop for itself, by
what it is and not by where: a branch backwards that comes where the last one came, with every register and the
status the same, nothing written and nothing read that anything but the ARM can change. The rounds that fit before
the time it is run to are then counted, not run, and the state at the end is exactly what running them gives
(`--arm-plain` runs them all: the whole board's hash is the same after every frame). 97.8% of the ARM's cycles in
the checks' play are counted this way.

### The ARM beside the 68000

The rule everything rests on: what the 68000 reads must be the same however its code is run (interpreted,
interpreted block by block, translated, or a translated block run on the side by the check with its writes only
noted). So nothing beside the 68000 is run on because the 68000 reads something. What the game does with the ARM
(found by tracing it, `SPELUNK_TRACE_ARM=1` on the headless runner):

- A command (the routine at 145918): the 68000 puts the command's number in the shared RAM (D00010), writes a
  count into its latch, and then goes round a loop reading the ARM's latch until the count plus one is in it, up
  to 10,000 times. One command a frame in the attract show, five a frame in play.
- The ARM's fast interrupt's handler reads the latch, puts 80 in its own (busy), does the command there and then,
  and puts the count plus one in its latch as it leaves. That takes 171 cycles for the shortest and up to some
  9,500 in the checks' play. Work the command leaves for later (the handler's tasks) the ARM does after it, in its
  main loop: up to 22,700 cycles more, 7% of a frame, with the 68000 long since on its way.

So the ARM is run like this (`Pgm::RunArm`, `ArmThroughInterrupt`):

1. It is brought up to the 68000's time whenever the 68000 writes to the shared RAM or to its latch (a write's
   time is the same however the code is run), and at 64 fixed steps of the frame.
2. At a write to the 68000's latch the ARM is run on at once, ahead of the 68000, until it has taken the interrupt
   and left its handler. What it puts in its own latch meanwhile is kept with the ARM's time of putting it there,
   and the 68000's read of the latch gives what was in it at the time of the read (the start of the reading
   instruction: the same however the code is run, which the video chip's line counter relies on already; and
   nothing changes by the read). The 68000's loop therefore ends on the cycle it would with the two run cycle by
   cycle: a command costs the 68000 what the ARM's handler takes, no more and no less.
3. The work the ARM does after its handler is seen by the 68000 at the next step or the next write: up to a 64th of
   a frame late (5,280 cycles).

What could go wrong, and how it is known not to: in 2 the ARM does not see a 68000 write made while it is ahead
(there are none: the 68000 is in its loop; the runner counts them, 0 in every run so far), and in 3 the 68000 could
read from the shared RAM something the ARM's later work has not yet put there at the step's coarseness. So
`tools/check_arm.sh` runs the checks' play with the ARM brought up 16, 64 and 1,024 times a frame: the work RAM
after every frame, and the frames with a new picture, are the same in all three. The game does not depend on how
finely the two are run side by side, as long as the handler is run through at once. (Without 2, with steps alone,
each command would have cost the 68000 up to a step more than the board takes.)

Save states, run-ahead, rewind and online play's rollback carry all of it (`Pgm::Serialize`: the ARM's registers
of every mode, its two RAMs, the shared RAM, both latches and what the ARM's is still to show, the table), and
`StateHash` covers it.

Checked (2026-11-15): `tools/check_arm.sh rom` (the self-test; waits counted == run; 16, 64 and 1,024 steps a
frame the same game; no write behind the ARM); `tools/check_tools.sh rom ddp2` (the practice point, rewind and both
with run-ahead put the game back exactly, the ARM with it); `tools/check_online.sh rom ddp2` (two copies, 6,000
frames: 99 hashes alike, and again with every packet late and some lost, `FAKE=60:20:5`: 308 rollbacks, 1,249 frames
run again, 99 hashes alike; and the same with a game begun at stage 4, out of harm, bombs kept and the hit boxes
on: 309 rollbacks, 99 hashes alike). Again after the training features and the kept credit were added: all the same.

**The ARM's code is interpreted**, not translated: with the waits counted it is 3% of the frame's ARM cycles that
are run (4% in a long run of play), and the interpreter takes about 30 microseconds a frame for them in the checks'
play and about 100 in 30,000 frames of made-up play, on the Mac mini (M4) this was written on: 0.2% to 0.6% of a
frame's 16.9 ms, and 2% to 5% of a headless run going as fast as it can on translated 68000 code. A translator for
it would save next to nothing.

## The frame

264 lines of 64 microseconds (59.18 Hz; 337,920 CPU cycles). `Pgm::RunFrame` runs from one vertical blank's start
(line 224) to the next:

1. The CPU runs, with an interrupt at level 4 every 62 lines at a line's horizontal sync (a counter of its own, in no
   step with the frame) and the sound side brought up to the CPU's time 16 times a frame and whenever the 68000
   writes to it or reads it (a read at the time of its instruction's start, so it returns the same however the code
   is run).
2. At line 261's horizontal sync, three lines before the blank, the sprite DMA copies the list from the start of work
   RAM into the chip (5 words a sprite, to the first of height 0) and holds the CPU off the bus meanwhile.
3. At the blank: the sprites latched at the last blank go on show and the picture is made; the list the DMA copied is
   latched for the next frame (so sprites are a frame behind the layers, as on the board); the level 6 interrupt.

Interrupts stay up until the game clears their enable bits in the control register.

The Z80 runs a sample's time (256 of its cycles) at a go; the ICS2115 runs on its own clock ticks (four a Z80 cycle),
its timers (the driver's tempo) and its samples (33,075 a second) in their order. Its sample addresses are 24 bits:
the bank byte's high bits fall off.

## Video and the check

The sprites' pixels are packed: the B ROM has a mask bit for every pixel, the A ROM the pixels only where the mask is
clear, three to a word. The GPU finds a pixel without unpacking the sprite: a count of the clear mask bits before
every B word is made once when the ROMs go to the GPU, so the pixel of row r, column c is the A data's k-th, k one
subtraction and the clear bits of its own mask word. Zoom (whole rows and columns repeated or left out by the bits of
32-bit tables) and flips map the screen pixel to (r, c) first. The sprites are drawn as quads in the list's order into
a 16-bit buffer (the chip draws from the list's end keeping the first pixel it draws: the same pixel wins); then one
pass makes the picture: the background with its line scroll, the sprite unless it is behind the background where the
background has a pixel, the text over both.

`--check-video` runs the software model beside it and compares every frame: 0 of 7,000 frames differ in each of the five
games (boot, attract, a game begun with free play; up to 253 sprites, Espgaluda; DoDonPachi II up to 130).

## Translated code

As on Cave's 68000 board (docs/CAVE68K.md): `GAME=<id> tools/translate68k.sh rom 3`, then `GAME=<id> tools/check68k.sh
rom` with games/<id>/recipe.txt. Translated == interpreted over 7,000 frames in all four of Cave's (70 to 82 million blocks checked each), none of the
code left to the interpreter in play.

DoDonPachi II (2026-11-15): 668,427 instructions in 3,345 routines, from 1,414 places: three rounds of made-up
play, three more that begin games through free play (`tools/translate68k.sh rom 3 30000 --features free_play`: the
made-up play's Start comes too soon after its coin for this game), and then games begun at each of the seven stages
with the training features (the trip to each boss, and 45,000 frames of made-up play at each stage, twice). Translated
== interpreted over the 7,000 frames of its recipe: 47,907,630 blocks checked, 1,435 more run but not comparable
(they write the RAM shared with the ARM and read it back: on the side, where a block's writes are only noted, the
ARM has not seen them), none of the code left to the interpreter; the same with the training hooks at work (a game
begun at stage 3 and run to its boss, bombs and ships kept: 9,000 frames, 46,569,301 blocks; at stage 6 out of harm
with the board's waits on, `--timing`: 60,694,282 blocks). The last fourteen runs of 45,000 frames of made-up play,
two at each stage: 0 instructions interpreted. `tools/check_arm.sh` is this game's own check, of the ARM (above).

The Z80's sound driver is translated too, from the copy the 68000 puts in its RAM (docs/NATIVE.md): `GAME=<id>
tools/translatez80.sh rom`, then `GAME=<id> tools/checkz80.sh rom` (translated == interpreted every frame, the sound
sample for sample: all five games; DoDonPachi II's driver is 7,961 instructions, and 12% of the Z80's cycles in its
recipe's 7,000 frames are interpreted, nearly all of them the BIOS's own driver before the game's is loaded).

## The games

| Game | Credits byte | Notes |
|---|---|---|
| ddpdoj | 80394A | the PGM BIOS (logos taken out) at 0, the game at 100000 |
| ddpdojblk | 80395A | two games on the cartridge: A (Dai-Ou-Jou of 2002.04.05, at 100000) and B (Black Label, at 200000), chosen on a screen at power on (20 seconds, up/down, shot) that starts at the choice kept in the battery RAM's word 803810; the factory RAM has A, and `nvram_set = 803810:0001` makes this entry start at B |
| ket | 801160 | no BIOS: the game at 0 |
| espgal | 8011E8 | no BIOS: the game at 0 |
| ddp2 | 80EED6 | the PGM's BIOS (`pgm_p02s.u20`) at 0, the game at 100000, not enciphered; the ARM's two ROMs (`arm_internal`, `arm_external`); no battery RAM in the set: the board starts with it empty and the game fills it in; three buttons (shot, bomb, rapid shot) |

Free play gives a credit at Start (as on Cave's 68000 board). In DoDonPachi II a credit alone is not enough: its
demonstration and title go to the "push start" screen only for a coin they are told of (the coin's code puts a
message in the game's queue), and a press of Start before that screen is up, some 45 frames after the coin, is lost.
So free play there keeps a credit in by the board's own coin switch: whenever the counter is at 0 a coin is dropped
(games/ddp2/src/game.cpp), the game is at "push start" before the player is, and Start is the player's own press,
taken at once (the select screen is up 10 frames after it). The first coin goes in as the board starts, so the
title is there with its credit when the boot's unseen part ends. Two things follow, both the board's own way with a
credit in it: the attract show's demonstration and opening are not seen while free play is on, and a game over left
alone comes back to the title and, after the game's own waits (a minute or two), begins a game by itself.

DoDonPachi II's sound driver is not the first in the Z80's RAM: the BIOS puts its own there for its logo, and the game
its own some 760 frames in. `tools/translatez80.sh` notes the places the driver reaches only from game.cfg's
`z80_driver_frame` on; before that the Z80 is interpreted (the translation runs only while the RAM holds what it was
made from).

### Zoomed sprites: the game's tables, and the core's

A sprite's zoom says, for each of 32 source columns (and rows) in turn, whether it is drawn twice (grow) or left out
(shrink). The games write the patterns themselves, sixteen of them, into the IGS023's zoom RAM (B01000) at boot:
Ketsui 5555 5555, 5515 5555, 5515 5515, ... 0000 0000. The port reads them as MAME does: pattern n is
(word 2n << 16) | word 2n+1, its bit k for column k, a grow of z taking pattern 16 - z.

The PGM core for MiSTer does not read that RAM. It has 32 patterns of its own (rtl/igs023_sprite.sv, `scale_pattern`),
bit-reversed when used. They agree with the games' tables on how many columns are doubled or dropped, but not always
on which: for a shrink they are the games' tables with the two words swapped (the same where the two words are alike,
at the even patterns), and a grow is out of phase with the port's by a column. So a zoomed sprite can differ in a few
columns and rows between the two. Ketsui's hit counter ("x21", grown by 15) at frame 3,300 of the play script: the
core's picture and the port's differ in 945 pixels, all of them on its digits; every other pixel of the frame is the
same. Neither has been compared with a board. The port keeps the games' own tables, since a board that ignored what the
games write there would be the less likely one, and the question is listed for a capture of a board
(spelunkomp-data/research/PCB-EVIDENCE.md).

## Against a board: the PGM core's test ROMs

MiSTer-devel's Arcade-IGSPGM_MiSTer (wickerwaka's PGM core) carries test ROMs of its own and their results **run on a
real PGM board** (through a debug cartridge; util/vram_bench/results/hw_full.jsonl and hw_gap.jsonl,
util/sdr_stress/results/sdr_hw_postfix.jsonl), with logic-analyser captures of the board's IGS023
(util/vram_bench/results/LA_FINDINGS.md). The same ROMs were built from that repository (testroms/, `PAGE=vram_bench`
and `PAGE=sdr_stress`, m68k-elf-gcc) and run on the port, headless, with the board's waits on: the ROM takes the
BIOS's place (a game.cfg of its own), and its host talks to it as the core's simulator does, through the ring it keeps
in work RAM (`--link REQUESTS:RESPONSES`, port/headless_pgm.cpp). The tests and the windows are the ones the board was
given (run.py's matrix: 60 frames a test, the board's median of three). Nothing of the core is used: the board's own
numbers are the reference. `tools/pgm_board.py` runs it all and prints the comparison.

**sdr_stress** (68000 loops while the sprite chip draws 0 to 224 sprites: none of it touches the video memory). All
20 results within 0.005% of the board's, the same before and after the change below:

| loop | 0 sprites | 32 | 64 | 128 | 224 |
|---|---|---|---|---|---|
| registers only (chunks in 120 frames) | 238,110 / 238,118 | 237,927 / 237,938 | 237,750 / 237,757 | 237,384 / 237,395 | 236,846 / 236,853 |
| one ROM word read over and over | 170,078 / 170,083 | 169,950 / 169,955 | 169,819 / 169,825 | 169,560 / 169,567 | 169,175 / 169,180 |
| ROM reads 16 KB apart | 149,922 / 149,926 | 149,808 / 149,812 | 149,695 / 149,698 | 149,467 / 149,471 | 149,126 / 149,130 |
| a call through 32 KB of code | 618 / 618 | 617 / 617 | 617 / 617 | 616 / 616 | 614 / 614 |

(port / board.) The 68000's instruction times and the sprite DMA's hold of the bus are the board's. The port's
constant 0.003% short is the board's 68000 crystal: the core's notes measure 337,921.7 of its cycles in a frame
(it runs some 5 ppm fast against the video's), the port 337,920.

**vram_bench** (the same loops reading and writing work RAM and the video memory, a frame at a time or only between
given lines, with the control register's bit 10, the CPU first, off and on). Accesses a frame: the board's, then the
shipped model's difference from it (0.1.0-beta.1: the IGS023 fitted to the core of June 2026) and **this model's**.
The text layer's memory (fg) gives the same as the background's in every row and is left out; so are the one-line
windows (48 to 100 accesses: a chunk more or less is 20%):

| access | target | whole frame | lines 16-199 | blank | whole frame, CPU first |
|---|---|---|---|---|---|
| word_r | work RAM | 23065: +0.0%, **+0.0%** | 12997: -0.0%, **-0.0%** | 2336: +0.0%, **+0.0%** | 23065: +0.0%, **+0.0%** |
| word_r | video memory | 14506: +27.6%, **+0.0%** | 7856: +31.4%, **+0.0%** | 2105: +11.0%, **-0.0%** | 20467: +12.7%, **-0.9%** |
| word_w | work RAM | 23065: +0.0%, **+0.0%** | 12997: -0.0%, **-0.0%** | 2336: +0.0%, **+0.0%** | 23065: +0.0%, **+0.0%** |
| word_w | video memory | 14307: +7.7%, **+1.4%** | 7856: +12.6%, **+0.0%** | 2106: -5.0%, **-0.1%** | 20290: -5.7%, **+0.0%** |
| long_r | work RAM | 31748: -0.0%, **-0.0%** | 16672: -0.0%, **+0.0%** | 2992: +0.0%, **+0.0%** | 31748: +0.0%, **+0.0%** |
| long_r | video memory | 18336: +38.4%, **+0.0%** | 9584: +38.2%, **-0.2%** | 2624: +14.0%, **+0.0%** | 27022: +17.5%, **-1.1%** |
| long_w | work RAM | 31748: -0.0%, **-0.0%** | 16672: +0.0%, **+0.0%** | 2992: +0.0%, **+0.0%** | 31748: +0.0%, **+0.0%** |
| long_w | video memory | 18336: +8.2%, **+0.0%** | 9584: +12.7%, **-0.2%** | 2624: -5.5%, **+0.0%** | 26718: -7.3%, **+0.0%** |
| byte_r_hi | work RAM | 23065: +0.0%, **+0.0%** | 12998: -0.0%, **-0.0%** | 2336: +0.0%, **+0.0%** | 23065: +0.0%, **+0.0%** |
| byte_r_hi | video memory | 14506: +27.6%, **+0.0%** | 7856: +31.4%, **+0.0%** | 2106: +10.9%, **-0.1%** | 20407: +13.0%, **-0.6%** |
| byte_r_lo | work RAM | 22677: +0.0%, **+0.0%** | 12817: -0.0%, **-0.0%** | 2304: +0.0%, **+0.0%** | 22677: +0.0%, **+0.0%** |
| byte_r_lo | video memory | 13968: +27.0%, **+0.0%** | 7856: +31.2%, **+0.0%** | 2080: +10.8%, **+0.0%** | 20104: +12.8%, **-0.6%** |
| byte_w_hi | work RAM | 23065: +0.0%, **+0.0%** | 12997: -0.0%, **-0.0%** | 2336: +0.0%, **+0.0%** | 23065: +0.0%, **+0.0%** |
| byte_w_hi | video memory | 14300: +7.8%, **+1.5%** | 7856: +12.6%, **+0.0%** | 2106: -5.0%, **-0.1%** | 20290: -5.7%, **+0.0%** |
| byte_w_lo | work RAM | 22677: +0.0%, **+0.0%** | 12817: -0.0%, **-0.0%** | 2304: +0.0%, **+0.0%** | 22677: +0.0%, **+0.0%** |
| byte_w_lo | video memory | 13968: +7.5%, **+0.0%** | 7856: +12.6%, **+0.0%** | 2080: -4.6%, **+0.0%** | 19989: -5.6%, **+0.0%** |
| movem_r | work RAM | 44239: -0.0%, **-0.0%** | 21168: +0.0%, **+0.0%** | 3795: +0.4%, **-0.1%** | 44239: -0.0%, **-0.0%** |
| movem_r | video memory | 23088: +52.9%, **+0.4%** | 11792: +37.3%, **+0.0%** | 3187: +19.5%, **+1.0%** | 35084: +26.1%, **-0.1%** |
| movem_w | work RAM | 45738: -0.0%, **-0.0%** | 21664: +0.0%, **+0.0%** | 3888: +0.0%, **+0.0%** | 45738: -0.0%, **-0.0%** |
| movem_w | video memory | 23832: +10.6%, **+1.6%** | 11792: +12.3%, **+0.0%** | 3285: -7.2%, **-0.2%** | 35981: -9.6%, **-0.0%** |
| rmw | work RAM | 31748: -0.0%, **-0.0%** | 16672: +0.0%, **+0.0%** | 2992: +0.0%, **+0.0%** | 31748: -0.0%, **-0.0%** |
| rmw | video memory | 15647: +41.2%, **+0.0%** | 7856: +50.1%, **+0.0%** | 2624: +3.0%, **+0.0%** | 26828: +3.7%, **-0.4%** |

Over all 268 results: within 0.5% of the board 102 shipped, **231** now; within 1%, 102 and **247**; within 2%, 104
and **264**. The gap probes (bg_vram, lines 16 to 199: a word read with 0, 1, 2, 4 or 8 NOPs after it, reads and
writes in turn, read-modify-write; the chip's schedule, the CPU first, and work RAM) are all within 0.5% now
(word reads with 2 NOPs between: 6,640 against 6,661; with 8: 3,312 against 3,328).

What the board showed that the shipped model had wrong (machine/pgm.cpp, `Pgm::AccessWait`, now as below):

- Every access to the video memory, read or write, byte or word, costs 2 cycles (the shipped model: reads none,
  writes 3).
- Before each line of the picture the chip has the memory to itself, from 6 cycles before the line to 362 into it
  (the text layer's row for the line, then the background's first columns; 11.8 µs after the horizontal sync, less
  100 ns for each pixel the background is scrolled within its 32-pixel tile). The hole's length, 368 cycles, is the
  only one that puts the gap probes on the board's: 364 or 372 miss by up to 200%. The shipped model had 274 cycles
  of it and then 9 of every 64 cycles for the background's tiles.
- From the hole's end to the next, the chip's 800-ns microcycle: its first half the background's, the second the
  CPU's. An access begun in the second half goes on; one begun in the first waits for the second, unless it is a new
  one arriving at the microcycle's very first instant. So on those lines the CPU gets one access a microcycle, and a
  MOVEM's run of words one and a half.
- A read-modify-write instruction to memory (ADDQ, ADD, SUB, AND, OR, EOR to memory, CLR, NEG, NOT, Scc, the bit
  instructions and shifts on memory) fetches the next word of the program between its read and its write
  (`nr np nw`), so its write comes 4 cycles later than the port had it: the rmw rows were 50% fast without this.
  The interpreter (`M68K::Write`, `accessInto`) and the translator (`access_times`) both count it; MOVE writes
  straight after its read as before. Only the PGM's waits ask where in an instruction an access falls, so Cave's
  translations are unchanged (DoDonPachi's generated code before and after: the same, byte for byte), and on the
  PGM only `c.At(n)` of those writes changes.

What is left: a write in the whole-frame windows 1.4% short (the board's write that arrives at the microcycle's end
straggles into the next; the core's notes find it happening at random as the board's two crystals drift, and leave
it out too); MOVEM runs of words up to 1.6% off; with the CPU first, reads 0.6 to 1.1% slow. The game's own effect
over the checks' play (7,000 frames, the board's waits on): the waits a frame drop by 2% (Dai-Ou-Jou) to 12%
(Ketsui), and the frames with no new picture stay as they were (Dai-Ou-Jou 2,501 to 2,504, the others the same).

## Not done yet

- **Compared with a board only through test ROMs** (above), and with the PGM core running (docs/SLOWDOWN.md): no MAME beside it. The sound has not been listened to; the background's scale
  register (MAME does not draw it either) is not drawn.
- **Training facts** (runtime/shmup.h: stages, ships, bombs, the safe time, hit boxes) for all four games.
- **DoDonPachi II's training** is done (docs/TRAINING.md; games/ddp2/src/ddp2.h has the facts): all five features at
  its seven stages, and the hit boxes of every kind. Its ships, enemies, shots and hit tests are the 68000's, in
  work RAM; the ARM moves the enemies' bullets, in the RAM the two share. Not there: player 2's bombs and ships
  (the features are player 1's, as in every game), and a hit has not been compared with the boxes frame by frame.
- **DoDonPachi II against a board or MAME**: the pictures of its boot, opening, title, selects and first stage were
  looked at and look right, its sound comes out (not silent, and going through the stage) but has not been listened
  to; the ARM's clock is taken as the board's 20 MHz with no wait states, and the 68000 and the ARM have not been
  compared with a board, the FPGA core or MAME running. Its other regions' chips (Japan, Korea, Taiwan, Hong Kong,
  China: a byte of the chip's ROM) and older versions are not sets here.
- **Lag** measured for DoDonPachi II only (tools/lag.sh: the board 1 frame, run-ahead 1 takes it to 0), not yet for
  Cave's four.
- The battery RAM is written to the saves folder whenever it changed (every 5 seconds in play): it is the whole work
  RAM, as MAME keeps it.

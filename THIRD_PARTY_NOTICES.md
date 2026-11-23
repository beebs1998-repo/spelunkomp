# Third-party notices

spelunkomp is GPL-3.0-only. Parts of it follow or are adapted from other projects:

## MAME (BSD-3-Clause)

The CV1000's memory map and ports (machine/cv1000.*), the blitter's operations, blending and timing (machine/blitter.*),
the RTC-9701's serial protocol (machine/rtc9701.h) and the NAND flash's commands (machine/nand.h) were written with MAME's drivers as the reference: `src/mame/cave/cv1k.cpp` and
`cv1k_v.cpp` (David Haywood, Luca Elia, MetalliC), `src/devices/machine/rtc9701.cpp` (Angelo Salese, David Haywood),
`src/devices/machine/nandflash.cpp` (Raphael Nabet). The sound chip (machine/ymz770.*) follows `src/devices/sound/ymz770.cpp`
(R. Belmont, MetalliC, Olivier Galibert). Its AMM decoder (machine/amm_decoder.*) is this project's own, written from the MPEG
audio standards (ISO/IEC 11172-3 Layer II, and 13818-3's tables for the half rates) and from what MAME's ymz770.cpp and
`src/devices/sound/mpeg_audio.cpp` (Olivier Galibert) do with AMM (its header, frame lengths and table choice), by
someone who had read MAME's decoder: it is not a clean-room work. It replaces MAME's decoder, which this project carried
until then, and makes the same samples but for about one in 9,000, one step apart (the standard's window coefficients,
where MAME's had some rounded otherwise). The blitter's timing in MAME is from buffi's "CV1000 Blitter
Research" (https://cave.buffis.com/docs/CV1000_Blitter_Research_by_buffi.pdf).

    Copyright (c) the MAME contributors named above (each file's copyright-holders).

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
    following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
       following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
       following disclaimer in the documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
       products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## cps2recomp and the author's other ports (GPL-3.0-only, the same author)

The SH-3 core, the block dispatcher and the translator are adapted from the same author's SH-4 ones (a port not yet
published); the save-state archive, the
zip reader and the PNG writer are from cps2recomp. The 68000 (machine/m68k.*), its translator (tools/m68k/), the
translated code's runtime and block checker (runtime/native68k.h, runtime/recomp68k.cpp) and the 93C46 EEPROM
(machine/eeprom93c46.*) are cps2recomp's, with what is CPS-2's own taken out.

## MAME: Cave's 68000 boards (BSD-3-Clause, as above)

The memory maps, interrupts and frame timing of Cave's 68000 boards (machine/cave68k.*) and what is in the sprite
chip's and the tile layer chips' registers and memories, with the order things are drawn in (machine/cave_video.*),
were written with MAME's `src/mame/atlus/cave.cpp`, `cave_v.cpp` and `sprite013.cpp` (Luca Elia) and
`src/devices/video/tmap038.cpp` (Luca Elia, Paul Priest, David Haywood) as the reference. machine/ymz280b.* follows
`src/devices/sound/ymz280b.cpp` (Aaron Giles): its registers, its ADPCM decoding and its mixing.
machine/okim6295.* (DonPachi's sound) follows `src/devices/sound/okim6295.cpp` and `okiadpcm.cpp` (Mirko Buffoni,
Aaron Giles; okiadpcm.cpp: Andrew Gardner, Aaron Giles) and `src/devices/machine/nmk112.cpp` (Alex W. Jackson): the commands, the decoding, the volumes and the
paging of the sample ROMs. The sound side of Mazinger Z's and Air Gallet's boards (machine/cave_z80.*: the Z80's
memory and ports, the latches between the two CPUs, the chips' clocks and their shares of the mix) and the rule that
unscrambles Mazinger Z's sprite ROM (runtime/romset.cpp) are from `cave.cpp` too.

## ymfm (BSD-3-Clause)

Yamaha FM synthesis by Aaron Giles, [ymfm](https://github.com/aaronsgiles/ymfm) revision 81aec25, copyright (c) 2021
Aaron Giles: the YM2203 of Mazinger Z's board and the YM2151 of Air Gallet's. Its sources are kept as published:
`third_party/ymfm.h`, `ymfm_fm.h`, `ymfm_fm.ipp`, `ymfm_opn.*`, `ymfm_ssg.*`, `ymfm_adpcm.*` (the YM2203's file includes
them) and `ymfm_opm.*`. The licence's text is the one above, with its own copyright line, at the top of each file.

## MAME: IGS's PGM board (BSD-3-Clause, as above)

The PGM board (machine/pgm.*: its memory map, frame, interrupts, sound latches and the IGS027A's answers to Cave's
games) was written with MAME's `src/mame/igs/pgm.cpp` and `pgmprot_igs027a_type1.cpp` (David Haywood, ElSemi) as the
reference. The program's decipher (machine/pgm_crypt.cpp) takes from `src/mame/igs/pgmcrypt.cpp` (David Haywood,
ElSemi) facts only, written here in a form of its own: which bit of each word's low byte flips at which addresses for
each game, and that the high byte is exclusive-ored with a 256-byte table by the address's low byte. MAME's tables are
not here: each game's is worked out from the player's own program ROM when the set is loaded, and checked against its
SHA-1 in game.cfg. The video chip (machine/igs023.*) follows `src/mame/igs/igs023_video.cpp` (David Haywood),
the synthesizer (machine/ics2115.*) `src/devices/sound/ics2115.cpp` (Alex Marshall, nimitz, austere), and the clock
`src/devices/machine/v3021.cpp` (Angelo Salese, cam900). The Z80 (machine/z80.*) is cps2recomp's.

DoDonPachi II's IGS027A, an ARM7 that is run: its memory map, the latches, the fast interrupt and the table its
cartridge's ROM is read through (machine/pgm.*) are as MAME's `src/mame/igs/pgmprot_igs027a_type2.cpp` (David Haywood,
ElSemi, Xing Xing) describes them, and the bits that ROM's fixed decipher flips (machine/pgm_crypt.cpp, `ddp2`; it has
no table) are those of `pgmcrypt.cpp`'s `pgm_ddp2_decrypt`. The ARM7TDMI interpreter (machine/arm7.*) was written here, from the processor's behaviour as
ARM documents it (the ARM7TDMI data sheet, ARM DDI 0029); MAME's ARM7 core was not used for it.

## HyperSquid3 (BSD-2-Clause): the SH7709S's timing

machine/bus_timing.h (the CPU's cache, bus and pipeline timing: the CV1000's "Slowdown as the board") is the
project's own code, written from the SH7709S hardware manual (Renesas, REJ09B0081) and the SH-3 software manual,
with the cycles each case takes measured on [HyperSquid3](https://github.com/ika-musume/HS3) (HS3), ika-musume's
SH7709S in SystemVerilog, run under Verilator on a CV1000 board around it (docs/SLOWDOWN.md, "Checked against HS3").
No HS3 source is in this repository. The file's author had read MAME's `src/devices/cpu/sh/sh7709s.cpp` (Jesus Ramos), which
the file followed before; it was written afresh and none of MAME's model is left in it.

    Copyright (c) 2024, Raki

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
    following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the
       following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
       following disclaimer in the documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Country flags (MIT)

`recomp-common/launcher/flags.png` (compiled into the launcher) holds every ISO country's flag from
[flag-icons](https://github.com/lipis/flag-icons) 7.2.3, Copyright (c) 2013 Panayiotis Lipiridis, MIT License;
`recomp-common/launcher/src/countries.h` has their names from it. The app and the Linux release carry the license as
`licenses/flag-icons-MIT.txt`.

## librashader and RetroArch's shaders

The Graphics page's Shader (port/shading.*, adapted from cps2recomp's) draws the picture through
[librashader](https://github.com/SnowflakePowered/librashader) 0.12.1, MPL-2.0 or GPL-3.0 (used here under GPL-3.0),
copyright (c) chyyran and its contributors. Its C header, `third_party/librashader.h`, is in this repository (MIT).
`librashader.dylib` in the macOS app is built from its source (`tools/librashader.sh`) with the Rust crates its
`Cargo.lock` names; its licences are in the app's `Resources/licenses`.

The presets are RetroArch's [slang-shaders](https://github.com/libretro/slang-shaders) (commit `2b0530b`), with the
files they use, in the app's `Resources/shaders` (`tools/shaders.py`). Each file's licence is its own, stated at its
top: crt-geom-mini (after cgwg's crt-geom), CRT Royale (TroggleMonkey), CRT Guest (guest(r), Dr. Venom) and zfast-crt,
CRT Light (Greg Hogan) GPL-2.0-or-later; CRT Lottes (Timothy Lottes), scanline-fract (hunterk) and
sharp-bilinear-scanlines (rsn8887) public domain; xBRZ (Hyllian, Zenju) and ScaleFX (Sp00kyFox) and the blur and gamma
includes MIT; the stock pass (libretro), ossc (DariusG) and integer-scaling-scanlines with no notice, and
crt-geom-mini's linearize pass none either.

## FPGA cores for MiSTer: the 68000 boards' bus (read, not copied)

What docs/SLOWDOWN.md says of the 68000 boards' waits was read from the descriptions of the boards in two FPGA cores;
no code of either is in this repository. Both cores were also run, in simulations kept outside this repository, to
measure the port against them (docs/SLOWDOWN.md):

- [Arcade-IGSPGM_MiSTer](https://github.com/wickerwaka/Arcade-IGSPGM_MiSTer) (wickerwaka and contributors,
  GPL-2.0): `rtl/PGM.sv`, `rtl/igs023.sv`, `rtl/igs023_fg.sv`, `rtl/igs023_sprite.sv`: how the IGS023 acknowledges the
  68000 at its video memory, palette and registers and holds it there while the text and background layers are
  fetched (machine/pgm.cpp, `Pgm::AccessWait`), when it starts the sprite DMA and the level-4 interrupt, and its sound
  clock; the rules were measured on the core running and written fresh.
  Also read (docs/PGM.md, "How others do it"; now at
  [MiSTer-devel/Arcade-IGSPGM_MiSTer](https://github.com/MiSTer-devel/Arcade-IGSPGM_MiSTer)): `rtl/PGM.sv`,
  `rtl/igs027a.sv`, `rtl/exrom_decrypt.sv`, `rtl/share_cache.sv` and the game's MRA, for how that core runs
  DoDonPachi II's IGS027A (a real ARM7 on the dumped ROMs, at 20 MHz, the fast interrupt at a write to the latch).
  The port's ARM and its place beside the 68000 are its own.
- [Arcade-Cave_MiSTer](https://github.com/MiSTer-devel/Arcade-Cave_MiSTer) (Josh Bassett and contributors, from Rick
  Wertenbroek's work; GPL-3.0): `rtl/cave/Main.sv`, `rtl/cave/CaveCpuWrappers.sv`: the 68000's acknowledge on Cave's
  boards (none to add: see docs/SLOWDOWN.md). Run (DoDonPachi, its work RAM at every blank and its bus cycles
  compared with the port's), with the 68000 inside it, **fx68k** (Jorge Cwik, GPL-3.0), also run alone over the games'
  instruction forms: the instruction times, the interrupt's recognition and the E clock's wait in machine/m68k.cpp
  were measured so (docs/SLOWDOWN.md, "The 68000 against the Cave core running"), written fresh. The divide times
  follow Jorge Cwik's published rules for the 68000's DIVU and DIVS (2005), written fresh.

## recomp-common (GPL-3.0-only, the same author)

`recomp-common/` is a copy of [recomp-common](https://github.com/tmcleroy/recomp-common) (the in-game menu, the
settings, the launcher), kept by tools/sync-common.sh. Its own notices apply and are summed up here:

- The launcher descends from the launcher of **wiiparty-recomp** by **arelkair**
  (https://github.com/arelkair/wiiparty-recomp), **GPL-3.0-or-later**: `recomp-common/launcher/src/ui.cpp`, `ui.h`
  and `embedded.h` are that project's nearly unchanged; `layout.h`, `main.cpp`, `game_process.cpp` and
  `embed_fonts.cmake` carry parts of it, as each file says.
- Downloaded when the launcher is built and inside the built launcher: **SDL 3.4.16** (zlib License, Copyright (C)
  1997-2026 Sam Lantinga), **Dear ImGui 1.92.9b** (MIT License, Copyright (c) 2014-2026 Omar Cornut), **Inter 4.1**
  (SIL Open Font License 1.1, Copyright (c) 2016 The Inter Project Authors).
- `recomp-common/launcher/flags.png`: the flags of **flag-icons 7.2.3** (MIT License, Copyright (c) 2013 Panayiotis
  Lipiridis), its licence in `recomp-common/launcher/flags-LICENSE.txt`.
- `recomp-common/text.h`: a generic 5x7 dot-matrix font, typed in for that project.
- Downloaded when a Windows game program is built and inside it: **Switchres 2.2.2**
  (https://github.com/antonioginer/switchres, **GPL-2.0-or-later**, used here under GPL-3.0; Copyright 2010-2021
  Chris Kennedy, Antonio Giner, Alexandre Wodarczyk, Gil Delescluse): its common and Windows sources, as published
  (checked against their SHA-256: `recomp-common/switchres.cmake`), compiled into the program for the Arcade monitor
  page's modes made at the board's exact rate (`recomp-common/arcade_screen.h`). Its sources state their licence
  (it ships no licence file); the Windows download's programs are under the GPL-3.0 in its `LICENSE.txt`.

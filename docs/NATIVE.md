# Native at the edges

A port whose game code still drives a register-level model of the board is mostly an emulator. The plan for each edge
(the board model stays as the reference build, and a check runs both side by side every frame):

- **Video: done** (port/gpu.cpp, port/shaders/list.frag). VRAM is a texture on the GPU and the software blitter
  draws nothing. The game's interface to the screen was already a list (uploads of pictures into VRAM, draws of
  rectangles from VRAM with a flip, a tint and one of 64 blends, a move of the screen's window), so the list is the
  native interface: the blitter's parser hands each draw on with its clipping done (`NativeList`), and the time the
  board's blitter would have taken stays modelled, because the games wait on it and their slowdown depends on it.
  - A draw blends with what is under it, in order, so the draws are not rasterized one by one. The CPU bins a run of
    draws into 16 x 16 pixel tiles over the area they cover; one pass draws that area, and for each pixel the shader
    walks its tile's draws in the game's order, doing the board's own 5-bit integer sums on the pixel as it goes.
    That is exact, keeps overlap and order, needs no reading of the target being drawn, and works on every SDL_GPU
    backend. A run ends where a draw reads from the area the run has written (it must see those pixels); the games
    do that a few times a frame at most (SaiDaiOuJou the most: 69 passes in its busiest frame of the check).
  - The screen is drawn straight from VRAM at the scroll position, turned and scaled by the shader.
  - Save states bring VRAM back from the GPU and send it again; run-ahead keeps the areas written in a second
    texture and copies them back.
  - **The check** (`tools/check_video.sh`, `--check-video`): the software blitter run beside the GPU, the screen
    compared every frame and all of VRAM at the end. All five games: 0 pixels differ over 7000 frames each (boot,
    attract, a stage). `--software` shows the software blitter's picture through SDL's renderer.
  - To come on top of it: a sharper or smoother scaling shader as a choice, the hit boxes and readouts drawn by the
    GPU, and (later) taking the draws from the game's own sprite routines instead of the list in RAM.
- **Video on Cave's 68000 board: done** (port/gpu68k.cpp, port/shaders/sprites68k.*, compose68k.frag). The board's
  interface to the screen is a sprite list, tile layers with their registers, and a palette, so those are what the
  video model hands on (`NativeFrame68k`, machine/cave_video.h), at the moments the board uses them: the sprites as
  the chip latches its list at the vertical blank's interrupt (read from the list on the CPU as the chip reads it:
  its order, its time limit, the screen's flips), the layers and the palette as the picture is made 90 microseconds
  later.
  - The sprites are drawn as quads in the list's order into a 16-bit buffer of the chip's kind: where two overlap the
    GPU writes the later last, as the chip does. Each pixel finds the sprite's pixel that lands on it with the
    chip's own steps (256ths of a pixel apart: its zoom), the last of them that is not 0.
  - One pass then makes the picture: for each pixel the layers' tiles (each layer's scroll, line scroll and row
    select, flips), the frontmost by priority, the sprite buffer's pixel over it unless the tile's priority is
    above, the colour from the palette.
  - The GPU keeps nothing of a frame but its picture, so save states, run-ahead and the training tools need nothing
    of it put back: the board is put back and its picture drawn again.
  - **The check**: `--check-video` (and tools/check_video.sh) runs the software model beside it and compares the
    pictures every frame: DoDonPachi, 0 pixels differ over 7,000 frames (boot, attract, stage 1, up to 473 sprites)
    and over 8,000 begun at stage 4 with run-ahead and the readouts on. Zoomed sprites (none in DoDonPachi) and
    flipped screens are drawn by the same steps: their start in 256ths of a pixel, as the software model places them;
    tools/check_video.sh on all seven 68000 games, 7,000 frames each, every frame equal.
  - Both boards' renderers share port/gpu_base.*: the device, the window, presenting with the overlay, the menu's
    picture, window shots.
- **Loading.** The games read the NAND a page at a time through the CPU's DMA and unpack into RAM. The loader's
  routines are taken at their entry and served from the player's file directly.
- **Sound.** The game's calls into its sound code (start this sequence, stop, volume) are taken at the game's own
  routines and mixed natively; the YMZ770 model stays as the reference.
- **Input and saves.** The port read at the game's input routine; the EEPROM (settings, scores) and the NAND's
  written blocks kept in the player's data folder as plain files.
- **The CV1000's code: all translated in play.** The SH-3 code is translated from recordings of the blocks the games
  ran (tools/translate.sh); what no recording had was left to the block interpreter. Two things were missed:
  - The boot code in the program flash (U4) that copies the program into RAM (about 756,000 runs of one block, the
    0.7% of the check's run): the recorder and the dispatcher took only RAM. Blocks in the flash are now recorded and
    translated too (runtime/dispatch.cpp: `SH3::IsFlash`, looked at once against the flash, which never changes).
  - The later stages, the attract's demos of them, continues, game over, name entry, two players and the test menu:
    the recordings were of a coin, Start and stage 1 (the 2026-11-10 additions) or of one long run. `tools/explore.sh`
    runs the game on its translated code in all those ways side by side (`--auto` made-up play, and through the
    training facts with the headless runner's new `--features`: a game begun at each stage, out of harm with bombs
    held, then on through the stages after it), recording only what the interpreter still had to run (and four
    steps of what it leads to), then translates and builds. `MEASURE=1 tools/explore.sh ROMS 40000` gives the share
    still interpreted over a run of made-up play out of harm (a seed no recording used) and over the check's run.
  - What it did (2026-11-11; blocks interpreted over 40,000 frames of made-up play out of harm, seed 99, which no
    recording used; the check's run, 6,000 frames: about 83,000 before in each game, the boot copy, 0 after in all 14):

    | Game | Before | After | | Game | Before | After |
    |---|---|---|---|---|---|---|
    | akatana | 1,071,199 (0.05%) | 2,940 | | futaribl | 25,522,086 (1.5%) | 9,705 |
    | ddpdfk | 3,264,698 (0.2%) | 8,447 | | ibara | 4,499,856 (0.2%) | 5,081 |
    | ddpsdoj | 50,468,493 (2.5%) | 1,800 | | ibarablk | 460,836 (0.03%) | 966 |
    | deathsml | 75,091,891 (5.2%) | 0 | | mmpork | 572,610 (0.03%) | 160 |
    | dfkbl | 97,978,642 (5.2%) | 1,716 | | mushisam | 2,098,332 (0.2%) | 2,471 |
    | dsmbl | 82,739,016 (5.6%) | 0 | | pinkswts | 307,539 (0.01%) | 17 |
    | espgal2 | 25,190,666 (1.3%) | 166 | | futari15 | 57,293,702 (4.1%) | 70 |

    Every one under one block in 100,000 (of 1.1 to 2.3 billion). What is left is code no run has reached yet (a mode
    or ship a run did not pick, Futari Black Label's needed a third round): another `AGAIN=N tools/explore.sh ROMS
    40000 1` finds it.
  - The interpreter stays for what is new (code loaded over other code differs from what was recorded) and as the
    reference: `tools/check.sh` compares the two every frame.
- **The PGM's Z80: translated** (tools/z80/recompz80.py and z80.py, from cps2recomp; runtime/recomp_z80.cpp,
  runtime/native_z80.h). The sound driver is not in a ROM: the 68000 copies it into the Z80's 64 KB of RAM at boot.
  `tools/translatez80.sh` runs the game, takes the Z80's RAM once the driver is there (work/<id>/z80.bin), and
  translates everything fixed jumps reach from the reset, 38h, the NMI's 66h and the places runs saw it reach by
  computed jumps (work/<id>/entries_z80.txt; returns from interrupts are not noted: they go back to any instruction).
  - The translation keeps the bytes it was made from, and the board counts, at every write into the Z80's RAM (the
    Z80's own and the 68000's), how many of those bytes the RAM does not hold (`Z80::codeDiffers`; counted afresh at
    a reset and a loaded state). Translated code runs only while that is 0, and stops after the instruction whose
    write made it more: another driver, or the RAM before the upload, is simply interpreted.
  - It does exactly what the interpreter does, cycle for cycle, so the default needs no reference mode of its own:
    the time is looked at after every instruction (the board runs the Z80 256 cycles at a go), and translated code
    goes back to the run loop (which takes interrupts as `Z80::Step` does) after each instruction that can change
    whether one is taken: EI, the returns, HALT, and IN/OUT (the ICS2115 raises and drops its interrupt as it is
    read and written). `--z80 interpreter` in the headless runner runs the interpreter alone.
  - **The check** (`tools/checkz80.sh`): a run on the interpreter and one on translated code give the same hash of
    the board after every frame (with the Z80's RAM and registers) and the same sound, sample for sample. All four
    games, 7,000 frames of their recipes; DoDonPachi Dai-Ou-Jou and Ketsui also 20,000 frames of made-up play.
    99.1-99.8% of the Z80's cycles run translated (the rest: IN/OUT, and from a place where time ran out on to the
    next block's start).
  - **Mazinger Z's and Air Gallet's Z80** (docs/CAVE68K.md) run the same way. Their drivers are in a ROM: 16 KB that
    are always there and the rest a 16 KB bank at a time. `tools/translatez80.sh` translates the 16 KB (`--limit
    4000`: all the code is there, the banks hold the music), from the Z80's 64 KB as it sees them; what is in the bank
    would be left to the interpreter. Mazinger Z 2,251 instructions in 69 routines, Air Gallet 1,547 in 39;
    `tools/checkz80.sh`: both equal to the interpreter over 7,000 frames of their recipes, every frame's hash and the
    sound sample for sample, 99.6% and 99.9% of the Z80's cycles translated.

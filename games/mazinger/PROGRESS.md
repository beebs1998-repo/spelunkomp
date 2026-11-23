# Mazinger Z: progress

Set: MAME's `mazinger` (World, 1994/06/27), Banpresto's game on Cave's 68000 board (docs/CAVE68K.md): a Z80 with a
YM2203 and an OKI for sound, a screen 384 across, turned 90 degrees.

## State

- Plays from boot: the watchdog's reset, the warning, the title, the demonstration, the robots' select, the eight
  stages, continue. Code translated to C++ (47,076 instructions; the interpreter runs only the start-up), equal to
  the interpreter block for block over 7,000 frames (tools/check68k.sh). Drawn by the GPU, no pixel different from
  the software model's over 7,000 frames (tools/check_video.sh). Compared with nothing but itself; the sound has not
  been listened to.
- Free play, the shared menu and launcher, the practice point with replay, rewind, fast forward, run-ahead
  (tools/check_tools.sh), online play (tools/check_online.sh).
- Three buttons: the shot, the robot's special attack (free), the bomb.
- Training from the game's facts (runtime/shmup.h, games/mazinger/src), 2026-11-15: start at any of the eight stages
  or the second loop's eight, Begin at the stage (through the robots' select: the first robot), straight to the boss,
  out of harm, bombs that never run out, energy that never runs out (the game has no ships: three bars of energy).
- Hit boxes: the robot, the bullets, the enemies' bodies, the enemies only the shots can touch, the players' shots,
  read as the game's hit test (00608E) is entered.

## Facts found

All in games/mazinger/src/mazinger.h, with how each was found. In short:

- The game is one long routine that waits for each frame (0064EA), not a table of tasks. A real game: 004496; the
  robots' select 0100CC; then 004624, which only a new game reaches, where the stage (a word, 100A24) and the loop (a
  byte, 104EBE) say what is set up. The program has a stage select of its own before it (00454E), shown when the ROM's
  word at 03FFFE is 0, which it is not in this set: the stage start does what that select does.
- A stage is a script (040722, its pointer at 104E2E): entries of a scroll position and a command. Command 0x0C plays a
  tune; the boss's is 0x0D (0x0E in stage 8). Stage 3 has no boss tune. No word of RAM says a boss is on.
- A stage's bosses give up after a time, so a run that only holds the safe time gets through stages 1 to 4 (a run of
  110,000 frames); stage 5's boss does not.
- Each player's record is 0x30 bytes from 100A90: what the robot is doing (+0), where it is (+6, +0xA), the safe
  frames (+0x24), the bombs (+0x26), the robot chosen (+0x29), the energy (+0x2B), the score (+0x2C, BCD).
- Everything else is one table of 0x11F records of 0x30 bytes from 100AF0; the flags at +0x12 say what each is to the
  hit test (00608E), which builds four lists from them each frame.
- The shot held fires a burst and stops (the count at +0x1A of the record: 00A34C); it must be pressed again.

## Recipes (headless)

- games/mazinger/recipe.txt: coins at 2040, 2070 and 2100, Start at 2200, button 1 at 2400, 2500 and 2600 (the
  robots' select), the shot from 2700.
- A stage alone, with no hooks: `--poke 2300:0x100A24:N` holds the stage's word from the select on (the stage then
  plays again when it ends); `--poke 2900:0x100AB4:0x78:1` holds player 1's safe time.

## Not done, not checked

- Nothing is compared with MAME or a board: the boxes were read from the game's test and looked at over the sprites
  (stage 1, stage 5, a boss), not tried hit by hit.
- The trip to the boss was run for every stage (4,100 to 16,500 frames unseen; the picture looked at 150 and 450
  frames after: the boss on or coming in, except stage 6's, which comes some seconds later) and for the second
  loop's stages 1 and 5; the second loop's other stages were not begun, and none was played through.
- With the energy kept, the bars show two of three, and the robot still goes through its hit (0x68 frames).
- Only player 1 has the features (as in the other games); the boxes are drawn for both players.
- The scoring readouts, the other regions' settings (the energy's full set is 2 to 5 bars by the board's settings:
  the feature keeps two bars whatever the set).

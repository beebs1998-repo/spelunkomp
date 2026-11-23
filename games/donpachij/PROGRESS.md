# DonPachi (Japan): progress

Set: MAME's `donpachij` (Ver. 1.01, 1995/05/11), on Cave's 68000 board (docs/CAVE68K.md). The collection's DonPachi:
the version high-score players use. The US set (`donpachi`, Ver. 1.12) is beside it as "DonPachi (US 1.12)".

## State

Added 2026-11-23. The set differs from the US one in two files: the program (prg.u29) and layer 2's tiles (u58.bin,
the text). The board, the sprites, the other layers, the sound and the EEPROM are the US set's.

- Code translated to C++ (`GAME=donpachij tools/translate68k.sh ROMS 1 12000`: one round of made-up play, then the
  check's own places): 41,429 instructions in 1,141 routines. `tools/check68k.sh ROMS 7000`: translated equal to
  interpreted over 7,000 frames of the recipe's play (119 million blocks checked, 311 not comparable: the OKI chips
  written and read back), and two runs alike.
- The work RAM is the US program's, address for address; the code from 020000 on sits 6 to 0xF2 bytes earlier, the
  code below it (the main loop, the hit tests) where it was. Every address of src/donpachij.h was found again for
  this program (it says how).

## Training

Free play, start at stage, straight to the boss, out of harm, bombs and ships that never run out, the hit boxes:
the US version's, from the same facts. Checked 2026-11-23 in the game program's pictures and RAM dumps: free play
starting a game with no coin (the credits byte found again by three coins: 0, 1, 2, 3); a game begun at stage 3
(through stage 1's carrier and the game's own stage end) with out of harm, the bombs used and filled again and the
hit boxes drawn (the ship, the bullets, the bodies, the ground enemies, the shots and the laser); the trips to stage
1's boss (the tank formation, about 5,400 frames unseen) and stage 5's (its warning, about 18,600); ships kept at two
while the ship is lost again and again.

## Not checked by a person

Played only by scripts; nobody has played it through or listened to it.

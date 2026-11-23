# Deathsmiles MegaBlack Label: progress

Set: MAME's `dsmbl` (2008/10/06 MEGABLACK LABEL VER), CV1000-D, a horizontal game. See games/mushisam/PROGRESS.md for the board's state, which is shared.

## State

Added 2026-11-10: boots, attract, a game started and played by a script, with free play (the credits byte is in
src/dsmbl.h). Code translated from one recording of the first stage; later stages run partly interpreted until they are
recorded too.

## Training

From its facts (src/dsmbl.h, found 2026-11-11 as Deathsmiles' at this version's places): start at stage (nine
places: the map's six, a castle, a canyon of this version's, another castle), straight to the boss (the bosses'
tunes as Deathsmiles'; the castles' and the canyon's are not known), out of harm, bombs and ships that never run out,
hit boxes of the ship and the bullets. Tried: each place begun (shots after 15, 30 and 60 seconds); 40 seconds of bomb
spam with out of harm and the bombs kept (no life lost) and without (two ships lost); a still ship with the life kept
at two ships playing on past where the game ends.

Added 2026-11-12: the enemies' bodies in the hit boxes. The test is Deathsmiles' after all (0x0C1E6D00, called from
the ship's pass 0x0C1E61E4 with the kinds 0, 1 and 21, members with bit 0 of the byte at +27); 0x0C1E7120 and
0x0C1E7390, taken for it before, test the enemies against the player's shots. Buttons checked (both versions): 1 shot
left, 2 shot right (the character turns: the ship's +0x198 0x180 / 0x80), 3 bomb, 4 nothing; game.cfg says so now.

## Not checked by a person

Everything: nobody has played it or listened to it.

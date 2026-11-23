# Ibara: progress

Set: MAME's `ibara` (2005/03/22 MASTER VER.., '06. 3. 7 ver.), CV1000-B. See games/mushisam/PROGRESS.md for the board's state, which is shared.

## State

Added 2026-11-10: boots, attract, a game started and played by a script, with free play (the credits byte is in
src/ibara.h). Code translated from one recording of the first stage; later stages run partly interpreted until they are
recorded too.

## Training

The practice point with replay, rewind, fast forward and the input display every game has (docs/TRAINING.md). Out of harm, bombs and ships that never run out, start at stages 2-6, straight to the boss, hit boxes (src/ibara.h). The stage start sends the game's own script on past the stages before the one chosen as its first stage begins (Facts::scriptStage). Out of harm was moved 2026-11-11 from 0x0C2839FC (16 bits, 137) to the byte 0x0C2839FD held at 59: the old value stopped a lost ship from ever coming back.

## Not checked by a person

Everything: nobody has played it or listened to it.

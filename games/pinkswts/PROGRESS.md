# Pink Sweets: Ibara Sorekara: progress

Set: MAME's `pinkswts` (2006/04/06 MASTER VER....), CV1000-B. See games/mushisam/PROGRESS.md for the board's state, which is shared.

## State

Added 2026-11-10: boots, attract, a game started and played by a script, with free play (the credits byte is in
src/pinkswts.h). Code translated from one recording of the first stage; later stages run partly interpreted until they are
recorded too.

## Training

The practice point with replay, rewind, fast forward and the input display every game has (docs/TRAINING.md). Out of harm, ships that never run out, start at stage, straight to the boss (src/pinkswts.h: Ibara's script engine). Out of harm held at 59 (was 164, which kept a lost ship from coming back). Added 2026-11-12: hit boxes (the ship's, the bullets', the bodies': read at the entry of the collision pass's first test, 0x0C076AE0, from its own lists; the owners' flags 0x80000000/0x40000000 turn the boxes' offsets) and, standing for the bombs, the Rose Cracker kept charged (the game has no bombs: its gauge, record +0x286, full at 128, is held there and every press of the shot releases one). Button 2 is not a bomb (it only marks the record; the game is a 2-button game, A+B for speed). See src/pinkswts.h.

## Not checked by a person

Everything: nobody has played it or listened to it.

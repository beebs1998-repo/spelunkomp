# Muchi Muchi Pork!: progress

Set: MAME's `mmpork` (2007/ 4/17 MASTER VER.), CV1000-B. See games/mushisam/PROGRESS.md for the board's state, which is shared.

## State

Added 2026-11-10: boots, attract, a game started and played by a script, with free play (the credits byte is in
src/mmpork.h). Code translated from one recording of the first stage; later stages run partly interpreted until they are
recorded too.

## Training

The practice point with replay, rewind, fast forward and the input display every game has (docs/TRAINING.md). Out of harm, ships that never run out, start at stages 2-5, straight to the boss (src/mmpork.h: Ibara's script engine). Out of harm held at 59 (was 189, which kept a lost ship from coming back). Added 2026-11-12: bombs that never run out (the stock byte 0x0C510D3C, record +0x214; the bomb is button 3, input 0x40: round 1 pressed button 2, the lard attack, so game.cfg's labels are now Shot|Lard attack|Bomb|Full auto; button 4 unverified) and hit boxes (Pink Sweets' scheme at other places: the collision pass 0x0C074250 bodies / 0x0C0745C0 bullets; ground enemies have no body box, as the game tests them). See src/mmpork.h.

## Not checked by a person

Everything: nobody has played it or listened to it.

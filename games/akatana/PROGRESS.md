# Akai Katana: progress

See games/mushisam/PROGRESS.md for the board's state, which is shared.

## Training

The practice point, rewind and fast forward every game has (docs/TRAINING.md); out of harm; bombs and ships that never
run out (found 2026-11-11: player 1's record at 0x0C7E8628, the same in every game and stage played; the ships are a
life of 1600 a ship, kept at two ships; the bombs a byte with its most beside it). Tried: a still ship through stage 1
plays on where it would continue; the safe count and ships held through three stages (the record unmoved).

Added 2026-11-12: start at stage (2 to 6: the game's own way on to a stage, 0x0C1F48F0, given the stage chosen where a new
game calls it with the first; the game plays a stage's end and begins the stage chosen with the ship flown), straight
to the boss (boss tunes 6/7 in stages 1 to 4, 18/19 and 20/21 in the last; the ship flown is record +0xAC; tried at the
bosses of stages 2 and 3; stage 5's boss tune is not known, so its trip times out) and hit boxes (SaiDaiOuJou's engine
of collision kinds, its table at 0x0C7E45F8, the boxes unturned for a horizontal game; while the ship is out of harm
the game gives it a box of 24 pixels each way, drawn as it is). The facts are in src/akatana.h. The bomb is button 2.

Added 2026-11-25 (#55): the energy kept full, a switch of its own (record +0x14, 32 bits: added 20 an energy item by
0x0C1F9470, which holds it at the bar's 8,000 (+0x1C) less the life's ceiling (+0x0C); spent 5 a frame in Phantom
form). Phantom form is button 4 (input 0x80, held) and button 3 is full auto, so game.cfg's labels for them were
swapped. Tried: Phantom held 1,800 frames with the gauge full throughout, where without it the energy ran out in
under 400; with one ship left the gauge fills to 6,400, the bar's end.

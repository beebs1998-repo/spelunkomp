# Deathsmiles: progress

Set: MAME's `deathsml` (2007/10/09 MASTER VER), CV1000-B, a horizontal game. See games/mushisam/PROGRESS.md for the board's state, which is shared.

## State

Added 2026-11-10: boots, attract, a game started and played by a script, with free play (the credits byte is in
src/deathsml.h). Code translated from one recording of the first stage; later stages run partly interpreted until they are
recorded too.

## Training

The practice point with replay, rewind, fast forward and the input display every game has (docs/TRAINING.md), and from
its facts (src/deathsml.h, found 2026-11-11): start at stage, straight to the boss, out of harm, bombs and ships that
never run out, hit boxes.

- Start at stage: a stage is a place on the map. A game begins with the count of stages played (0 to 8) given to its
  new-game routine (0x0C06F040, r5); given n the game skips the map and begins the place numbered n (the default order:
  port town, graveyard, forest, lake, ruins, cavern, then two castle stages: the 7th after a story scene and the map's FINAL, the 8th after another scene), its graphics loaded in full.
  Tried: each of the eight, a shot after 15 and 30 seconds. The level (1 to 3) of a place begun this way is the
  game's own default for that slot; after it the game goes on in its own order (a ruins start played the ruins twice).
- Out of harm: the ship's safe count (kPlayer + 0xAC) held at 64; the collision routine leaves the ship alone while it
  is not 0. Ships: the life word (130 a ship) kept at two ships, less 1. Bombs: a byte, three.
- Boss tunes 12/13 and 17/14 (the last places' are not known: there the trip to the boss gives up after 30000 frames).
- Hit boxes: read after the frame from the collision kinds (they last): ship kind 2, bullets kind 20 (points), bodies
  kinds 0, 1, 21 with bit 0 of +27. Checked by drawing a RAM dump's boxes on the same frame's shot: dots on the bullets,
  the ship's 4 x 4 box on the girl.

The controls: the bomb is button 3 (0x40); button 4 does nothing (game.cfg's labels say otherwise: not changed).

Buttons checked 2026-11-12: 1 shot left, 2 shot right (the character turns that way: the ship's +0x194 0x180 / 0x80),
3 bomb, 4 nothing; game.cfg's labels (they said 2 lock shot, 3 shot right, 4 bomb) now say so.

## Not checked by a person

Everything: nobody has played it or listened to it.

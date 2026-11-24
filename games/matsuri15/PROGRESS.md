# Mushihimesama Matsuri Ver 1.5: progress

Set: FinalBurn Neo's `matsuri15` (2011/5/23 CAVEMATSURI VER1.5), CV1000-B; the files' checksums are those of the
set in the MAME fork issue #3 points to. See games/mushisam/PROGRESS.md for the board's state, which is shared.

## State

Added 2026-11-24: boots, attract, a game started and played by a script, with free play. The code was translated from
one recording of the first stage (with the shot held and the ship moved); later stages run partly interpreted until
they are recorded too.

The program is Mushihimesama's reworked: the credits byte and the main loop's wait are where Mushihimesama has them,
but most routines and nearly all of RAM moved, and the player's record grew (8 bytes before the ship's state, more
before the out-of-harm count). Every place in src/matsuri15.h was found again by watching the record's fields being
written (SPELUNK_WATCH) and breaking on the routines (SPELUNK_BREAK), with Mushihimesama's as the model.

## Training

The practice point with replay, rewind, fast forward and the input display every game has (docs/TRAINING.md). Start
at stage, straight to the boss, out of harm, endless bombs and ships, and the hit boxes. Checked in headless runs: a
game begun at stage 3 (the volcano, with the 100,000 points the game gives a later start); the trip to stage 1's boss
(tune 4, 5,307 frames after the ship is flown); a game with the shot and bomb held and out of harm and both endless
features on still in play at frame 12,000 where the same play without them is over; the hit boxes over stage 1 in
the game program's pictures (the ship, the bullets, the enemies', the ground's, the shots' and the lasers').

Checks: tools/check.sh (and TIMING=1) 6,000 frames, check_video 0 of 7,000 frames differ, check_tools ok.

## Not checked

- Only the mode the shot select leaves on (Original) was played; the game's other modes, its Ver 1.5 rules among
  them, were not tried with the features.
- By a person: nobody has played it or listened to it.

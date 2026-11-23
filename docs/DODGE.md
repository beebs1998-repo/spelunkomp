# Dodge training

**Paused** (2026-11-16) and not in the downloads: its state and what is left are in docs/plans/DODGE.md.

An alternative mode (the in-game menu's Alternative modes page), in **DoDonPachi** so far: your ship
alone against an emitter at the top of the screen, where a boss would be, that fires the game's own enemy fire for you
to dodge. Every pattern the game's enemies and bosses fire is there, one at a time, in both loops and from the hidden
last boss: 439 patterns.

## Playing it

In the in-game menu: **Alternative modes > Dodge training > Begin Dodge training**. The board starts again and is run
on unseen, through a coin and Start, to the boss of the stage the game begins at (Training mode's Start at stage chooses
which stage's arena); the boss is taken away, and the emitter is there instead, drawn as what fires the pattern: the
enemy or the boss, as the game drew it when the pattern was recorded. Reset, or Training mode's Begin at the stage,
leaves the mode.

- The top of the picture names the pattern being fired (its stage, whether a boss's, its number among the stage's; a
  `2-` before it: the second loop's), the hits you have taken, and the seconds since the last.
- **Start** goes on to the next pattern.
- Ships never run out: a ship lost is counted and given back. Bombs are as in the game (they clear the bullets, and
  nothing is fired while one is out), or never run out with Training mode's feature.
- The practice point, rewind, replay, hit boxes, save states and run-ahead all work as in the game.

What is fired is chosen on the same page, under the row that begins the mode:

| Row | Choices |
| --- | --- |
| Fires | Every pattern in turn (the game's order, from the pattern chosen below); One pattern, over and over; A scenario |
| Pattern's stage, Pattern | The pattern: a stage (or Hibachi, the hidden last boss), and which of the stage's patterns, its enemies' first, then its boss's |
| Loop | The patterns as the first loop fires them or as the second does (more bullets, faster, and the bullets enemies leave as they are destroyed) |
| Scenario | Warm-up (the easiest fifth, in any order), Every enemy, Every boss, Hard (the hardest quarter), Crossfire (a boss's pattern and an enemy's at once), The last boss, Gauntlet (every pattern from the easiest to the hardest) |
| Each pattern for | 15 or 30 seconds, 1 minute, 10 seconds |

How hard a pattern is, for the scenarios, is its bullets a second, counted more for fast bullets and aimed ones.

## How it is made

**The bullets are the game's own.** The emitter makes each bullet by calling the game's bullet routine (four of them:
one of 64 directions or one of 256, in the first loop and in the second; games/ddonpach/src/ddonpach.h), with a kind,
a speed, an angle and a place. From then on the bullet is the game's: it moves it, draws it, tests it against the ship,
and a bomb clears it. Aimed bullets are aimed by the game's own two aim routines at the moment they are fired.

**The patterns are taken from play** (tools/dodge): `record.sh` plays the game through both loops four times, the ship
out of harm and moved about at random, and writes a line for every bullet the game makes (221,000 to 242,000 a run):
its kind, speed, angle and place, the object that fired it, the place in the program it was fired from, and where the
ship was. `extract.py` makes the patterns from them:

- an object's fire is split by the place in the program it came from; places that fire at the same times are one
  attack; an attack's fire is split where it pauses for 45 frames into bursts;
- the burst of the usual size stands for the attack, and the usual time from one burst to the next is its pause (a
  burst is cut at 600 frames and 400 bullets);
- each bullet's angle is found to be **fixed** (the same in every burst seen), or the **game's aim at the ship** plus a
  fixed turn (the coarse aim or the fine one, from the bullet's place or the object's, as it is fired or as the burst
  began), or the **burst's first bullet's angle** plus a turn, by which of these holds across all the bursts of all the
  recordings, the ship being somewhere else in each. What is none of these (a pattern that turns on from one burst to
  the next, a random one: about a quarter of the bullets) is fired as it was recorded;
- the same burst from two places (a turret and its twin) or two objects is one pattern; of an object's small bursts the
  three most seen are kept, of its large ones the eight most seen.

**The emitter's look** is from a second step of the recording (`LOOKS=1 tools/dodge/record.sh`): the same runs again,
which play out the same, noting what each of the game's tasks put on the sprite list in the frame each pattern's burst
began. A pattern's look is its firing object's sprites then (a boss's: every object's of the stage, which in a boss's
fight is the boss), put on the game's sprite list each frame. A pattern without one is drawn as the carrier a game's
first ship is launched from.

The result is work/ddonpach/dodge_patterns.inc (made, not written; data recorded from the game, so, like the translated code, never committed: a build without it has no Dodge training). A bullet's place is kept from the middle of
its burst's fire, which is put at the emitter: 64 pixels down from the top, in the middle.

**The arena is a boss's, without the boss.** As the boss's warning begins, the stage's objects are taken away, and the
task that brings a stage's objects in (the boss among them) is stopped: the emitter has its turn there instead, once
a frame, inside the game's frame. With no objects, the memory the game keeps for them holds the emitter's place and the
mode's state, so everything the mode keeps is part of the game's memory: the practice point, rewind and run-ahead put
it back with the rest (checked: a rewind and a practice point with run-ahead end in the same state as a run without).

## Not yet

- The other games.
- Patterns are bursts of one attack; a boss's phases, where several attacks overlap, are not kept as such (Crossfire
  fires two patterns at once, any two).
- Patterns whose angle could not be explained are fired as recorded: the same each time.
- A pattern's name is a number. Nobody has yet compared each pattern with its enemy by eye.

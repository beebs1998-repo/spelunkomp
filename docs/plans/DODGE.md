# Plan: picking Dodge training back up

For an agent taking this over. **Paused by the owner on 2026-11-16 ("good work but still needs a lot of tweaking").
Do not work on it, or port it to other games, until the owner says so.** What the mode is and how it is made:
docs/DODGE.md. This file is the state it was left in and what is still to do.

## Where everything is

All the code is on main and builds; only the patterns are kept out of git.

| What | Where |
| --- | --- |
| The mode (sequencer, scenarios, the menu page and its rows, the readout, hits) | runtime/dodge.h, runtime/dodge.cpp |
| DoDonPachi's part (the arena, the emitter, the game's bullet and aim routines called as guests) | games/ddonpach/src/dodge.cpp, the "Bullets made" facts in games/ddonpach/src/ddonpach.h |
| The menu: Alternative modes first, Dodge training's page under it | port/main.cpp (kModesPage, dodge::kPage), runtime/game.h |
| Recording play and making the patterns | tools/dodge/record.sh, tools/dodge/extract.py |
| **The patterns** (439, with the emitter's looks and colours) | work/ddonpach/dodge_patterns.inc: never committed (data recorded from the game) |

CMakeLists.txt builds the mode only when work/ddonpach/dodge_patterns.inc is there (it defines SPELUNK_DODGE); without
it games/ddonpach/src/dodge.cpp is empty and the menu has no Dodge training. The installed app has it; a fresh clone
and the release downloads do not.

**Copies of the patterns file** (SHA-256 a7e3a872a2f61a0e...), if work/ was lost:

- `git show 301fd1b:games/ddonpach/src/dodge_patterns.inc` (the private repository's history; it was committed until
  590a0d7, and the private history is kept in a private archive when the project goes public);
- a copy in the owner's notes beside the repository (spelunkomp-notes/dodge/, outside git);
- or make it again from the player's set: `tools/dodge/record.sh ROMS OUT`, `tools/dodge/extract.py
  work/ddonpach/program.bin work/ddonpach/dodge_patterns.inc OUT/r?.txt`, then `LOOKS=1 tools/dodge/record.sh ROMS OUT`
  and extract.py again (the looks). Four headless runs through both loops; read record.sh's header.

## What is open (the owner's list from the playtest, and what was known)

1. **About a quarter of the bullets are fired as recorded**: their angle is none of fixed, the game's aim plus a turn,
   or the burst's first bullet's angle plus a turn (patterns that turn on from burst to burst, random ones). They are
   the same every time. Find the rule for the common ones (a turn that grows per burst, an aim at an older ship place,
   the game's random number) in the firing routines, and model it.
2. **A boss's phases are split into bursts of one attack.** Where several attacks overlap (most bosses) they are not
   fired together; Crossfire fires two patterns at once, any two. Group a boss's attacks by the boss's own phase (its
   task's state or its life) so a phase is fired whole.
3. **Pattern names are numbers** (stage, boss or not, number among the stage's). Name them by their enemy (a list by
   eye, against the game) or at least by enemy kind.
4. **22 patterns have no look** (drawn as the ship's carrier): their firing object had nothing on the sprite list in the
   frame the burst began; take its sprites from the frames before.
5. **Stage 4's boss's parts have the wrong colours**: its palettes are loaded in parts during the fight; take the
   palette lines at the burst's frame, not the fight's first.
6. **The readout's place** (the pattern's name, hits, seconds) over the boss's life bar was not liked: ask the owner.
7. Nobody has compared each pattern with its enemy in the game by eye; the owner played it once (2026-11-15).
8. Before it is in a public download: the patterns must be made on the player's machine from their own set (a first-run
   step: the recordings run unseen, a few minutes), or the downloads leave the mode out as now. The README and
   CHANGELOG then get it back (it was taken out of both on 2026-11-19).

## Other games, when the owner asks

DonPachi is the closest (the same engine: its bullet and aim routines and the stage-objects task to find). The other
68000 and PGM games each need their own bullet, aim and arena facts. The CV1000 games also need a way to call the game's
routine as a guest from a hook on the SH-3 (the 68000 side has M68K::CallGuest).

## Ways of working that helped

- Test runs of the game program pass `--settings <an empty file>`, or the owner's settings.ini interferes (scripted
  Start did nothing). The `--menu` test WRITES that settings file.
- The menu path that begins the mode: `--menu F:dddccc:shot` (Alternative modes > Dodge training > Begin).
- Free play on the recordings: Start at frame 4100, then button 1 at 4300/4400.
- Headless `SPELUNK_TRACE_AT=addr,...` (with `SPELUNK_TRACE_MEM`) and `--cpu blocks`; `--dump-ram FRAME:file` also writes
  the sprite list (`.sprites`).
- The checks the mode must keep passing: tools/check_tools.sh ddonpach (a rewind and a practice point with run-ahead
  end in the same state as a run without), tools/check68k.sh, check_video.

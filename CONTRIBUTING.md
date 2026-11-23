# Contributing

Thanks for helping! This repository is Spelunkomp: the toolkit (the boards, the translators, the game program),
the launcher, and every game ported with them (`games/<id>/`). A change to the toolkit reaches every game at once.

## The rules that keep this project legal

These aren't negotiable; pull requests that break them are closed.

1. **Never add anything from a game.** No ROM or part of one, no extracted graphics, sounds, palettes or text, no
   settings memory (EEPROM, NAND), save states, RAM dumps, recordings of play or video, and nothing generated from a
   game: translated C++ (`work/<id>/gen`), block recordings, entry lists, disassembly listings. What a game's folder
   may hold is its own code, its `game.cfg` (the ROM set's file names and checksums, the board's settings), notes
   found by playing and watching memory (addresses, what they mean), and `recipe.txt` (the presses the checks play
   it with). The README's pictures of the games are made by the programs and kept few. The same goes for issues:
   attach logs, not game files.
2. **Never point to where ROM sets can be found.** The programs work with the player's own copy of each game.
3. **Nothing from the makers' own materials or from leaked material**: no leaked source, SDK, tools or documents,
   and no names taken from them. Work from the boards' behaviour, your own play and your own analysis of the code
   (clean-room: what you learn, written in your own words and code).
4. **No emulator's code**, beyond what `THIRD_PARTY_NOTICES.md` lists. Reading an emulator to learn how the hardware
   behaves is fine; copying its code in is not. A library this project cannot do without goes in `third_party/` as
   its project publishes it, with its notice in `THIRD_PARTY_NOTICES.md`, and its state goes in the save state like
   the rest of the machine; a file of ours written with another project as the reference says so in its first
   lines and is named in the notices.
5. **Only add code you have the right to contribute**, under the GPL-3.0. Code from other projects must have a
   compatible license and be credited in `THIRD_PARTY_NOTICES.md`.

## The rules that keep the games what they are

- **Every feature is opt-in.** Anything that changes how a game plays, looks or is controlled is a feature (the
  launcher's Features page, the in-game menu), off unless the player chooses it. With every feature off a game runs
  exactly as the board runs it: the same frames, the same slowdown.
- **No added input lag.** Never more than the board, and less where the board itself adds some (docs/LAG.md):
  controls are read at the moment the game reads them. Measure with `tools/lag.sh` when timing, input, sound or
  presentation change.
- **Keyboard and gamepads both work**, for everything a player can do.

## Where things go

The toolkit (everything outside `games/`) may not be about one game: no addresses, names or special cases of a
game's. What a game needs that others do not goes in its `game.cfg` or its own sources (`games/<id>/src`: its
names header and its facts for the training features, `runtime/shmup.h`). A feature every game can have belongs in
the toolkit.

`recomp-common/` is a copy of another repository and is never changed here: changes go to
[recomp-common](https://github.com/tmcleroy/recomp-common), then `tools/sync-common.sh`. `make check-common`
fails if the copy was changed.

## Building and testing

`make check` needs no game: it audits the files (`tools/audit.sh`), checks the copy of recomp-common, and starts
the game programs and the launcher that were built (`make games`, `make launcher`) with no game and no window. CI
does the same on macOS, Linux and Windows, with every game's program built without translated code (it then runs
the game in the board's interpreters). [docs/BUILDING.md](docs/BUILDING.md) has the rest.

A change to a board, a translator, the picture, the sound or a game's code needs the games as well, which only
someone with their ROM sets can run:

- `tools/check.sh ROMS` (CV1000) and `tools/check68k.sh ROMS` (the 68000 boards and the PGM; `tools/checkz80.sh`
  for a Z80 sound driver): the translated code and the interpreter must give the same state, and two runs must be
  alike;
- `tools/check_video.sh ROMS`: the GPU's picture against the software model's, pixel for pixel;
- `tools/check_tools.sh ROMS`: the practice point and rewind put the game back exactly;
- `tools/check_online.sh ROMS`: two copies in an online match stay the same;
- `tools/lag.sh ROMS`: the frames from a press to the picture;
- `tools/gate.sh ROMS [GAME...]`: the translation, picture and training tools checks for every game built.

Set `GAME=<id>` for the scripts that take one game. A toolkit change is checked with every game, a game's change
with that game. Say in the pull request which were run.

## Code style

- Match the surrounding code: naming, comment density and idiom. Comments explain why.
- C++20; 4-space indents, braces on the same line, lines up to about 120 columns.
- Every source file starts with the license and copyright lines the others have (after a `#!` or `#version` first
  line); the audit fails on one without. A tool that writes a source file writes them too.
- Commit messages: a short summary line, then what changed and why.

## Code of conduct

Be kind and constructive. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

# How it works

Spelunkomp is a static recompilation: each game's program is translated to C++ ahead of time and compiled, and runs
on a model of its board's hardware written for this project. One toolkit serves every game; a game's folder holds
only what is its own.

## The parts

| Folder | What is in it |
| --- | --- |
| `machine/` | The boards. The CV1000 (`cv1000.*`: an SH-3, `sh3.*`; the blitter, `blitter.*`; the NAND flash, the RTC and EEPROM; the YMZ770 sound chip), Cave's 68000 board (`cave68k.*`, `m68k.*`, the sprite and tile chips in `cave_video.*`, the YMZ280B, the OKI M6295s, the Z80 sound side of Mazinger Z and Air Gallet), and IGS's PGM (`pgm.*`, the IGS023 video chip, the ICS2115 synthesizer, the Z80, and the ARM7 of DoDonPachi II's IGS027A). `board.h` picks the board a game's build is for |
| `tools/` | The translators: `translate.py` for the SH-3 (blocks recorded by running the game on the interpreter), `m68k/` for the 68000 (everything fixed branches reach, and the entries running finds), `z80/` for the sound drivers; the scripts that run them and the checks (CONTRIBUTING.md) |
| `runtime/` | What the translated code runs on (the dispatcher, the block registries), the ROM set loaders, the timeline the practice point and rewind keep, and the training features built from a game's facts (`shmup.*`, `hit_boxes.h`, `dodge.*`) |
| `port/` | The game program: the window, the GPU renderer (`gpu*.cpp`, the shaders in `shaders/`, compiled into `shaders.inc` by `tools/gen_shaders.py`), the keyboard and gamepads read when the game reads its ports, the in-game menu, save states, online play (`netplay.*`) and the headless runners the checks use |
| `launcher/` | The collection's description for the shared launcher |
| `games/<id>/` | One game: `game.cfg` (its ROM set, board, screen and buttons), `src/` (its names header: the addresses of its stage, ships, bombs and hit tests, and what its features need) and `recipe.txt` (the presses the checks play it with) |
| `recomp-common/` | The menu, settings and launcher shared with the other recomp ports: a copy, kept by `tools/sync-common.sh` |
| `third_party/` | Other projects' files, as they publish them (THIRD_PARTY_NOTICES.md) |

## The rules it keeps

- **With every feature off a game runs as the board runs it.** The translated code is checked block by block against
  the interpreter (`tools/check.sh`, `tools/check68k.sh`, `tools/checkz80.sh`), the GPU's picture pixel by pixel
  against the software model (`tools/check_video.sh`), and the training tools for exact restore
  (`tools/check_tools.sh`).
- **No added input lag** (docs/LAG.md): the controls are read at the moment the game reads them, and each frame is
  shown as soon as it is done.

## More

- [CAVE68K.md](CAVE68K.md) and [PGM.md](PGM.md): the 68000 board and the PGM.
- [NATIVE.md](NATIVE.md): the GPU renderer and what is native.
- [SLOWDOWN.md](SLOWDOWN.md): the CV1000's cache and bus timing.
- [LAG.md](LAG.md): input lag, measured.
- [TRAINING.md](TRAINING.md), [DODGE.md](DODGE.md): the training features and how each game's were found.
- [ONLINE.md](ONLINE.md): online play and rollback.
- [ROADMAP.md](ROADMAP.md): what comes next.

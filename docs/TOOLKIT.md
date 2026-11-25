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

## The board's settings and autofire

These boards have no DIP switches: each game keeps its settings (difficulty, lives, extends) where its test menu (F2)
writes them, the EEPROM (the CV1000's RTC-9701, the 93C46 of Cave's 68000 board) or the PGM's battery RAM. A game's
`game.cfg` declares those it offers on the **Board** page (under Game), in `runtime/board_settings.h`'s way, as
cps2recomp's board options are declared:

```
board_option_1 = board_difficulty | Game difficulty | eeprom 4:8-9 | How hard the game is. | A (easy)=1 | B (normal)=0
board_option_2 = board_ships | Ship stock | ram 801006.b | The ships a game starts with. | 1=3 | 2=4 | 3=0
board_checksum = eeprom 1 = sum 2-7
```

Consecutive from 1. `where` is `eeprom W:low-high` (the EEPROM's word W, 0 to 255, bits low to high; a single bit
`eeprom 4:2`, the whole word `eeprom 4`) or `ram ADDRESS.b:low-high` / `.w` (the battery RAM at the 68000's address,
hex). A choice's value is as its bits hold it (decimal, or `0x` hex). The first choice, **As the board is set**, writes
nothing; with every one there the board is as it was. `board_checksum` (several, between commas) recomputes a word the
game checks as the 16-bit sum of others, after a setting is written.

The settings are put in as the board is made (its power-on and Reset), before the game reads them; the saves folder's
copy keeps the board's own values (the test menu's) in their place, so going back to As the board is set gives the
board again. A store the game has never written (a CV1000's EEPROM on a new install: the game writes its own at its
first power-on) has that first power-on run unseen first, on a board of its own. Online, the host's (the match is the
host's board). A declaration that cannot be read stops the program and `--list-features`, which `tools/ci_smoke.sh`
runs; it also checks every declaration is listed. Each game's were found by changing each setting in its test menu and
reading back the EEPROM (the saves folder's `eeprom.bin`) or the battery RAM (`--dump-ram`).

**Autofire** (the Controls page, every game): a choice for each of the game's buttons (game.cfg's `buttons`), Off or 30,
20 or 15 presses a second of the board's frames (pressed one frame of two; two of three; two of four). It is put into
the controls the game is given by the board's frame number, where the frame reads them (`Controls::Read`), so a
replay of what was read, rewind and run-ahead have the same, and the practice point's replay too. Online each player's
is sent in the hello or welcome and put in by both sides as each frame is run (port/netplay.h: `TheirFeatures`); the
controls sent are as held. `--input-log FILE` writes the controls each frame was given, for checking it.

## More

- [CAVE68K.md](CAVE68K.md) and [PGM.md](PGM.md): the 68000 board and the PGM.
- [NATIVE.md](NATIVE.md): the GPU renderer and what is native.
- [SLOWDOWN.md](SLOWDOWN.md): the CV1000's cache and bus timing.
- [LAG.md](LAG.md): input lag, measured.
- [TRAINING.md](TRAINING.md), [DODGE.md](DODGE.md): the training features and how each game's were found.
- [ONLINE.md](ONLINE.md): online play and rollback.
- [ROADMAP.md](ROADMAP.md): what comes next.

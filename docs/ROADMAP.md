# Roadmap

The aim: the definitive way to play and to train on Cave's arcade shooting games: the CV1000's first, and from
2026-11-10 the boards before it too (docs/CAVE68K.md: the 68000 board, DoDonPachi first). Native, readable code that runs on macOS,
Linux (Steam Deck, Steam Machine) and Windows; never an emulator wrapper.

Two rules sit above every item:

- **No input lag.** Never more than the board, and less where the board itself adds some (docs/LAG.md). Measured for
  every game, in the gate.
- **With every feature off, a game runs exactly as the board runs it**: the same frames, the same slowdown.

## Order of work

1. **The board** (done for a first cut): SH-3, blitter, NAND, RTC and EEPROM, inputs. Three games boot and play
   without a window.
2. **Translated code**: the game's SH-3 code as C++ (tools/translate.py), checked block for block against the
   interpreter, no interpreter fallbacks in play.
3. **Sound**: the YMZ770 (sequenced voices and its compressed streams), then the game's sound calls taken natively.
4. **The game program**: window, SDL_GPU presenter, keyboard and gamepads, the shared in-game menu and launcher
   (recomp-common), save states, remapping, free play, the collection app in /Applications.
5. **Slowdown as the board has it**: the SH7709S's cache and bus timing and the blitter's time decide when a Cave
   game slows down, and players train against it. Modelled, measured against known figures, and then offered as a
   choice (as the board / none / a fixed amount).
6. **Native at the edges** (docs/NATIVE.md; the owner's priority ahead of more games: the GPU drawing the game's
   own draw lists, as the CPS-2, NAOMI and CPS-3 collections now do): the game's draw lists drawn by the GPU, its sound calls mixed natively,
   input, saves and loading at the level of the game's own routines. The board model stays only as the reference.
7. **Training**, the reason for the project (below).
8. **The other systems, much later** (the owner, 2026-11-10): everything the CPS-2 collection runs on (Linux with
   Steam Deck and Steam Machine, Windows, Android), with its release scripts and self-updating launcher. Until then
   macOS only, but nothing may be written that only a Mac can run: SDL3 and SDL_GPU, no platform calls outside the
   port layer.
9. Readable rewrites where features need them; more games (Ibara, Pink Sweets, Muchi Muchi Pork!, Deathsmiles,
   DoDonPachi Dai-Fukkatsu and the Black Labels).

Akai Katana and DoDonPachi SaiDaiOuJou are in (the owner's decision, 2026-11-10) though MAME took both out in 2021
at their present rights holder's request, and they are sold today: whether they stay in a public release is a
question for that release's legal audit.

## Training and play features

The list starts from the shooting games of the CPS-2 collection (practice, attract mode, score attack, caravan, boss
rush, stage viewer) and goes further where these games ask for it.

- **Practice**: any stage, any point of it, straight at a boss or one of its patterns; the ship, shot type, lives,
  bombs, power, rank and the game's hidden counters chosen. Start over at once on one button (a state kept in
  memory). Replay of the last attempt with the controls shown.
- **Rewind**: step back through the last seconds and take over, for learning a pattern.
- **Hit boxes**: the ship's true hit box, enemy shots and bodies, drawn through the game's own draw list.
- **Readouts**: the scoring counters each game hides (Mushihimesama's counter, Espgaluda II's gems and gold and
  kakusei gauge, Futari's stage counter and multiplier), rank, the slowdown the board is giving right now, input
  display, frame counter.
- **Slowdown control**: as the board, none, or forced; and a slow-motion and frame-advance for study.
- **Score attack and caravan** with scores kept; **boss rush**; **stage viewer**; **attract mode**.
- **Free play**, the board's settings on a menu page, service menu on a button, cheats (invincible, bombs), skip of
  the boot check.
- **Display**: the picture stood up (rotated) or as the cabinet has it (tate, for a monitor on its side), integer
  scaling, the side space used for the readouts in a wide window.
- Online play and replays shared between players come after the above.

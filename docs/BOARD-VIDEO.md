# Videos of real boards against the port

The port's slowdown should be the board's, frame for frame. Videos of real boards are the only reference for
many of these games. This page says how a video is lined up against the port and what the first four showed.

## The method

The tool is `tools/boardvideo/boardvideo.py` (Python 3 with numpy, and ffmpeg).

1. **Run the port.** Start the headless runner from power-on with no input, so it plays the attract show. Set
   `SPELUNK_FRAME_DUMP=FILE` to save every frame as a small grey picture (port/frame_dump.h). Also save which
   frames were late: `SPELUNK_LATE_LOG` with `--timing` on the CV1000, `--frame-log` on the 68000 boards and the
   PGM (and `--timing` on the PGM).
2. **Register.** `boardvideo.py register VIDEO PORT.thm OUT.npz` matches each video frame to the port frame
   with the same picture (correlation, with the video scaled and shifted onto the port's picture; the fit is
   automatic).
   - Duplicated and dropped video frames do not matter: they just match the same or a later port frame.
   - 30 fps does not matter either: half the frames are enough.
   - A match is "sure" only when it stands out from the best match more than 3 frames away. Static screens
     (titles, rankings) are not sure, and are not counted.
3. **Count.** `boardvideo.py report OUT.npz` splits the sure matches into segments: runs in which the port's
   frame moves on steadily.
   - In each segment, board frames = R × (video seconds), port frames = Δn. The difference is how many more
     frames the board was late than the port.
   - The video's clock is not exact. R is fitted per video, on calm segments (where the port is never late).
     The residuals of that fit show the noise: about ±2 frames per segment.
4. **Check by eye.** `boardvideo.py sheet` puts video and port side by side at chosen times. Check that the
   demo has not diverged before counting (scores in the HUD, ship positions).

Example (Mushihimesama):

```
SPELUNK_FRAME_DUMP=port.thm SPELUNK_LATE_LOG=late.log build/mushisam/spelunk-headless --game games/mushisam/game.cfg \
    --roms ROMS --frames 21000 --cpu interpreter --timing
boardvideo.py register attract.mp4 port.thm reg.npz --late late.log --rate 60.0184
boardvideo.py report reg.npz --rate 60.0184
```

The videos are third-party. Keep them outside the repository, and never commit or publish them.

## The four attract videos (CAVE PCB channel, 30 fps)

All four are direct captures from boards, uploaded in 2011–2012 at 29.97 fps. The port ran from power-on with
the board's defaults: the ROM sets as shipped, a fresh EEPROM, no input.

| game | video | refresh used | video clock (fitted) | demos diverged? |
|---|---|---|---|---|
| Mushihimesama | RdBO-r4Apeg (2011), 300 s, starts mid-attract | 60.0184 | ×1.0011 | no (3 demos) |
| DoDonPachi | 8GJ-8Tr6QQA (2012), 358 s, from power-on | 57.44 | ×1.0076 | demo 3 parts at ~238 s |
| Dangun Feveron | L1KU51vX5SE (2012), 294 s | 57.44 | ×1.0080 | no (pictures match) |
| Dai-Ou-Jou | WCMazI4yEkg (2012), 273 s | 59.185 | ×1.0065 | no (pictures match) |

### Slowdown, board against port

"board − port" is how many more frames the board was late in that segment. Noise is about ±2.

**Mushihimesama.** The port is late 3 times in the whole video. The board agrees in every segment.

| video s | port late | board − port |
|---|---|---|
| 12.5–21.8 | 0 | −0.9 |
| 23.5–56.0 (demo) | 0 | −2.3 |
| 76.3–91.8 | 0 | +1.2 |
| 93.5–140.2 (demo) | 1 | −0.2 |
| 154.7–161.8 | 0 | −1.0 |
| 163.5–210.3 (demo) | 0 | +1.8 |
| 222.9–231.9 | 0 | −0.9 |
| 233.6–286.8 (demo) | 2 | −2.6 |
| 294.6–300.0 | 0 | −1.3 |

**DoDonPachi.** The port is never late in the attract show. The board agrees until the demo diverges.

| video s | port late | board − port |
|---|---|---|
| 60.8–104.1 (demo 1) | 0 | +1.8 |
| 134.4–172.5 (demo 2) | 0 | −0.4 |
| 211.0–239.6 (demo 3, up to where it parts) | 0 | −0.6 |
| 287.6–322.5 (demo 1 again) | 0 | −1.9 |

**Dangun Feveron.** The port is late 9 times. Each 10-second piece is within ±1.5 frames; total −0.0.

**Dai-Ou-Jou.** The demo is in the 147–191 s segment; the port is late 3 times in it.

| video s | port late | board − port |
|---|---|---|
| 17.1–59.7 (boot, title) | 66 | −1.1 |
| 82.1–121.8 | 3 | +1.1 |
| 147.5–191.3 (demo) | 3 | −3.7 |
| 210.1–252.8 (title) | 66 | −2.2 |

(The PGM's "late" frames on the title are frames with no new sprite list, not slowdown. They are the same on the
board.)

### What this shows

- **Where the port does not slow down, the board does not either**, to within ±2 frames per 10–60 s, on all four
  boards.
- **The attract demos barely slow down.** The port is late 0–9 frames in each whole video. So these videos cannot
  test heavy slowdown. That needs gameplay captures (below).

## Findings

### Mushihimesama: the board's title screen is ~19 s shorter

The attract cycle is 4,209 ± 2 frames on the board and 5,330 in the port: 1,122 frames more in the port. All of
it is in the title screen.
- The port's program counts the title down from a fixed 1,376 frames (0x560, read at 0C0A101C from 0C0A1084).
- 1,376 − 1,122 = 254, which is 0xFE, a constant stored right next to it (0C0A1082).
- The demos themselves are the same, picture for picture.

So the board in the video almost certainly runs **another program revision**, with a title of about 254 frames.
MAME knows two "2004/10/12 MASTER VER" Mushihimesama sets (mushisam and mushisama); only mushisam is here. This is
a revision difference, not a timing error. Worth checking if the other set can be had.

### DoDonPachi: the demo parts at ~238 s, as expected

- Demo 3 (two players) matches the port, scores included, up to about 238 s (frame ~13,900 from power-on). Then
  the board's player 1 keeps a chain going that the port's loses, and the board's demo runs about 9 s longer.
- DoDonPachi's random numbers come from its idle loop's count. The reset's phase changes that count, and on a
  board the phase is arbitrary (docs/SLOWDOWN.md, the shipped timing). So a demo that parts after some minutes
  is expected. It is not evidence of a timing error.
- Between the matched stretches, the board's gaps (each a demo's end, the ranking, the logos and the title) are
  50 and 156 frames shorter than the port's after demos 2 and 1. The ends of those demos are not sure matches, so
  whether they part too is not settled.

### Refresh rates: what a 30 fps video can and cannot say

- Each 2012 video's clock runs 0.65–0.80% fast (fitted). For Dai-Ou-Jou the refresh is known (59.185 Hz, logic
  analyser), so its ×1.0065 is the capture chain alone.
- DoDonPachi and Dangun Feveron came out ×1.0076 and ×1.0080. Feveron's board was probed at 57.44 Hz (nullobject).
  If both videos had the same clock, DoDonPachi's board would be 57.42 Hz. That agrees with the port's 57.44 (272
  lines), and is 0.2% below MAME's 57.55 (271.5 lines).
- But the clock differs by about 0.15% between videos of the same year (Dai-Ou-Jou against the other two). So this
  only weakly favours 57.44 over 57.55. A board measurement is still needed to settle it.

## Futari 1.5: a whole game on a board (60 fps)

buffi's Mushihimesama Futari 1.5 board, "Original 1CC (Abnormal Palm)", 22.6 minutes (YouTube 2B6s3fFMsv0, 2020;
BetsuBetsu, OSSC and Elgato HD60 Pro). It is the only 60 fps CV1000 video with a known chain. Original mode,
Palm, the Abnormal shot, free play; all five stages without a continue. The port ran the same: free play, Original,
Palm, Abnormal (the menu inputs: see "How it was run"), slowdown on, the ship out of harm, the shot held.

### The chain halves the frame rate

- The file is 60.000 fps, constant (81,304 frames). But only every other frame is new: 30 pictures a second, each
  shown twice. The port draws 60 a second in the same scenes. So the chain passes every second board refresh only
  (or a pair of them merged).
- The doubling changes phase in bursts every **57.26 s** (23 of 25 bursts on the beat, 0.28 s rms). That is the
  beat of the board against a 60.000 Hz capture: the board makes 60.0175 refreshes a second of video. buffi's logic
  analyser gives 60.0184 Hz, so the capture clock is 15 ppm off. There are no other duplicates: the chain does not
  resample.
- So single late frames cannot be seen in this video. A late frame shows only as a picture that is 1 game frame on
  from the one before instead of 2. A picture held 3 refreshes or more shows as a repeated picture.
- `repeats.py scan` / `runs` / `beat` do these checks.

### How the board's game frame was found

The port cannot play the same route, so whole pictures do not match (median correlation 0.26). The background does:
it scrolls with the game's frames, and only shifts across a little with the ship (up to 6 board pixels).
`follow.py` matches the board's pictures to the port's game frames on the closest 55% of the pixels, with that
shift searched, every 15th picture. Matches are kept only when they chain: never back, never faster than one game
frame a refresh. Each stage is chained on its own. That kept 913 of 2,603 pictures looked at.

Between two kept pictures, the board was late R·Δt − Δg frames (R = 60.0175, g the game frame). Where neither side
slows down, this stays within ±3 frames over minutes, so each step is good to about ±3. The port's late frames over
the same Δg are counted from its log.

### Scenes that do not depend on the player

Each boss's death runs the same scene: a flash, the explosion, then black (the last boss: a fade to white). The port
is late 92 frames in it at every stage end, the same in every route. Timed from the flash to the black on both
(board ±2 refreshes at each end):

| scene | port refreshes (late) | board refreshes | board − port |
|---|---|---|---|
| stage 1 boss dies | 302 (92) | 300.1 | −1.9 |
| stage 2 boss dies | 302 (92) | 298.1 | −3.9 |
| stage 3 boss dies | 300 (92) | 292.1 | −7.9 |
| stage 4 boss dies | 303 (92) | 302.1 | −0.9 |
| last boss dies (to white) | 276 (138) | 268.1 | −7.9 |
| stage 1 midboss dies | 82 late | 73.5 ± 1.5 late | −8.5 |

- The board is late a few frames **fewer** in every one: 4.5 on average of 92 (5%), 8.5 of 82 for the midboss
  (10%). Each one alone is within 2–3 times its error. All six on the same side is not likely by chance.
- ~~So the port is probably slightly too slow in big explosions, about 5–10%.~~ Not so (2026-11-22,
  docs/SLOWDOWN.md, "The blitter's interrupt"). These late frames are the game's own: it runs each of these scenes
  at half speed for a set number of game frames. Making the port's blitter 30% faster does not change the count at
  all. The game skips the extra refresh in a frame whose list took more than 14 ms. It times this with the blitter's
  IRQ1, which the port did not have until then. So the board's few frames fewer mean the board's lists went past
  14 ms a few times there. Its route had more on screen than the port's (on the port's route no list in these scenes
  takes 14 ms: 11.7 at most at the boss deaths, 13.5 at the midboss). The scenes are not player-independent in this
  respect.
- The midboss's explosion comes at another game frame on each side (it depends on when the midboss is killed). Its
  size does not: 82 in both port routes.
- The stage starts' own late frames (6 at game frame 125–150 of each stage, every route) were not resolved on the
  board.
- The stage 5 boss: the port has no long load stalls there. Its late frames come in the boss's patterns, as single
  frames. The board's boss fight is route-timed (its phases follow the damage), so it cannot be lined up frame by
  frame.

### Where the player decides

| stage | video seconds covered | board late | port late (same game frames) |
|---|---|---|---|
| 1 | 147 | 419 | 251 |
| 2 | 190 | 694 | 219 |
| 3 | 203 | 985 | 392 |
| 4 | 270 | 892 | 438 |
| 5 | 390 | 1,060 | 640 |
| all | 1,199 | 4,050 | 1,940 |

- Where the port is never late, the board mostly is not either: 39 of 54 well-covered 10-second stretches are
  within 10 frames.
- Overall the board is late about twice as often as the scripted port. The routes differ a lot:
  - the board's player plays close in and keeps a much higher counter (16,058 against the port's 2,131 in one
    scene), so there are more sprites;
  - the board's screen fills with gems in the busy scenes (Original mode's scoring, big enemies killed point
    blank with the laser); the port's scripts kill fewer of them that way;
  - three simple port routes (ship centred, ship left, ship high and point blank) are late 2,412, 2,035 and 2,335
    frames over the whole game: within 9% of each other. Per 600 game frames two of them differ by 25 frames rms,
    up to 115.
- Since IRQ1 (docs/SLOWDOWN.md), route 1 is late in 5,324 frames (5,573 before): the game drops its extra refresh
  where a list takes over 14 ms. That widens the gap to the board a little. A busier board route also drops more.
- So simple routes do not come near the board's count, and this video cannot say how much of the gap is the route
  and how much is the port. **The port may slow down too little in heavy play**; it is not shown. A board run of a
  known route would settle it. Confidence that a gap of 2x is route alone: low.

### How slowdown repeats

- **The port:** in play it is never late two frames running. All 3,446 late frames of a run are single frames
  (one picture held for two refreshes), loads and explosions included. Only the boot has longer stalls.
- **The board:** in play, no picture is held 3 refreshes or more (no repeated picture in 21 minutes). The repeated
  pictures come only at the stage ends' tallies and the ending, which are still screens: the port's same screens
  repeat about as much (56–65 board, 61–99 port per stage end, depending on the threshold).
- So the board does not make multi-frame stalls in play either. The 1-of-2 alternation itself cannot be seen at
  30 pictures a second.

### How it was run

- Port: `build/futari15/spelunk-headless --game games/futari15/game.cfg --roms ROMS --frames 84000 --cpu
  interpreter --timing --features free_play,out_of_harm=1 --system 2600:5:0x10 --press 2860:5:0x10 --press
  3000:8:0x08 --press 3080:5:0x10 --press 3240:5:0x02 --press 3320:5:0x10 --press 3400:80600:0x10`, with
  `SPELUNK_FRAME_DUMP=port.thm:2 SPELUNK_LATE_LOG=late.log`. With `--timing` the title comes about 400 frames later
  than without. Route 2 adds `--press 4200:30:0x14` (left), route 3 `--press 4200:70:0x01` (up).
- Board: `repeats.py scan board.mp4 scan.npz --crop 370,6,540,690`, `repeats.py beat scan.npz`,
  `follow.py thumbs board.mp4 scan.npz pics.npz --warp 369.5,9.5,2.245,2.125` (the chain squashes the picture
  vertically: 2.245 by 2.125 video pixels a board pixel), `follow.py track pics.npz port.thm late.log out.npz`
  (two processes with `--first/--last`: about 30 minutes each), `follow.py report out*.npz --max-score 0.25
  --min-sharp 0 --split 147,360,590,885`.

## A 60 fps chain: Futari Black Label (owlnonymous)

owlnonymous's "PCB- Mushihimesama Futari Black Label - 384,072,690 (1CC ALL)" (YouTube nyZ2Uk5DFEE, 2023, an
original PCB in a Taito Egret 2; 25 min, 60 fps). It is the first CV1000 video here whose chain passes 60 pictures
a second: `beat` finds no 1-of-2 doubling.

- **The repeat test misses its late frames.** (Corrected 2026-11-23, "Checking the counters" below: the repeats are
  2-18 grey levels, new pictures 35+; a threshold in that gap counts them all. `runs`' automatic threshold fell too
  low over the whole video, which holds still screens. Use `repeats.py held`.) `repeats.py runs` finds only 764 repeated frames in 90,360 (0.85%).
  But a picture the board shows twice is not exactly the same twice in this capture: the second copy changes a
  little (median block change 20–50 grey levels where a new picture is ~2× that). Count dips instead: a frame whose
  largest block change is under 0.6 of both neighbours'. Stretches with a dip every other frame are the board's
  half speed. There are 94 of them (25+ dips each).
- **Scripted scenes, board against port** (port: futaribl, Original, Palm, the route in "How it was run", with IRQ1):
  - stage 1 midboss dies: board 162 frames, 81 dips; port 165 refreshes, 82 late.
  - stage 1 boss dies: board 218 frames, 108 dips; port 220 refreshes, 109 late.
  - later boss deaths on the board: 217/109 (421 s), 219/109 (838 s); 226/111 and 224/112 run into the next
    scene.
  - So on this board the scripted explosions are as long as the port's, within the ±1 of where a stretch starts
    and ends. The board is not faster here. (Futari 1.5's 30 fps video had the board 1–8 refreshes shorter, each ±2
    at each end: above.)
- Not done: the board's heavy play against the port's (the routes differ; statistics only). The dip count is now
  `repeats.py dips` (2026-11-22).
- Samples (2:00–4:00) of three other 60 fps uploads were not clean: KidpandaAG's Futari BL PCB (GIBmn63FUm8, 30% of
  frames repeated: a 30 fps chain), and two Futari 1.5 Ultra runs of unknown source (Wes112 GPczGO22mis, 5.6%
  single repeats; Emm fZU9FItjwXk, 12%). With dips counted they might be usable; not looked at.
- The videos were deleted after the scan.

### Black Label in heavy play, lined up by background (2026-11-23)

The same video (owlnonymous, 60 distinct pictures a second), lined up with a port run by the background as Futari
1.5's was (`follow.py`; thumbs `--warp 520.5,2.5,2.2917,2.217`, the game's area of the 1280 x 720 stream; track every
15th picture; report `--rate 60.0184 --max-score 0.25 --min-sharp 0 --split 150,345,625,945 --every 10`). Port:
futaribl, Original, Palm, the menu inputs of "How it was run" above, out of harm, the shot held, 90,000 frames, with
the blitter's clock and line fetch of docs/SLOWDOWN.md. 1,755 sure anchors. Counted only where a 10 s stretch has at
least 12 anchors (and 1.2 a second): the boss fights and the stretches the chain crosses badly drop out.

| stage | seconds counted | board late | port late | board / port |
|---|---|---|---|---|
| 1 | 109 | 501 | 199 | 2.5 |
| 2 | 93 | 322 | 167 | 1.9 |
| 3 | 65 | 490 | 613 | 0.8 |
| 4 | 169 | 964 | 1,049 | 0.9 |
| 5 | 278 | 1,806 | 1,172 | 1.5 |
| all | 714 | 4,082 | 3,200 | 1.3 |

- Calm stretches agree (both 0-3 late per 10 s at 14-44 s, 74-94 s, 444-454 s, 1,043-1,051 s, 1,224-1,234 s), so
  the clock fits.
- The midboss and boss deaths of stage 1 stay 164 refreshes (82 late) and 219 (109 late) in this run, as before the
  blitter change.
- **Route caveat:** the player plays point-blank in Original mode with the screen full of gems (the contact sheet
  shows it from stage 1 on); the port's ship sits in the middle holding the shot. Per stretch the two are late in
  different places both ways (stage 3: port 613 against the board's 490; stage 1: board 2.5 times the port).
- (Corrected 2026-11-23, "Checking the counters" below: two of the counted stretches were not play, a stage-end
  tally and a stage start, where the anchors cannot tell the game frame. In play only, and counted by held frames, the
  board is late 1.17 times as often as the port, not 1.3.)
- So the board is late about 1.3 times as often as the port overall in Black Label, against about 2 times in buffi's
  30 fps Futari 1.5 video; with stages where the port is the later one, this does not show a cost missing from the
  port. A known route on a board is still the only decisive test.
- Small results kept in spelunkomp-data/video/futaribl_owl (tr_a/b.npz, rep10.md, the port's late log); the video
  was deleted.

### Black Label: the player's route replayed (stage 1, 2026-11-23)

The same video's stage 1, with the port flying owlnonymous's route instead of a centred ship. `route.py track` finds
the ship in every frame (Reco: two pictures cut from the video, normalised correlation; found in 90-99% of the frames
per 10 s, 57-70% in two busy stretches, the gaps filled between). `route.py make` turns it into game frames by the
earlier run's anchors and writes a route for `SPELUNK_ROUTE` (port/headless.cpp): each game frame, the ship's place
poked (0C383E98/0C383E9C, 16.16) and the buttons. Out of harm as before. The laser was told by speed: the ship flies
1.5 board pixels a frame with the laser (button 1 held) and 3 with the shot (button 3, full auto), and the player's
speeds come in those two bunches. Standing still was tried three ways (keep, laser, shot).

How close it got:

- The ship goes where the player's went; side by side the screens hold the same enemies, explosions and bullets.
- The counter (0C383EF4) against the board's, read off the video at 12 points: within 1-3% up to the midboss on the
  best variant (3858 / 3848 at game frame 4800, 4418 / 4453 at 5200); 5-20% low after it (8323 / 7902 at 8000). The
  score is 16% low at 100 s. The port kills the boss later than the player.
- The midboss's death is 81-82 late frames on both, as before.

Late frames, stage 1 to the boss fight (game frames as the anchors; the board counted two ways: R·Δt − Δg between
anchors, and the dips `repeats.py dips` finds; the two differ by up to 30 in a stretch, and the anchors fail in the
boss fight, where the dips find 4 late frames and the anchors 193):

(The dips column overcounts a little; held frames, the corrected count, are 0, 0, 16, 2, 78, 81, 0, 0, 52, 26 and 3
 per row: 255 to the boss fight, 258 with it. "Checking the counters" below.)

| video s | board (dips) | board (anchors) | port, centred ship (before) | port, player's route (5 variants) |
|---|---|---|---|---|
| 13.9-53.7 | 39 | 36 | 1 | 0-2 |
| 53.7-61.7 | 78 | 40 | 67 | 4-65 |
| 61.7-73.5 (midboss dies) | 81 | 82 | 82 | 82 (one variant kills it later) |
| 73.5-93.8 | 2 | -1 | 0 | 0 |
| 93.8-113.1 | 81 | 57 | 47 | 0-16 |
| 13.9-123.1 | 285 | 216 (boss fight left out) | 197 | 72-157 |

- **The player's route does not make the port slower.** It is late less than with the centred ship. So stage 1's gap
  is not the route as the earlier comparison guessed. Nor is it shown to be the port: the replay's counter and score
  are lower after the midboss, and on the board the gems are bigger and denser.
- **53.7-61.7 s:** the same scene runs at half speed on both, for game frames ~5335-5470 on the board, ~5390-5455 on
  the port (late in a frame whose list takes more than ~14 ms: blitter time plus the CPU's work after it). With the
  blitter's time ×1.05 the port is late 74 there (×1.10: 98).
- **95-104 s** (two bursts, 1 in 2 and 1 in 3, on the board): the port is never near late. The blitter ends at 81% of
  a refresh at most (99th percentile), and the CPU's own work by 41%. ×1.10 changes nothing (3). The game's own busy-
  play slowdown (Black Label's frame wait, 0C06CCB0: the limits at 0C384884) is off all through stage 1 in the port.
  Only the midboss turns the wait word (0C1DE148) to 2. So the board's late frames there are real overruns: its frames
  took about 25% longer than the port's, on screens that look the same apart from the gems.
- No single scale fits: ×1.05 matches 55 s and does nothing at 95-104 s. A blitter scale is ruled out anyway:
  cv1kbench's blitter rows are within 0.4% of the PCB, and Mushihimesama's stage 3 goes from ~350 to 545 with ×1.03.
- What is left: a cost that is large in Futari's gem-heavy explosions and small in the benchmark and Mushihimesama, or
  a screen the replay does not reproduce (the bigger gems of a higher counter). About 80 late frames in stage 1's
  95-113 s. Found by the per-list make-up (`SPELUNK_BLIT_LOG`) of a better-matched replay, if one can be made.
  Not done: stage 5 (a stage's route needs the stages before it replayed too: the counter carries over).
- **The counter is not it** (checked 2026-11-23). The board's counter read off the video was poked into the replay
  (0C383EF4 and 0C383EF8: 4418 at game frame 5200, 8094 at 7600), so the gems after those frames come from the board's
  counter. The port's counter then follows the board's (8115 at 7734). Late frames, laser / shot variants, before ->
  after: 53.7-61.7 s 56 -> 56 and 58 -> 58 (board 78 dips, 40 anchors); 93.8-113.1 s 0 -> 3 and 12 -> 12 (board 81
  dips, 57 anchors). So the gap left in stage 1 is about 20 late frames at 55 s and about 70 at 95-113 s. It cannot
  be settled from this video: it needs a board run of a known route (an input replay on a board).
- Small results: spelunkomp-data/video/futaribl_route (the track, the routes, the port's late logs and counters,
  stage1.md); the video was deleted.

## Checking the counters, the board's version, and a second game (2026-11-23)

The Black Label gap (board 1.3x the port; ~90 late frames in stage 1 unexplained) was counted two ways that
disagreed (stage 1's boss fight: 193 by anchors, 4 by dips). Checked before any model change.

### The late-frame counters, tested where the answer is known

The port's own pictures were made into a video the way owlnonymous's was made, and counted with the port's late log
as the truth:

- Pictures: futaribl, every refresh as a PNG (`--shot`), two runs: stage 1 on the player's route (`SPELUNK_ROUTE`, the
  counter poked; 7,100 refreshes, 166 late, mostly light) and the centred-ship run in stages 3-4 (refreshes
  29,000-36,200; 2,163 late, 30%, heavy). In play a picture equal to the one before is exactly a late frame.
- Video: the game's picture scaled to 550 x 709 and laid over owlnonymous's own stream frames (webcam, chat) at the same
  place, encoded as a stream (x264 CBR 6 Mbit/s), then as YouTube encoded his (H.264 High, 720p60, GOP 304, up to 6
  B frames, 2-pass): at his 3.8 Mbit/s, and at 1.5 and 0.8 to stress it.
- Counted with `repeats.py scan` on the game's area, then held frames (block change under the gap, the new `held`),
  dips (`dips`), and anchors (`follow.py` against a different port run, as the board was).

| test | truth | held frames | dips | anchors |
|---|---|---|---|---|
| stage 1, 3.8 Mbit/s, in play | 142 | 142, none false | 142 + 11 false (1-4 per 10 s) | 166 total = truth; per 10 s within 2 |
| stage 1, 1.5 / 0.8 Mbit/s | 142 | 142 / 142 | + 17-26 false | |
| heavy, 3.8 Mbit/s | 2,163 | 2,163, none false | 2,151: 12 missed (two late in a row), 11 false | |
| heavy, 1.5 Mbit/s | 2,163 | 2,155 | 2,152 | |

- Held frames are exact in light and heavy play alike. Repeats come out at a block change of 3 (median, max 9-15)
  at 3.8 Mbit/s, 9 (max 25) at 0.8; new pictures 35-200. owlnonymous's real repeats are 2-18, a little noisier than
  the 3.8 Mbit/s test (two generations), but the gap (20-35) is empty in play.
- Dips are good to a few percent: they add 1-4 frames per 10 s where the change alternates big/small, and miss a late
  frame next to another. Not the cause of a 1.3x gap.
- Anchors are exact where the two runs scroll together; they fail where the background does not decide the game
  frame: boss fights (stage 1's 193 against held 3), stage-end tallies and stage starts (still screens; the tally's
  length depends on what the player collected).
- Neither held frames nor dips work on still screens (tally, menus, fades): there the picture barely changes every
  frame. Count play only (the gap clean: under 2% of frames at 12-35).

### Black Label re-counted

All of owlnonymous's video scanned again (in sections), the board's late frames counted as held frames, the port's
late frames over the same game frames as before (the earlier run's anchors; data: spelunkomp-data/video/gapcheck_owl,
restage.md):

| stage | s | board, anchors (old) | board, held | port | held / port |
|---|---|---|---|---|---|
| 1 | 99 | 216 | 255 | 197 | 1.29 |
| 2 | 93 | 322 | 277 | 167 | 1.66 |
| 3 | 59 | 410 | 319 | 613 | 0.52 |
| 4 | 169 | 964 | 1,036 | 1,049 | 0.99 |
| 5 | 258 | 1,762 | 1,812 | 1,139 | 1.59 |
| all | 679 | 3,674 | 3,699 | 3,165 | 1.17 |

(Play stretches with 12+ anchors. The old table's 4,082 / 3,200 included the stage 1 tally (285 by anchors, port 2)
and stage 3's start (80, port 0). Over all clean play stretches, whatever their anchors: board 5,743, port 5,609.)

- In well-anchored play, held frames and anchors agree (stage 5's 1083-1114 s: 282 / 282, 230 / 228, 210 / 210).
- **Corrected gap: 1.17x, about 530 late frames in 679 s, not 1.3x.** It swings both ways by stage (stage 3: the
  port later by 294; stage 5: the board by 673): route.
- Split by pattern: in long strict 1-in-2 runs (the game's frame wait of 2, or a steady overrun) board 2,606, port
  2,537: equal. In the other, irregular late frames (single overruns) board 1,093, port 628: **the board leads in
  single overruns, 1.7x.** That is where a missing cost would show, and where a busier route would too.
- Stage 1 against the replayed route: board 255 held to the boss fight against the port's 72-157 (5 variants);
  without the scripted midboss (81 on both): board ~175, port 0-75. That gap (~100 frames, at 33-44 s, 54-62 s,
  94-113 s) stands with the corrected count.

### The board's version: 2007 Black Label, the port 2009 "Another Ver"

- The port's ROM set (MAME futaribl) is the 2009/11/27 INTERNATIONAL BL program, which prints "Another Ver" under the
  logo and a rank line on the title screen. owlnonymous's board, on his 2018 stream of the same cabinet
  (UlKA6KR6ZK8, the title at 26-38 s), shows neither: it is the 2007 Black Label (MAME futariblj, 2007/12/11 BLACK
  LABEL VER). The 2023 video skips the title; same owner and cabinet, so most likely the same board. Pictures:
  spelunkomp-data/video/gapcheck_ver.
- The two share the NAND (graphics); only the program (u4) differs. futariblj's u4 (CRC b9467b6d) is not in
  ../cv1k-roms, so what changed in its timing code (the 14 ms rule, the frame waits, the busy-play limits at 0C384884)
  cannot be compared here. Get that program ROM to settle it.
- Futari 1.5 (buffi): the video's title says Ver 1.5; MAME's futari15 and futari15a are both 1.54 of 2006/12/8 (other
  dumps), so the same program as far as known. Mushihimesama's mushisama was found before.

### A second game: DoDonPachi Dai-Fukkatsu 1.5 (buffi, 1PGxgqtlD4U)

- Version: DFK 1.5 on both (the port's ddpdfk is 2008/06/23 Ver 1.5). Chain: 60 pictures a second with a 1-in-6
  band for some seconds every ~55 s (as buffi's other USB3HDCAP chains); held frames otherwise clean (median 4, new
  80-96). The band's repeats were subtracted (gap-6 repeats).
- **Stage 1 boss's death (scripted):** board 377 refreshes / 188 late, port 364 / 182, same pattern (a 1-in-2 run, a
  short break, a second run of exactly 89 on both, ending x...x). The board's start falls in a band, so ±10. Matches.
- **Stage 1, start to the boss's death:** board ~703 late in 8,385 refreshes (914 held less 211 band), port 623 in
  8,122 (the generic start, type A, out of harm, shot held, centred): 1.13x, as Black Label's 1.17.
- Stages 2-3: board ~4,140 against the port's ~1,200 over the same time, but the player builds hit counts of 500-650
  with hypers (bullets cancelled to items, the screen full) and the port's route never fires a hyper: not
  comparable. Background anchors failed in DFK (smoke and transparencies; 105 and 91 sure anchors in 300 s), so no
  stretch-by-stretch lining up. Data: spelunkomp-data/video/gapcheck_dfk.

### Verdict

- The 1.3x was partly the method: the stage-end tally and a stage start counted as late frames. Corrected, the board
  is late 1.17x as often as the port in Black Label's well-anchored play, and 1.13x in DFK's stage 1, both on routes
  with more on screen than the port's. Scripted scenes match on both games within a few frames.
- What is left is single overruns in busy play (board 1.7x the port's in Black Label), the ~100 frames of Black
  Label's replayed stage 1, and Black Label's board running another program (2007) than the port's (2009).
- **Not shown to be a model shortfall.** A model change would need, in order: the 2007 program (to replay stage 1 on
  the board's own code), then a board run of a known route. The counters are now trustworthy for that (`held`).

## 60 fps board videos found (2026-11-22)

Each candidate was tested on a 90 s sample (`repeats.py runs`, `beat`, `dips`; the gaps between repeated frames). On a
clean 60-picture chain the repeats come only where the board slows down: none in calm play, single frames, and runs
of a repeat every other frame in the game's half-speed scenes. A chain artifact shows as a period (a repeat every N
frames, or a band that comes and goes with the board/capture beat).

| video | uploader, year | game | verdict |
|---|---|---|---|
| Lgh-AqVLRQQ | KidpandaAG, 2019 | Mushihimesama, PCB 1CC | **clean 60**: no periodic repeats over 22 min; calm play 0-10 repeats per 30 s |
| nFBSb8hq6Jc | Aaron Kraten, 2022 (from Twitch) | Mushihimesama, Original 1CC | 60, with a doubled frame every 251 frames (39 in 23 min) and a few dropped ones: usable with care |
| LoZQXAD2t94 | Kiwi, 2022 | Mushihimesama Ultra | 60 in the sample (repeats as half speed); not looked at further |
| b6jWBt4louk | Viku, 2025 | Mushihimesama (vertical 720x1280) | 60 in the sample (repeats only as dips, 2.6%); not looked at further |
| GeYpn7UWQxA | Yong's Retro Game Capture, 2022 | Futari (OSSC test) | 59.94 capture (drops a board frame every ~13 s); repeats as half speed; not looked at further |
| ETUJqrV1XME | buffi, 2021 | Pink Sweets, Kasumi B | 60 pictures, but for about half of every 58.06 s the chain repeats one frame in six (a phase band of board against capture: board 60.0172 a video second). Repeats are not slowdown there; R·Δt − Δg still is |
| YGGzdD_Rj1A | buffi, 2020 | Futari Black Label | the same 1-in-6 band (gap 6 most common after 2) |
| nbwW5kQ-2IM | buffi, 2020 (BetsuBetsu, OSSC, USB3HDCAP) | Futari 1.0 | the same 1-in-6 band |
| a1VO8foAhfU | buffi, 2020 (OSSC, Elgato) | Espgaluda II | 30 pictures a second (each doubled; beat 53.4 s): no good |
| dVxBJVxWE-8 | neoxaquas, 2025 (from WaiWai's Twitch) | Pink Sweets | 23% repeats in the sample, mostly half speed (heavy play?); chain not settled |
| My4YUKK4WGo | ytriDlO, 2015 | Akai Katana attract | 50 fps upload: no good |
| 85UyDByXEbk, eHQOX9PsMuI, VhThvTZsCpE, 5avpksNGTqU | ytriDlO, 2015-16 | Pink Sweets, Mushihimesama, Espgaluda II, Ibara attracts | 30 fps uploads: no good |
| FdovvKaH4kY, tMDpXDSeld4, kvMySZMfHM4, 1el3953lghY | various | Pink Sweets, Espgaluda II, Akai Katana | 25-30 fps uploads |
| five superplay mirrors of Nicovideo uploads | various | Futari, Black Label, Ketsui, DFK Black Label, DFK 1.5 | 24-30 fps |

Not tested yet (60 fps uploads): EU5EaJByock (Kiwi, Muchi Muchi Pork), Ag0zJWpwUMw (trap0xf, Akai Katana),
1PGxgqtlD4U (buffi, DFK 1.5, XRGB-mini + USB3HDCAP), 4FpjtD0YbbE (buffi, Deathsmiles), NORT6TJZ2Ck and xvNvVEqdghE
(buffi, Ibara), Vur9s89b9Mg (buffi, Akai Katana). YouTube refused downloads from this Mac for about an hour
("confirm you're not a bot"); space ran short after that.

### Mushihimesama: boss deaths are scripted, and match

On both Mushihimesama boards every boss's death is a run of 126 refreshes with a repeat every other frame (63 late),
the same at all five stage ends (KidpandaAG: 135.5, 321.5, 595.1, 869.0, 1195.6 s; Aaron Kraten's the same, with
its chain's blips). The port (mushisam, `--features out_of_harm=1,to_boss=1`, the shot held) is late in the same
pattern: stage 1 boss frames 12,130-12,255, stage 2 boss 16,237-16,362: 126 refreshes, 63 late. After the explosion
the stage-end counting shows a repeat every 7 frames on the board and on the port alike. Both boards also show the
same 380-refresh (190 late) half-speed scene after the last boss, and a 284-286-refresh one in stage 5.

### Mushihimesama's stage 3: the port slowed down three times too much (fixed)

The stage after the stage 2 boss was cut from both Mushihimesama videos and lined up with a port run of stage 3
(`--features out_of_harm=1,start_stage=2`, the shot held, frame dump `:2`) by the background (`follow.py thumbs`,
`track --anchor-every 10`, `report --rate 60.0184 --every 20`). 724 and 926 sure anchors. Where neither side slows
down the board's late count stays within ±3 over 20 s, so the clock fits.

| game frames (KidpandaAG's anchors) | KidpandaAG late | port late, signed edge (#56) | port late, MAME's edge (now) |
|---|---|---|---|
| 4,524-6,918 | 14 | 9 | 4 |
| 6,918-9,300 | 17 | 355 | 139 |
| 9,300-12,582 | 226 | 553 | 124 |
| 12,582-15,773 | 84 | 228 | 26 |
| 15,773-18,407 | 16 | 83 | 10 |
| stage 3 in all | 356 | 1,228 | 303 |

| game frames (Aaron Kraten's anchors) | Aaron Kraten late | port, signed | port, MAME's |
|---|---|---|---|
| 5,051-7,443 | 1 | 7 | 2 |
| 7,443-9,803 | 50 | 355 | 139 |
| 9,803-13,134 | 270 | 679 | 127 |
| 13,134-16,676 | 62 | 185 | 33 |
| 16,676-17,846 | 0 | 0 | 0 |
| stage 3 in all | 384 | 1,226 | 301 |

- With the draws over the edge timed by what they paint (#56), the port was late 3.4 and 3.2 times as often as the
  two boards, in the same stretches, on a route with less on screen than either player's. With MAME's timing it is
  a little under both. The change is in docs/SLOWDOWN.md ("Draws over the edge"). Two independent boards agree, so the
  port went back.
- Per stretch the boards still differ from each other and from the port by tens of frames: the routes (where enemies
  die, what is on screen). Only the totals and the size of the gap are evidence.

### Mushihimesama's stage 3 again: the route, not a missing cost (2026-11-23)

After #72 the port was late 303 frames over the stage against the boards' 356 and 384. The same stretches, with two
more port routes (the ship moved left, or up, early in the stage; `--press 6000:40:0x04`, `--press 6000:70:0x01`)
and the blitter's clock and line fetch as the board has them (docs/SLOWDOWN.md; "new" below). Game frames as
KidpandaAG's anchors (Aaron Kraten's differ by a few frames at each end):

| game frames | KidpandaAG | Aaron Kraten | port centre | left | high | centre, new | left, new | high, new |
|---|---|---|---|---|---|---|---|---|
| 4,406-7,000 | 14 | 2 | 4 | 4 | 4 | 4 | 4 | 4 |
| 7,090-8,587 | 3 | 13 | 106 | 184 | 42 | 109 | 188 | 44 |
| 8,587-9,789 | 16 | 33 | 33 | 0 | 15 | 37 | 0 | 15 |
| 9,789-10,197 | 41 | 67 | 0 | 0 | 0 | 0 | 0 | 40 |
| 10,197-11,297 | 96 | 33 | 68 | 110 | 10 | 211 | 84 | 6 |
| 11,297-12,181 | 78 | 119 | 56 | 4 | 35 | 40 | 5 | 40 |
| 12,181-12,776 | 17 | 50 | 0 | 12 | 14 | 0 | 11 | 8 |
| 12,776-13,990 | 60 | 29 | 26 | 58 | 28 | 108 | 41 | 41 |
| 13,990-14,593 | 15 | 23 | 0 | 0 | 0 | 0 | 0 | 0 |
| 14,593-18,707 | 16 | 12 | 10 | 0 | 26 | 9 | 0 | 6 |
| stage 3 | 358 | 384 | 303 | 372 | 174 | 518 | 333 | 204 |

- The port's own routes span 174-372 with one model; both boards fall inside. Where the boards and the port are late
  differs by route in both directions (7,090-8,587: the port's ship leaves a wave of large insects alive that both
  players kill; 9,789-10,197: both players are late, no port route but one is).
- So these videos do not show a cost missing from the port. What a busy frame spends, and why the 14 ms rule, the
  draws over the edge, bus sharing and the CPU's memory costs are ruled out: docs/SLOWDOWN.md ("Mushihimesama's
  stage 3: where a busy frame's time goes").
- With the blitter change the three routes' mean goes from 283 to 352; the boss deaths stay 63 of 126 (stages 1
  and 2).

### Pink Sweets (buffi): the chain's band, and a port that slows down less

- buffi's Pink Sweets 1CC is 60 pictures a second, but for about half of every 58.06 s a picture in six is repeated
  (and one dropped): the phase of board against capture. Outside the band the board is late about 37 frames per 30 s
  in play (2%); the port's run of the same stages (Shasta, the menu's first choices, out of harm, the shot held) 1,023
  in 63,003 refreshes (1.6%, the boot's 750 included), so about 0.4% in play. The board slows down a lot more, but
  on a route with far more on screen (infinite lives, then point-blank scoring).
- The half-speed stretches on the board (21 of 20+ repeats) are bullet-heavy boss patterns (a spiral at 802 s, 96 late
  in 192), not scripted explosions: Pink Sweets has no fixed-length death scene like Mushihimesama's or Futari's.
- `follow.py` could not line it up: the port's background matches were poor (scores 0.16-0.24) and wandered between
  stages, probably the route (the port's Shasta and the board's Kasumi differ in what the shot covers). Not settled.
  Pink Sweets does not read IRQ1's blitter time (docs/SLOWDOWN.md), so that change does not touch it.

### The IRQ1 change against the boards

- Mushihimesama's boss deaths are the frame wait's half speed for a set count, which IRQ1's 14 ms rule can shorten.
  On both boards and on the port all five (two in the port) are 63 of 63: no list there takes 14 ms on either side.
- Owlnonymous's Futari Black Label (above) agrees within 1 on the two scripted deaths checked.
- Heavy play with a long list (where the rule drops refreshes) was not lined up on a board for Mushihimesama or
  Espgaluda II (Black Label: above, 2026-11-23); Mushihimesama's stage 3 above is mostly the over-the-edge draws, not the 14 ms rule.

## What is left

- **A board run of a known route** for Futari 1.5 (or any CV1000 game): the attract demo at 60 fps, or a TAS-like
  input replay on a board. Only that tests heavy play frame for frame.
- **Clean 60 fps CV1000 captures**: found for Mushihimesama (KidpandaAG, Aaron Kraten) and used above. Next, the
  same stage-by-stage lining up for Futari 1.5's stages 1-2 and Espgaluda II (the other games #56 had moved most),
  and the untested 60 fps uploads in the list above (Muchi Muchi Pork, Akai Katana, DFK, Deathsmiles, Ibara).
- **Explosions**: settled as far as the port goes. Futari's are scripted. The Black Label board video matches the
  port within ±1 frame; the 1.5 video's few frames fewer can come from the IRQ1 rule (above) on a busier route. To see
  the rule on a board: a 60 fps video of a busy boss death, with a game frame of one refresh in the middle of its
  1-of-2 pattern.
- **The remaining CAVE PCB attracts** (section C1 of the evidence list), one per game, the same way. Their demos
  will also barely slow down. Use them for demo divergence (settings, revisions) and attract cycle lengths.
- **The other 60 fps CV1000 long-plays** (MDBBSRemnants, Kiwi, PEG). These show real heavy slowdown. The method:
  - First check the chain (`repeats.py runs` and `beat`): Futari 1.5's above passes only 30 pictures a second.
  - Count the board's repeated frames there. On CV1000 into 60 Hz they are real slowdown, not capture artifacts.
  - Compare their run lengths (1-2-1-2 alternation, longer stalls) with the port's late-frame log in comparable
    scenes.
  - The gameplay cannot be replayed, so this compares statistics, not frames.
- **A board's refresh rate measured**, for the Cave 68000 boards (57.44 or 57.55 Hz). The video chains above are
  too loose for it.

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Finds the code a CV1000 game runs that its translation lacks, and translates it. The game is run on its translated
# code in several ways side by side, each run recording the blocks the interpreter had to take (and STATIC steps of
# what they lead to) in work/<game>/blocks-explore-<run>.txt:
#   attract    no one at the board: the demos, the rankings, the title, over and over
#   test       the board's test button (S3) at boot, then made-up presses through the test menus
#   coins      made-up play on coins (not free play): the credit screens, the continues, game over, name entry
#   lose-N     made-up play on free play, nothing held: ships lost, continues, game over, name entry
#   two        made-up play with both players in
#   play[-b,-c] made-up play out of harm from stage 1 on (where the game has the facts), each its own way through the
#              select screens (the modes, the ships)
#   stage-S    a game begun at each stage (the training facts' start at stage), out of harm and with bombs held, then
#              made-up play on through the stages after it, the bosses, the ending (where the game has these facts)
# Then the recordings are translated (tools/translate.sh's step, FOUND=1 with STATIC) and the game built, and the
# rounds go on until one finds nothing new (or ROUNDS are done).
#   tools/explore.sh ROMS [FRAMES [ROUNDS]]   (GAME=mushisam, B=build/$GAME, FRAMES 30000, ROUNDS 3, JOBS=4, STATIC=4;
#   AGAIN=N: other made-up play than the first N rounds had, for a later call)
# MEASURE=1 only measures: the share of blocks interpreted over a run of made-up play out of harm (seed 99, FRAMES
# long, through the stages) and over the check's run (tools/check.sh's input, 6000 frames).
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/explore.sh ROMS [FRAMES [ROUNDS]]}
FRAMES=${2:-30000}
ROUNDS=${3:-3}
GAME=${GAME:-mushisam}
B=${B:-build/$GAME}
JOBS=${JOBS:-4}
STATIC=${STATIC:-4}
W=work/$GAME
mkdir -p $W
H=./$B/spelunk-headless
build() {
    cmake -S . -B $B -G Ninja -DGAME=$GAME -DCMAKE_BUILD_TYPE=Release >/dev/null
    ninja -C $B spelunk-headless >/dev/null
}
build
features=$($H --game games/$GAME/game.cfg --features list)
has() { echo "$features" | grep -q "^$1 "; }
# (what keeps the ship in play: out of harm and bombs, where the game has them)
keep=free_play
has out_of_harm && keep=$keep,out_of_harm
has endless_bombs && keep=$keep,endless_bombs

measure() {
    $H --game games/$GAME/game.cfg --roms "$ROMS" --frames $FRAMES --features $keep --auto 99 2>&1 | tail -1 | sed "s/^/  long run ($keep, made-up play): /"
    case $GAME in
    mushisam) PLAY="--system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4700:1200:0x11" ;;
    *) PLAY="--system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10 --press 4900:5:0x10 --press 5000:5:0x10 --press 5200:800:0x11" ;;
    esac
    $H --game games/$GAME/game.cfg --roms "$ROMS" --frames 6000 $PLAY 2>&1 | tail -1 | sed 's/^/  the check'"'"'s run: /'
}
if [ -n "$MEASURE" ]; then
    echo "$GAME:"
    measure
    exit 0
fi

# Each run: a name and its options
runs() {
    echo "attract"
    echo "test --system 30:600:0x100 --auto 11:700"
    echo "coins --auto $(((round + ${AGAIN:-0}) * 10 + 1))"
    echo "lose-1 --features free_play --auto $(((round + ${AGAIN:-0}) * 10 + 2))"
    echo "lose-2 --features free_play --auto $(((round + ${AGAIN:-0}) * 10 + 3)):2000"
    echo "two --features free_play --auto $(((round + ${AGAIN:-0}) * 10 + 4)):400:2"
    echo "play --features $keep --auto $(((round + ${AGAIN:-0}) * 10 + 5))"
    # (more made-up play: each picks its own way through the select screens, the modes and ships)
    echo "play-b --features $keep --auto $(((round + ${AGAIN:-0}) * 10 + 1000))"
    echo "play-c --features $keep --auto $(((round + ${AGAIN:-0}) * 10 + 2000))"
    if has start_stage; then
        n=$(echo "$features" | awk '$1 == "start_stage" { print $2 }')
        s=1
        while [ $s -lt "$n" ]; do
            echo "stage-$s --features $keep,start_stage=$s --auto $(((round + ${AGAIN:-0}) * 10 + 5 + s))"
            s=$((s + 1))
        done
    fi
}

count() { cat $W/blocks-*.txt 2>/dev/null | awk 'NF >= 2' | sort -u | wc -l | tr -d ' '; }
round=0
while [ $round -lt "$ROUNDS" ]; do
    before=$(count)
    runs | xargs -P "$JOBS" -L 1 sh -c 'name=$0; '"$H"' --game games/'"$GAME"'/game.cfg --roms "'"$ROMS"'" --frames '"$FRAMES"' \
        --record '"$W"'/blocks-explore-$name-new.txt --record-static '"$STATIC"' "$@" 2>&1 | tail -1 | sed "s/^/  $name: /"'
    for f in $W/blocks-explore-*-new.txt; do
        [ -f "$f" ] || continue
        python3 tools/merge_blocks.py "${f%-new.txt}.txt" "${f%-new.txt}.txt" "$f"
        rm -f "$f"
    done
    after=$(count)
    round=$((round + 1))
    echo "round $round: $before -> $after blocks recorded"
    python3 tools/translate.py --found $W/gen $W/blocks-*.txt
    build
    [ "$after" != "$before" ] || break
done
echo "$GAME:"
measure

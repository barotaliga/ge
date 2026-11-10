#!/usr/bin/env bash
# A 4on4 of the bots played on our own server and rendered as a video to watch: `se-team` plays
# and records the match (an MVD), then the game renders the recording with the camera on the
# hottest player (`-demovideo ... --autotrack`) as an .mkv, by default in ~/Downloads.
#
#   tools/bot-match-video.sh [--map=e1m2] [--seed=1] [--minutes=10] [--features=<list>]
#                            [--features-red=<list>] [--features-blue=<list>] [--out=<folder>]
#                            [--no-build] [--no-video]
#
# The game folder comes from `.bots/local.md` (`basedir:`, default .bots/game; the bots skill §
# Shooter's Edge's bot plan), and the recording is kept under its `work:` (default .bots/work) in
# videos/. The video is named bots_<commit>_<map>_s<seed>[_<features>].mkv, so a checkpoint's
# video says which build played it. --features-red against --features-blue shows a head to head.
# The bots' decision log is written beside the recording (<name>.why.tsv): `make bot-why
# ARGS="--video=<name> --bot=red2 --at=4:12"` answers why a bot did something at a moment.
# It builds the release preset's `labs` and `shooters_edge` first unless --no-build. Rendering
# needs a GPU adapter (the Mac's, or the container's software one, much slower); --no-video keeps
# the recording (the demo) alone, to play in the window with `-playdemo <mvd>`.
#
# ge:summary Plays a 4on4 of the bots on our server, records it and renders it as an .mkv to watch
# ge:keywords bots 4on4 video mkv demovideo autotrack se-team record watch match checkpoint
# ge:skill bots
set -euo pipefail
cd "$(dirname "$0")/.."

map=e1m2 seed=1 minutes=10 out="$HOME/Downloads" build=1 video=1
features="" features_red="" features_blue=""
for a in "$@"; do
  case $a in
    --map=*) map=${a#--map=} ;;
    --seed=*) seed=${a#--seed=} ;;
    --minutes=*) minutes=${a#--minutes=} ;;
    --features=*) features=${a#--features=} ;;
    --features-red=*) features_red=${a#--features-red=} ;;
    --features-blue=*) features_blue=${a#--features-blue=} ;;
    --out=*) out=${a#--out=} ;;
    --no-build) build=0 ;;
    --no-video) video=0 ;;
    -h | --help)
      sed -n '2,17p' "$0"
      exit 0
      ;;
    *)
      echo "bot-match-video: unknown flag $a; next: tools/bot-match-video.sh --help" >&2
      exit 2
      ;;
  esac
done

# A key of .bots/local.md ("basedir: <path>"), else the default.
local_key() {
  local v=""
  [[ -f .bots/local.md ]] && v=$(sed -n "s/^$1:[[:space:]]*//p" .bots/local.md | head -1)
  echo "${v:-$2}"
}
basedir=$(local_key basedir .bots/game)
work=$(local_key work .bots/work)
if [[ ! -d $basedir ]]; then
  echo "bot-match-video: no game folder at $basedir; next: put the packs there or set" \
    "'basedir: <folder>' in .bots/local.md (the bots skill § Shooter's Edge's bot plan)" >&2
  exit 1
fi

if ((build)); then
  [[ -f build/release/CMakeCache.txt ]] || cmake --preset release
  cmake --build build/release --target labs shooters_edge
fi
labs=build/release/labs
game=build/release/games/shooters_edge/shooters_edge
for exe in "$labs" "$game"; do
  if [[ ! -x $exe ]]; then
    echo "bot-match-video: no $exe; next: run without --no-build" >&2
    exit 1
  fi
done

sha=$(git rev-parse --short=8 HEAD)
tag=""
clean() { echo "$1" | tr -c 'a-zA-Z0-9+-' '_' | sed 's/_*$//'; }
[[ -n $features ]] && tag+="_$(clean "$features")"
[[ -n $features_red ]] && tag+="_red$(clean "$features_red")"
[[ -n $features_blue ]] && tag+="_blue$(clean "$features_blue")"
name="bots_${sha}_${map}_s${seed}${tag}"
mkdir -p "$work/videos" "$out"
mvd="$work/videos/$name.mvd"
mkv="$out/$name.mkv"

echo "bot-match-video: playing $map, seed $seed, $minutes minutes ($name)"
"$labs" se-team --basedir="$basedir" --map="$map" --client-fed=1 --minutes="$minutes" \
  --seed="$seed" --features="$features" --features-red="$features_red" \
  --features-blue="$features_blue" --save="$mvd" --why="$work/videos/$name.why.tsv" \
  >"$work/videos/$name.log" 2>&1 || {
  echo "bot-match-video: the match failed; next: read $work/videos/$name.log" >&2
  exit 1
}
grep -E '^  team\.(red|blue)\.frags ' "$work/videos/$name.log" | awk '{printf "  %s %s\n", $1, $2}'
echo "bot-match-video: the demo is $mvd (watch: $game -basedir $basedir -playdemo $mvd)"
echo "bot-match-video: why a bot did something at a moment: make bot-why" \
  "ARGS=\"--video=$name --bot=<name> --at=<m:ss>\""
((video)) || exit 0

echo "bot-match-video: rendering $mkv"
"$game" -basedir "$basedir" -demovideo "$mvd" "$mkv" --autotrack \
  >"$work/videos/$name-video.log" 2>&1 || {
  echo "bot-match-video: the render failed; next: read $work/videos/$name-video.log" >&2
  exit 1
}
ls -la "$mkv"

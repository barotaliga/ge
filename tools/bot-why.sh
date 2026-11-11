#!/usr/bin/env bash
# Why did a bot do that, then? The bots' decision log of a match (`make bot-video` writes one
# beside every video, `se-team --why=<file>` too) read for a bot and a moment of the match's
# recording or video, by the se-team lab's companion `se-why`:
#
#   tools/bot-why.sh --bot=red2 --at=4:12 [--around=10] [--name=mode,trigger]
#                    [--video=<a bot-video's name or path> | --log=<a log>]
#
# Answers with every decision that bot changed and every event (a speed lost to a wall or a
# player, a death) within --around seconds of --at (the video's clock), each with what settled
# it, the margin and the facts it read, then how often each decision is made and changes. With no
# --video or --log, the newest log under .bots/local.md's `work:` (default .bots/work) videos/.
#
# ge:summary Answers why a bot did something at a moment of a match video, from its decision log
# ge:keywords bots why decision-log se-why video demo moment explain debug
# ge:skill bots
set -euo pipefail
cd "$(dirname "$0")/.."

log="" video="" flags=()
for a in "$@"; do
  case $a in
    --log=*) log=${a#--log=} ;;
    --video=*) video=${a#--video=} ;;
    -h | --help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *) flags+=("$a") ;;
  esac
done

work=""
[[ -f .bots/local.md ]] && work=$(sed -n 's/^work:[[:space:]]*//p' .bots/local.md | head -1)
work=${work:-.bots/work}
if [[ -z $log && -n $video ]]; then
  name=$(basename "$video")
  log="$work/videos/${name%.mkv}.why.tsv"
fi
if [[ -z $log ]]; then
  log=$(ls -t "$work"/videos/*.why.tsv 2>/dev/null | head -1 || true)
fi
if [[ -z $log || ! -f $log ]]; then
  echo "bot-why: no decision log${log:+ at $log}; next: make bot-video (it writes one beside" \
    "the video), or pass --log=<a file se-team --why=<file> wrote>" >&2
  exit 1
fi
labs=build/release/labs
[[ -x $labs ]] || cmake --build build/release --target labs
echo "bot-why: $log"
"$labs" se-why --file="$log" ${flags[@]+"${flags[@]}"}

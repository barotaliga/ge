#!/bin/sh
# EVERY MAP'S OVERVIEW, unattended: the `map-overview` lab on each map named (or every .bsp in the
# game folder's maps/), with its calculated rocket jumps, several at once.
#
#   overviews.sh <game folder> <out dir> [map ...]
#
# <game folder> is <basedir>/<game> (its packs and loose files). Each map's picture goes to
# <out dir>/<map>.png, its jumps (one `jump:` line each) to <out dir>/<map>.jumps.txt and its
# views for photos.sh to <out dir>/<map>.views.txt; a map with a `<map>.intent` beside it in
# <out dir> is drawn with that intent. LABS (default build/debug/labs) and JOBS (default the
# processor count) can be set. The pictures show the pack's own textures: keep them out of the tree.
set -eu
game=$1 out=$2
shift 2
labs=${LABS:-build/debug/labs}
jobs=${JOBS:-$(nproc 2>/dev/null || echo 4)}
mkdir -p "$out"
maps="$*"
if [ -z "$maps" ]; then
  for f in "$game"/maps/*.bsp; do [ -f "$f" ] && maps="$maps $(basename "$f" .bsp)"; done
fi
[ -n "$maps" ] || { echo "overviews.sh: no map named and none in $game/maps; name the maps to draw" >&2; exit 1; }
one() {
  map=$1
  work="$out/.work-$map"
  intent=""
  [ -f "$out/$map.intent" ] && intent="--intent=$out/$map.intent"
  if "$labs" map-overview --game="$game" --map="$map" $intent --out="$work" >"$work.log" 2>&1; then
    cp "$work/map-overview/overview.png" "$out/$map.png"
    cp "$work/map-overview/views.txt" "$out/$map.views.txt"
    grep -E '^ *jump:' "$work.log" | sed 's/^ *//' >"$out/$map.jumps.txt" || true
    echo "overviews.sh: $map: $(wc -l <"$out/$map.jumps.txt") jumps"
  else
    echo "overviews.sh: $map failed; its log is $work.log" >&2
  fi
}
n=0
for map in $maps; do
  one "$map" &
  n=$((n + 1))
  if [ "$n" -ge "$jobs" ]; then wait; n=0; fi
done
wait

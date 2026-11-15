#!/bin/sh
# THE PHOTO PASS: every area of a map photographed in the game's own window, one picture an area.
#
#   photos.sh <game folder> <map> <views.txt> <out dir> [labs binary]
#
# <game folder> is <basedir>/<game> (its packs and loose files, maps/<map>.bsp among them);
# <views.txt> is the `map-overview` lab's (one line an area: `<name> x,y,z,yaw,pitch`). Each
# area's enhanced frame is copied to <out dir>/<name>.png and the classic one to
# <out dir>/<name>-classic.png. The frames are private (the pack's own textures): keep them out of
# the tree.
set -eu
game=$1 map=$2 views=$3 out=$4
labs=${5:-build/debug/labs}
mkdir -p "$out"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
list=""
names=""
while read -r name view; do
  [ -n "$name" ] || continue
  list="$list${list:+;}$view"
  names="$names $name"
done < "$views"
[ -n "$list" ] || { echo "photos.sh: $views names no view; run the map-overview lab first" >&2; exit 1; }
"$labs" se-client --game="$game" --map="$map" --views="$list" --out="$work" >"$work/log.txt" 2>&1 || {
  tail -20 "$work/log.txt" >&2
  echo "photos.sh: se-client failed (log above); check the game folder and the map name" >&2
  exit 1
}
i=0
for name in $names; do
  for look in enhanced classic; do
    src="$work/se-client/$look-$i.png"
    [ -f "$src" ] || continue
    if [ "$look" = enhanced ]; then cp "$src" "$out/$name.png"; else cp "$src" "$out/$name-classic.png"; fi
  done
  i=$((i + 1))
done
echo "photos.sh: $i areas photographed into $out"

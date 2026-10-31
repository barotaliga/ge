#!/usr/bin/env bash
# STAGE A HELD WEAPON: build a weapon's Blender builder into a scratch .glb (the tree, the model
# and its stamp untouched) with the game's own `se_view` data, then put it beside the game's model
# of its slot in the `se-weapon-stage` lab, and print what to read: the kit's report (triangles,
# texture fill, wrist bend, reach), the staged table, where the tip lands each strip frame, and the
# files (stage, bench, wire, skin, animation strips, GIF and WebM). The inner loop of the
# held-weapons skill; `ge asset build <name>` puts the model in the tree once it is right.
#
# usage: stage.sh <asset> [--slot=<name in weapons.cpp>] [--builder=<build.py>] [--width=640]
#                 [--preset=debug]
#   <asset>     the model's folder under games/shooters_edge/content/models/view (axe, shotgun)
#   --slot      the weapon it stands in for (default: the asset's name)
#   --builder   a builder elsewhere (a scratch draft); default <asset>/build.py
#   Blender: $GE_BLENDER, else `blender` on the PATH, else the macOS app.
#
# ge:summary The held-weapons skill's loop: a builder into a scratch glb, staged beside the game's
# ge:keywords held-weapons stage weapon blender builder candidate se-weapon-stage scratch glb
# ge:skill held-weapons
set -euo pipefail

asset="${1:?usage: stage.sh <asset> [--slot=..] [--builder=..] [--width=..] [--preset=..]}"
shift
slot="$asset" builder="" width=640 preset=debug
for a in "$@"; do
  case "$a" in
    --slot=*) slot="${a#*=}" ;;
    --builder=*) builder="${a#*=}" ;;
    --width=*) width="${a#*=}" ;;
    --preset=*) preset="${a#*=}" ;;
    *) echo "stage.sh: unknown flag $a; next: see the usage at the top of this script" >&2; exit 2 ;;
  esac
done
root="$(git rev-parse --show-toplevel)"
cd "$root"
view="games/shooters_edge/content/models/view"
builder="${builder:-$view/$asset/build.py}"
[[ -f "$builder" ]] || { echo "stage.sh: no builder at $builder; next: write it (skill held-weapons) or pass --builder=" >&2; exit 2; }

blender="${GE_BLENDER:-}"
if [[ -z "$blender" ]]; then
  if command -v blender >/dev/null; then blender=blender
  elif [[ -x /Applications/Blender.app/Contents/MacOS/Blender ]]; then
    blender=/Applications/Blender.app/Contents/MacOS/Blender
  else
    echo "stage.sh: no Blender; next: install Blender 5.2 or set GE_BLENDER (skill assets)" >&2; exit 2
  fi
fi

out="build/$preset/stage"
mkdir -p "$out/lib"
cmake --build "build/$preset" --target assets labs >"$out/build.log" 2>&1 ||
  { tail -20 "$out/build.log"; echo "stage.sh: the build failed; next: read $out/build.log" >&2; exit 1; }
"build/$preset/assets" data se_view >"$out/se_view.json"
cp engine/assets/blender/ge_asset.py "$out/lib/"
glb="$out/$asset.glb"
rm -f "$glb"
if ! PYTHONPATH="$out/lib:$root/$view" "$blender" --background --factory-startup --python-use-system-env \
    --python "$builder" -- --out "$glb" --data "$out/se_view.json" >"$out/$asset.blender.log" 2>&1 ||
    [[ ! -f "$glb" ]]; then
  grep -E "Error|Traceback|File \"" "$out/$asset.blender.log" | tail -12 || true
  echo "stage.sh: the builder failed; next: read $out/$asset.blender.log" >&2
  exit 1
fi
grep "weapon-kit:" "$out/$asset.blender.log" || true
echo "model: $glb ($(( $(wc -c <"$glb") / 1024 )) KiB)"
"build/$preset/labs" se-weapon-stage --weapon="$slot" --candidate="$glb" --width="$width" \
  >"$out/$asset.lab.log" 2>&1 || true
sed -n '/staged$/,/^  [a-z_]* GAME:/p' "$out/$asset.lab.log" | grep -v " GAME: " || true
awk '/the tip on the screen/ {on = 1} on && /^  [a-z_]+ (GAME|CANDIDATE):/ {on = 0} on' \
  "$out/$asset.lab.log" | awk '!seen[$0]++' || true
awk '/the arms. joints/ {on = 1} on && /^$/ {on = 0} on' "$out/$asset.lab.log" | awk '!seen[$0]++' || true
grep -E "covers|off the picture| arm: |FAIL|no WebM" "$out/$asset.lab.log" || true
grep -E "^  (picture|file):" "$out/$asset.lab.log" | grep -i "$slot" || true

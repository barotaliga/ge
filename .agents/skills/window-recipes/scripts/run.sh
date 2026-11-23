#!/usr/bin/env bash
# RUN A RECIPE in Shooter's Edge's window (docs/console.md § Recipes): the window opened on the
# game, the recipe's cvars set, the recipe executed, and what it left printed: the console's dump
# (every `menu_list` page among it) and the pictures' paths. The window-recipes skill's loop.
#
# usage: run.sh <recipe.cfg> [--game=q1|q2] [--basedir=<dir>] [--preset=release] [--out=<name>]
#               [--timeout=180] [name=value ...]
#   <recipe.cfg>  a recipe (this skill's recipes/, or one of your own)
#   --game        the classic game (default q1)
#   --basedir     the folder that holds the game's files (id1, baseq2)
#   --out         recipe_out: the name its screenshots and its dump take (default: the recipe's)
#   name=value    a cvar the recipe reads, set before it runs (recipe_server=127.0.0.1:27500)
#
# ge:summary The window-recipes skill's loop: a recipe run in the window, its dump and pictures shown
# ge:keywords recipe window-recipes menu_press menu_list sleep condump screenshot scripted-window
# ge:skill window-recipes
set -euo pipefail

recipe="${1:?usage: run.sh <recipe.cfg> [--game=..] [--basedir=..] [--preset=..] [name=value ...]}"
shift
[[ -f "$recipe" ]] || { echo "run.sh: no recipe at $recipe; next: pass a .cfg path" >&2; exit 2; }
game=q1 basedir="" preset=release out="$(basename "$recipe" .cfg)" limit=180
sets=()
for a in "$@"; do
  case "$a" in
    --game=*) game="${a#*=}" ;;
    --basedir=*) basedir="${a#*=}" ;;
    --preset=*) preset="${a#*=}" ;;
    --out=*) out="${a#*=}" ;;
    --timeout=*) limit="${a#*=}" ;;
    *=*) sets+=("+set" "${a%%=*}" "${a#*=}") ;;
    *) echo "run.sh: unknown argument $a; next: see the usage at the top of this script" >&2; exit 2 ;;
  esac
done
root="$(git rev-parse --show-toplevel)"
exe="$root/build/$preset/se"
[[ -x "$exe" ]] || { echo "run.sh: no $exe; next: cmake --build --preset $preset --target se" >&2; exit 2; }
args=(-classic "$game")
[[ -n "$basedir" ]] && args+=(-basedir "$basedir")
log="$(mktemp -t recipe.XXXXXX)"
status=0
if command -v timeout >/dev/null; then
  timeout "$limit" "$exe" "${args[@]}" "${sets[@]}" +set recipe_out "$out" \
    +exec "$(cd "$(dirname "$recipe")" && pwd)/$(basename "$recipe")" >"$log" 2>&1 || status=$?
else
  "$exe" "${args[@]}" "${sets[@]}" +set recipe_out "$out" \
    +exec "$(cd "$(dirname "$recipe")" && pwd)/$(basename "$recipe")" >"$log" 2>&1 || status=$?
fi
dump="$(sed -n 's/^console: Dumped console text to \(.*\)\.$/\1/p' "$log" | tail -1)"
if [[ -n "$dump" && -f "$dump" ]]; then
  echo "== the console ($dump)"
  cat "$dump"
else
  echo "== no console dump: the recipe stopped before its condump; the window's own lines:"
  grep '^console:' "$log" | tail -40 || true
fi
echo "== pictures"
sed -n 's/^console: screenshot: \(.*\) (.*/\1/p' "$log"
echo "== the window's log: $log (exit $status)"
exit "$status"

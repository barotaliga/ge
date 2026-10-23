#!/usr/bin/env bash
# The labs a change reaches, one id a line, for precheck to run at their smoke flags against the
# committed digests (tests/labs_digests.txt) before CI's labs_smoke does. A lab is reached when a
# changed file is its source, or a header its source includes (the build's own dependency record,
# `ninja -t deps`, so headers reached through others count), or a .cpp whose same-name .hpp it
# includes; a moved row of the baseline reaches its lab. Only labs this build registers are named
# (a GPU-less build has no GPU labs). With no ninja dependency record, the labs whose source changed.
#
#   tools/labs-reached.sh BUILD_DIR [BASE_REF]
#
# ge:summary The labs a change reaches, off the build's dependency record, for precheck's digests
# ge:keywords precheck labs digests reached changed-files ninja deps smoke baseline
# ge:skill labs
set -euo pipefail
cd "$(dirname "$0")/.."

build="${1:?usage: tools/labs-reached.sh BUILD_DIR [BASE_REF]}"
base="${2:-origin/main}"
root="$PWD"

merge_base="$(git merge-base "$base" HEAD 2>/dev/null || echo HEAD)"
mapfile -t changed < <(git diff --name-only "$merge_base" --; git ls-files --others --exclude-standard)
((${#changed[@]})) || exit 0

# The labs this build registered: the first word of each listed lab's line.
mapfile -t registered < <("$build/labs" 2>/dev/null | awk '/^  [a-z0-9]/ { print $1 }')
declare -A known=()
for id in "${registered[@]}"; do [[ -n "$id" ]] && known[$id]=1; done

# A lab source's id: the first string in its LabSpec.
lab_id() { awk '/LabSpec k[A-Za-z]* *\{/ { f = 1 } f && /"/ { match($0, /"[^"]*"/); print substr($0, RSTART + 1, RLENGTH - 2); exit }' "$1"; }

declare -A reached=()
# A baseline row that moved reaches its lab.
if printf '%s\n' "${changed[@]}" | grep -qx 'tests/labs_digests.txt'; then
  while read -r id; do reached[$id]=1; done < <(git diff "$merge_base" -- tests/labs_digests.txt |
    sed -n 's/^[-+]\([a-z0-9-][a-z0-9-]*\) [0-9a-f]\{16\}$/\1/p')
fi

has_ninja=0
[[ -f "$build/.ninja_deps" ]] && command -v ninja >/dev/null && has_ninja=1
while read -r src; do
  id="$(lab_id "$src")"
  [[ -n "$id" && -n "${known[$id]:-}" ]] || continue
  deps=("$src")
  if ((has_ninja)); then
    mapfile -t deps < <(ninja -C "$build" -t deps "CMakeFiles/labs.dir/$src.o" 2>/dev/null |
      sed -n "s|^ *$root/||p" | grep -v '^build/')
    ((${#deps[@]})) || deps=("$src")
  fi
  declare -A dep=()
  for d in "${deps[@]}"; do dep[$d]=1; done
  for f in "${changed[@]}"; do
    if [[ -n "${dep[$f]:-}" ]] || { [[ "$f" == *.cpp ]] && [[ -n "${dep[${f%.cpp}.hpp]:-}" ]]; }; then
      reached[$id]=1
      break
    fi
  done
  unset dep
done < <(git grep -l 'GE_LAB(' -- '*_lab.cpp')

for id in "${!reached[@]}"; do [[ -n "${known[$id]:-}" ]] && echo "$id"; done | sort

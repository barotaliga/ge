#!/usr/bin/env bash
# clang-tidy's bug-finding checks (.clang-tidy) over the tree's .cpp files, in parallel
# (`make lint`, CI's `lint` job). Needs a built tree: the compile database and Ninja's header
# dependencies come from it.
#
#   tools/lint.sh BUILD_DIR            every .cpp (CI on main)
#   tools/lint.sh BUILD_DIR BASE_REF   only what a change since BASE_REF can move (CI on a PR):
#                                      each changed .cpp, and each .cpp whose object includes a
#                                      changed header (HeaderFilterRegex reports a header's
#                                      findings through the files that include it)
#
# A change to the lint's own rules (.clang-tidy, this script) lints everything, since any file's
# findings can move. A change to the build's CMake does not: it most often adds a source, which
# the change lists anyway, and a full lint on a pull request takes longer than main takes to
# move on. Every push to main still lints every file, which catches what a flag change moves.
# JOBS=N sets the parallelism (default: cores).
#
# ge:summary Runs clang-tidy in parallel over every .cpp, or what a change reaches (make lint)
# ge:keywords ci lint clang-tidy changed-files parallel header-dependencies
# ge:skill build
set -euo pipefail
cd "$(dirname "$0")/.."

build="${1:?usage: tools/lint.sh BUILD_DIR [BASE_REF]}"
base="${2:-}"
jobs="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}"
[[ -f "$build/compile_commands.json" ]] ||
  { echo "lint: $build/compile_commands.json missing; run \`make build\` first." >&2; exit 2; }

mapfile -t all < <(git ls-files --cached --others --exclude-standard '*.cpp')
files=("${all[@]}")

if [[ -n "$base" ]]; then
  if ! merge_base="$(git merge-base "$base" HEAD 2>/dev/null)"; then
    echo "lint: no merge base with $base (a shallow clone? fetch with depth 0); linting all." >&2
  else
    mapfile -t changed < <(git diff --name-only --diff-filter=d "$merge_base" -- ; git ls-files --others --exclude-standard)
    full=0
    for f in "${changed[@]}"; do
      case "$f" in
        .clang-tidy | tools/lint.sh) full=1 ;;
      esac
    done
    if (( ! full )); then
      root="$PWD"
      # Ninja's deps: "<object>: #deps ..." then one indented path per line, the source first.
      # Print every source whose object reads a changed file.
      affected="$(ninja -C "$build" -t deps | awk -v root="$root" -v list="$(printf '%s\n' "${changed[@]}")" '
        BEGIN { n = split(list, c, "\n"); for (i = 1; i <= n; i++) if (c[i] != "") want[root "/" c[i]] = 1 }
        /^[^ ]/ { src = ""; next }
        /^    / { p = substr($0, 5); if (src == "") src = p; if (p in want) hit[src] = 1 }
        END { for (s in hit) if (index(s, root "/") == 1) print substr(s, length(root) + 2) }')"
      files=()
      for f in "${all[@]}"; do
        if grep -qxF -- "$f" <<<"$affected" || printf '%s\n' "${changed[@]}" | grep -qxF -- "$f"; then
          files+=("$f")
        fi
      done
      echo "lint: ${#files[@]} of ${#all[@]} files reach a change since $base."
    else
      echo "lint: the lint's own rules changed since $base; linting all ${#all[@]} files."
    fi
  fi
fi

# A file this build does not compile (the GPU's or the window's, in a build without them) has
# no flags to lint it with: it is left to CI, whose lint build has every feature on.
db="$build/compile_commands.json"
kept=()
for f in "${files[@]}"; do
  if grep -qF -- "$PWD/$f\"" "$db"; then kept+=("$f"); fi
done
if (( ${#kept[@]} < ${#files[@]} )); then
  echo "lint: $(( ${#files[@]} - ${#kept[@]} )) files are not in this build ($build); CI lints them."
fi
files=("${kept[@]}")

(( ${#files[@]} )) || { echo "lint: nothing to lint."; exit 0; }
echo "lint: clang-tidy over ${#files[@]} files, $jobs at a time."
# xargs exits non-zero when any clang-tidy did. Its progress and "N warnings generated" lines
# (the suppressed ones, from system headers) are dropped, so only findings are printed.
set +e
printf '%s\n' "${files[@]}" | xargs -P "$jobs" -n 4 clang-tidy -p "$build" --quiet 2>&1 |
  grep -vE '^[0-9]+ warnings? (and [0-9]+ errors? )?generated\.$|^\[[0-9]+/[0-9]+\] Processing file |^Suppressed [0-9]+ warnings|^Use -header-filter|^\.?$'
status=${PIPESTATUS[1]}
set -e
if (( status != 0 )); then
  echo "lint: clang-tidy found the problems above. Fix each at its file and line (.clang-tidy says why the check is on)." >&2
  exit 1
fi
echo "lint: ${#files[@]} files clean."

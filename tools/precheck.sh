#!/usr/bin/env bash
# CI's cheap gates on this tree, before the push (`make precheck`): the checks that cost the most
# CI rounds, each a minute or less here, cheapest first, stopping at the first red one.
#
#   1. fmt-check     clang-format over every source
#   2. agents-check  the links, the router, the line cap, the ge: tags, the lessons
#   3. libm-check    no <cmath> transcendental in simulation code
#   4. release       the changeset fragments valid
#   5. build         the preset's build (incremental)
#   6. lint          clang-tidy over what the change since BASE (default origin/main) reaches
#   7. tests         cli_diagnostics_test (every message held to the diagnostics convention),
#                    each changed test, and the test every changed file's ge:test tag names
#   8. digests       each lab the change reaches (tools/labs-reached.sh) at its smoke flags,
#                    against tests/labs_digests.txt: a digest the change moved is caught here,
#                    not by CI's labs_smoke a round later
#
# It is not the whole of CI: the other compilers, the labs it does not reach and the GPU rows
# still run there. A game repo has `ge precheck` instead (engine/cli/ci.hpp).
#
#   tools/precheck.sh BUILD_DIR PRESET [BASE_REF]
#
# ge:summary CI's cheap gates before a push: format, agents, libm, lint, tests, the reached labs
# ge:keywords ci precheck gates fmt-check agents-check libm-check lint changed-files affected-tests
# ge:keywords reached-labs digests
# ge:skill commit
set -euo pipefail
cd "$(dirname "$0")/.."

build="${1:?usage: tools/precheck.sh BUILD_DIR PRESET [BASE_REF]}"
preset="${2:?usage: tools/precheck.sh BUILD_DIR PRESET [BASE_REF]}"
base="${3:-origin/main}"
start=$SECONDS

log="$build/precheck.log"
mkdir -p "$build"

# One gate: its output kept in $log, and shown only when it fails.
gate() {
  local name="$1"
  shift
  local t=$SECONDS
  if ! "$@" >"$log" 2>&1; then
    tail -n 40 "$log" >&2
    echo "precheck: $name failed (above; all of it in $log). Fix it, then make precheck again." >&2
    exit 1
  fi
  echo "precheck: $name ok ($((SECONDS - t)) s)"
}

gate fmt-check make -s fmt-check
gate agents-check make -s agents-check
gate libm-check make -s libm-check
gate release python3 tools/release.py check
gate build make -s build PRESET="$preset"
gate lint tools/lint.sh "$build" "$base"

# The tests the change reaches: changed tests, and the ge:test of every changed file.
merge_base="$(git merge-base "$base" HEAD 2>/dev/null || echo HEAD)"
mapfile -t changed < <(git diff --name-only --diff-filter=d "$merge_base" --; git ls-files --others --exclude-standard)
tests=(cli_diagnostics_test)
for f in "${changed[@]}"; do
  [[ -f "$f" ]] || continue
  if [[ "$f" == tests/*_test.cpp ]]; then tests+=("$(basename "$f" .cpp)"); fi
  while read -r t; do
    [[ "$t" == tests/*_test.cpp ]] && tests+=("$(basename "$t" .cpp)")
  done < <(sed -n 's/^.*ge:test \(tests\/[^ ]*_test\.cpp\).*$/\1/p' "$f" 2>/dev/null | head -5)
done
mapfile -t tests < <(printf '%s\n' "${tests[@]}" | sort -u)
regex="^($(IFS='|'; echo "${tests[*]}"))\$"
gate "tests (${#tests[@]})" ctest --test-dir "$build" -R "$regex" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu)" \
  --output-on-failure

# The labs the change reaches, each against its committed digest, side by side.
labs_digests() {
  printf '%s\n' "$@" | xargs -P "$(nproc 2>/dev/null || sysctl -n hw.ncpu)" -I{} \
    "$build/labs" {} --smoke --quiet --check-digests=tests/labs_digests.txt
}
mapfile -t labs < <(tools/labs-reached.sh "$build" "$base")
if ((${#labs[@]})); then
  gate "digests (${labs[*]})" labs_digests "${labs[@]}"
fi
echo "precheck: CI's cheap gates pass ($((SECONDS - start)) s); push, and CI runs the rest."

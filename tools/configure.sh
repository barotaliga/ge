#!/usr/bin/env bash
# Configures a preset (`cmake --preset <preset>`), then makes sure Ninja can build what it wrote.
# A cloud session's start builds the debug preset in the background (tools/session-start.sh);
# a second CMake and Ninja run in the same directory at once regenerate build.ninja under each
# other, so a configure here first waits for that build to finish (its lock), and says so.
# A build directory carried over from another session (or restored with its file times) can be
# left with CMakeFiles/cmake.verify_globs newer than build.ninja, or dated in the future. Ninja
# then re-runs CMake to regenerate build.ninja on every build, CMake leaves the unchanged file as
# it is, and the build loops until Ninja stops with "manifest 'build.ninja' still dirty". Here a
# stamp dated in the future is brought back to now, and build.ninja is dated no older than it.
#
#   tools/configure.sh <preset>   used by `make build`, `make test`, `make shaders-check` and precheck
#
# ge:summary Configures a CMake preset and keeps build.ninja from looping as still dirty
# ge:keywords build configure cmake preset ninja still-dirty verify-globs session-start lock
# ge:skill build
set -euo pipefail
preset=${1:?usage: tools/configure.sh <preset>}
lock=build/session-start.lock
if [[ -z ${GE_SESSION_START_WORKER:-} && -f $lock ]] && command -v flock >/dev/null; then
  if ! flock -n "$lock" true; then
    echo "configure: waiting for the session start's build (build/session-start.log) to finish" >&2
    flock "$lock" true
  fi
fi
cmake --preset "$preset"
dir=build/$preset
stamp=$dir/CMakeFiles/cmake.verify_globs
[[ -f $dir/build.ninja && -f $stamp ]] || exit 0
now=$(date +%s)
if (($(stat -c %Y "$stamp" 2>/dev/null || stat -f %m "$stamp") > now)); then
  echo "configure: $stamp was dated in the future; dated now so Ninja can settle" >&2
  touch "$stamp"
fi
if [[ $dir/build.ninja -ot $stamp ]]; then
  echo "configure: $dir/build.ninja was older than $stamp; dated with it so Ninja does not loop" >&2
  touch -r "$stamp" "$dir/build.ninja"
fi

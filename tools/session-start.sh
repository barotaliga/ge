#!/usr/bin/env bash
# The cloud session's start hook (.claude/settings.json, SessionStart): starts the debug build
# of the engine, the plugins and Fall Line in the BACKGROUND and returns at once, so the build
# runs while the agent reads. Only in a cloud session (CLAUDE_CODE_REMOTE=true), or when
# GE_SESSION_BUILD=1 asks for it locally.
#
# The worker (this script with --worker) installs the Linux packages CI installs that the
# container lacks (GE_LINUX_PACKAGES in .github/workflows/ci.yml, read from there, never
# restated), then runs `make build` into build/debug, reusing it, so a second session or a
# resumed one rebuilds only what changed. One worker at a time (a lock); a hook fired while
# one runs does nothing.
#
# Where to look: build/session-start.log (the whole output) and build/session-start.status
# (one line: `running <started>`, `ok <finished>` or `failed <exit code> <finished>`). Before
# relying on the build, an agent waits for it: `tools/session-start.sh --wait` blocks until
# the worker is done and exits with its result. A failed build is rerun by hand with
# `make build`; the log names the first error.
#
# ge:summary The cloud session's start hook: the debug build in the background, logged
# ge:keywords session-start hook cloud background build log status wait
# ge:skill build
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
OUT="$ROOT/build"
LOG="$OUT/session-start.log"
STATUS="$OUT/session-start.status"
LOCK="$OUT/session-start.lock"
stamp() { date -u +%Y-%m-%dT%H:%M:%SZ; }

worker() {
  exec 9>"$LOCK"
  flock -n 9 || exit 0  # another worker holds it
  echo "running $(stamp)" >"$STATUS"
  : >"$LOG"
  {
    echo "== session-start $(stamp): $ROOT"
    if [[ "$(uname -s)" == Linux && "$(id -u)" == 0 ]] && command -v apt-get >/dev/null; then
      # The folded block's lines: indented deeper than the key, up to the first that is not.
      pkgs="$(awk '/^  GE_LINUX_PACKAGES:/ { on = 1; next } on && /^    / { print; next } { on = 0 }' \
        .github/workflows/ci.yml)"
      [[ -n "$pkgs" ]] || echo "session-start: no GE_LINUX_PACKAGES in .github/workflows/ci.yml"
      missing=()
      for p in $pkgs; do dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p"); done
      if ((${#missing[@]})); then
        echo "== apt-get install ${missing[*]}"
        # Best effort: without them SDL builds without that backend, and the build goes on.
        { apt-get update -qq && DEBIAN_FRONTEND=noninteractive \
          apt-get install -y -qq --no-install-recommends "${missing[@]}"; } ||
          echo "session-start: apt-get failed; the build continues without those packages"
      fi
    fi
    echo "== make build $(stamp)"
    make build
  } >>"$LOG" 2>&1 && rc=0 || rc=$?
  if ((rc == 0)); then
    echo "ok $(stamp)" >"$STATUS"
    echo "== ok $(stamp)" >>"$LOG"
  else
    echo "failed $rc $(stamp)" >"$STATUS"
    echo "== failed (exit $rc) $(stamp): rerun \`make build\` for the first error" >>"$LOG"
  fi
  exit "$rc"
}

wait_for() {
  [[ -f "$STATUS" ]] || { echo "session-start: no build started (no $STATUS)" >&2; exit 2; }
  # Taking the lock waits for a running worker to finish. A status still `running` with the
  # lock free is a worker not yet started (the hook writes it first); after 30 s, a dead one.
  local tries=0 state rest
  while :; do
    exec 9>"$LOCK"
    flock 9
    read -r state rest <"$STATUS"
    [[ "$state" != running ]] && break
    exec 9>&-
    if ((++tries > 30)); then
      echo "session-start: status says running but no worker holds the lock; rerun \`make build\`" >&2
      exit 1
    fi
    sleep 1
  done
  echo "session-start: $state $rest (log: build/session-start.log)"
  [[ "$state" == ok ]]
}

case "${1:-}" in
  --worker) worker; exit ;;
  --wait) wait_for; exit ;;
  "") ;;
  *) echo "session-start: unknown flag '$1'; use none (the hook), --wait or --worker" >&2; exit 2 ;;
esac

if [[ "${CLAUDE_CODE_REMOTE:-}" != true && "${GE_SESSION_BUILD:-}" != 1 ]]; then
  exit 0
fi
mkdir -p "$OUT"
flock -n "$LOCK" true && echo "running $(stamp)" >"$STATUS"
setsid nohup "$0" --worker </dev/null >/dev/null 2>&1 &
if [[ -n "${CLAUDE_ENV_FILE:-}" ]]; then
  echo "export GE_SESSION_BUILD_LOG=\"$LOG\"" >>"$CLAUDE_ENV_FILE"
fi
echo "session-start: the debug build runs in the background; log build/session-start.log," \
  "status build/session-start.status, wait with tools/session-start.sh --wait"

#!/usr/bin/env bash
# The cloud session's start hook (.claude/settings.json, SessionStart): starts the debug build
# of the engine, the plugins and Fall Line in the BACKGROUND and returns at once, so the build
# runs while the agent reads. Only in a cloud session (CLAUDE_CODE_REMOTE=true), or when
# GE_SESSION_BUILD=1 asks for it locally.
#
# The worker (this script with --worker) brings the container up to what the tree needs with
# tools/setup-ubuntu.sh (the one list of Linux packages, CI's too; a no-op in well under a second
# when nothing is missing), switching the Ubuntu archive to its HTTPS mirrors first because the
# cloud session's proxy passes HTTPS alone, then runs `make build` into build/debug, reusing it,
# so a second session or a resumed one rebuilds only what changed; a new container's first build
# takes its objects from the shared compiler cache (tools/ccache-share.sh), which it then saves. One worker at a time (a lock); a hook fired while
# one runs does nothing.
#
# Where to look: build/session-start.log (the whole output) and build/session-start.status
# (one line: `running <started>`, `ok <finished>` or `failed <exit code> <finished>`). Before
# relying on the build, an agent waits for it: `tools/session-start.sh --wait` blocks until
# the worker is done and exits with its result. A failed build is rerun by hand with
# `make build`; the log names the first error. The package step is cut off after GE_SETUP_TIMEOUT
# seconds (600) and tried twice, a worker stopped by a signal writes `failed killed ... during
# <step>`, and one killed outright is named by --wait (`failed died`) instead of left `running`.
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

# The packages, tried twice with a time limit each: a mirror that hangs is cut off and retried
# rather than holding the build back for good.
setup_packages() {
  local attempt
  for attempt in 1 2; do
    echo "== tools/setup-ubuntu.sh (attempt $attempt) $(stamp)"
    local rc=0
    timeout "${GE_SETUP_TIMEOUT:-600}" tools/setup-ubuntu.sh --https-mirrors || rc=$?
    ((rc == 0)) && return 0
    echo "session-start: tools/setup-ubuntu.sh $( ((rc == 124)) && echo "timed out" || echo "failed (exit $rc)"), above"
  done
  return 1
}

worker() {
  exec 9>"$LOCK"
  flock -n 9 || exit 0  # another worker holds it
  echo "running $(stamp)" >"$STATUS"
  : >"$LOG"
  # A worker stopped by a signal says so in the status instead of leaving it `running`.
  local step=start
  trap 'echo "failed killed $(stamp) during $step" >"$STATUS"
        echo "== killed during $step $(stamp): rerun tools/session-start.sh --worker" >>"$LOG"
        exit 1' HUP INT TERM
  {
    echo "== session-start $(stamp): $ROOT"
    if [[ "$(uname -s)" == Linux && "$(id -u)" == 0 ]] && command -v apt-get >/dev/null; then
      step=packages
      # Best effort: without a package SDL builds without that backend, and the build goes on.
      setup_packages ||
        echo "session-start: tools/setup-ubuntu.sh failed twice (above); the build continues without those packages"
    fi
    step=build
    echo "== make build $(stamp)"
    make build
  } >>"$LOG" 2>&1 && rc=0 || rc=$?
  if ((rc == 0)); then
    tools/ccache-share.sh save >>"$LOG" 2>&1 || true
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
      # A worker killed outright (no signal it could catch): name the step it died in, loudly.
      local last
      last="$(grep '^== ' "$LOG" 2>/dev/null | tail -1)"
      echo "session-start: FAILED: the worker died without finishing (last step: ${last:-none})." >&2
      echo "  Rerun it: tools/session-start.sh --worker (or tools/setup-ubuntu.sh, then make build)" >&2
      echo "failed died $(stamp) after: ${last:-start}" >"$STATUS"
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

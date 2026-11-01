#!/usr/bin/env bash
# The build caches carried between machines that do not keep their disk: a cloud session's
# container starts empty, so its first build would compile everything (about 18 minutes for the
# debug preset on four cores) and download and build every dependency. This keeps two caches in a
# folder that outlives the container and puts them back before the first build of the next one:
#   ccache  the compiler cache (`ccache -k cache_dir`): every object file built before;
#   deps    .cache/deps/: Dawn, SDL3 and the codecs as downloaded and built at configure time.
#
#   tools/ccache-share.sh restore   each cache from the share when its local copy is empty (a new
#                                   container); otherwise nothing. In a cloud container without
#                                   ccache it first runs tools/setup-ubuntu.sh, so the first build
#                                   compiles through the cache. `make build`, `make test` and
#                                   `make precheck` call it first.
#   tools/ccache-share.sh save      each cache into the share, in the background, at most once in
#                                   GE_CCACHE_SHARE_EVERY minutes (default 30) by anyone sharing
#                                   it, and deps only when they changed. `make build`, `make test`,
#                                   tools/precheck.sh and tools/session-start.sh call it after a
#                                   good build. --now: in the foreground, whatever the age.
#   tools/ccache-share.sh where     the share's folder, or nothing when no share is set.
#
# The folder is GE_CCACHE_SHARE; with none set it is /mnt/project-files/.ccache when the cloud
# session's shared project folder is mounted. GE_CCACHE_SHARE=off turns it off. Without a folder
# every command does nothing; without ccache only deps are carried.
#
# THE SHARED FOLDER REFUSES BIG FILES. A file over about 500 MB written there never reaches
# another container (the write succeeds locally and the upload is dropped), so each cache is saved
# as a plain tar cut into parts of GE_CCACHE_SHARE_PART MB (default 200), one save per id:
# `<cache>-<OS>-<CPU>.<id>.NNN`. The small `<cache>-<OS>-<CPU>.manifest` (the id, then each part
# and its size) is written last, by rename, so a reader sees one whole save or the one before it;
# a restore checks every part's size first and builds without the cache when one is short. Older
# saves' parts are deleted after the manifest moves on.
#
# ge:summary Carries the compiler cache and the deps between cloud containers in a shared folder
# ge:keywords ccache compiler-cache build-cache cloud-session restore save archive share deps parts
# ge:skill build
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

share="${GE_CCACHE_SHARE:-}"
if [[ -z "$share" && -d /mnt/project-files ]]; then
  share=/mnt/project-files/.ccache
fi
[[ -n "$share" && "$share" != off ]] || exit 0

tag="$(uname -s)-$(uname -m)"
every="${GE_CCACHE_SHARE_EVERY:-30}"
part_mb="${GE_CCACHE_SHARE_PART:-200}"
deps_dir="$ROOT/.cache/deps"

# A cloud container (root, apt) without ccache installs the tree's packages first, so the build
# that follows compiles through the cache; tools/setup-ubuntu.sh is a no-op when nothing is missing.
bootstrap() {
  command -v ccache >/dev/null && return 0
  [[ "$(uname -s)" == Linux && "$(id -u)" == 0 ]] && command -v apt-get >/dev/null || return 0
  echo "ccache-share: no ccache in this container; tools/setup-ubuntu.sh installs it first" >&2
  timeout "${GE_SETUP_TIMEOUT:-600}" tools/setup-ubuntu.sh --https-mirrors --quiet >&2 ||
    echo "ccache-share: tools/setup-ubuntu.sh failed; building without the compiler cache" >&2
}

# The local directory of a cache, or nothing when this machine has none.
local_of() {
  case "$1" in
    ccache) command -v ccache >/dev/null && ccache -k cache_dir || true ;;
    deps) echo "$deps_dir" ;;
  esac
}

# Whether a local cache holds anything: ccache's results and manifests end in R and M.
has_entries() {
  case "$1" in
    ccache) [[ -n "$(find "$2" -type f -name '*[RM]' -print -quit 2>/dev/null)" ]] ;;
    deps) [[ -n "$(find "$2" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]] ;;
  esac
}

# The parts the manifest names, one "<file> <bytes>" per line; fails when one is missing or short.
parts_of() {
  local manifest="$1" file size
  tail -n +2 "$manifest" | while read -r file size; do
    [[ -f "$share/$file" && "$(stat -c %s "$share/$file")" == "$size" ]] || {
      echo "ccache-share: $share/$file is missing or not $size bytes; the save is incomplete" >&2
      return 1
    }
    echo "$share/$file"
  done
}

restore_one() {
  local name="$1" dir manifest parts t
  dir="$(local_of "$name")"
  [[ -n "$dir" ]] || return 0
  manifest="$share/$name-$tag.manifest"
  [[ -f "$manifest" ]] || return 0
  has_entries "$name" "$dir" && return 0
  mkdir -p "$dir"
  exec 9>"$dir.share.lock" # beside the cache, not in it, so it counts as no entry
  flock 9
  has_entries "$name" "$dir" && return 0 # another restore finished while this one waited
  t=$SECONDS
  if parts="$(parts_of "$manifest")" && [[ -n "$parts" ]] &&
    xargs -d '\n' cat <<<"$parts" | tar -xf - -C "$dir"; then
    # The saving container's counters came along; zeroed so `ccache -s` reads this one's builds.
    [[ "$name" == ccache ]] && ccache -z >/dev/null
    echo "ccache-share: $name restored ($(du -sh "$dir" | cut -f1), $((SECONDS - t)) s) from $share" >&2
  else
    echo "ccache-share: could not read the $name save in $share; building without it" >&2
  fi
  exec 9>&-
}

save_one() {
  local name="$1" dir manifest id tmp n size
  dir="$(local_of "$name")"
  [[ -n "$dir" ]] && has_entries "$name" "$dir" || return 0
  manifest="$share/$name-$tag.manifest"
  # deps change only when a pin moves: saved again only when something in them is newer.
  if [[ "$name" == deps && -f "$manifest" &&
    -z "$(find "$dir" -newer "$manifest" -print -quit 2>/dev/null)" ]]; then
    return 0
  fi
  mkdir -p "$share"
  id="$(date -u +%Y%m%dT%H%M%S)-$(hostname | tr -cd 'A-Za-z0-9' | cut -c1-12)-$$"
  local t=$SECONDS
  if ! tar -cf - -C "$dir" --exclude='*.lock' . |
    split -a 3 -d -b "${part_mb}M" - "$share/$name-$tag.$id."; then
    rm -f "$share/$name-$tag.$id."*
    echo "ccache-share: could not write the $name save to $share; the next container builds without it" >&2
    return 1
  fi
  tmp="$manifest.$id.tmp"
  echo "$id" >"$tmp"
  for n in "$share/$name-$tag.$id."*; do
    echo "$(basename "$n") $(stat -c %s "$n")" >>"$tmp"
  done
  mv -f "$tmp" "$manifest"
  # Older saves' parts (and the single archives of the first version of this script).
  find "$share" -maxdepth 1 -type f \( -name "$name-$tag.*.[0-9][0-9][0-9]" -o -name "$name-$tag.tar" \) \
    ! -name "$name-$tag.$id.*" -mmin +1 -delete 2>/dev/null || true
  size="$(awk 'NR > 1 { s += $2 } END { printf "%.0f", s / 1048576 }' "$manifest")"
  echo "ccache-share: $name saved ($size MB in $(($(wc -l <"$manifest") - 1)) parts," \
    "$((SECONDS - t)) s) to $share" >&2
}

case "${1:-}" in
  where) echo "$share" ;;
  restore)
    bootstrap
    restore_one ccache
    restore_one deps
    ;;
  save)
    if [[ "${2:-}" != --now ]]; then
      m="$share/ccache-$tag.manifest"
      if [[ -f "$m" ]] && [[ -n "$(find "$m" -mmin "-$every" 2>/dev/null)" ]]; then
        exit 0 # saved recently, by this session or another
      fi
      setsid nohup "$0" save --now </dev/null >/dev/null 2>&1 &
      exit 0
    fi
    exec 8>"${TMPDIR:-/tmp}/ge-ccache-share-save.lock"
    flock -n 8 || exit 0 # a save from this machine is already running
    save_one ccache
    save_one deps
    ;;
  *)
    echo "ccache-share: unknown command '${1:-}'; use restore, save [--now] or where" >&2
    exit 2
    ;;
esac

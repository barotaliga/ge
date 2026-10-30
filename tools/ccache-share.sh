#!/usr/bin/env bash
# The compiler cache carried between machines that do not keep their disk: a cloud session's
# container starts empty, so its first build would compile everything (about an hour for the
# debug preset on four cores). This keeps the local ccache as one archive in a folder that
# outlives the container, and puts it back before the first build of the next one.
#
#   tools/ccache-share.sh restore   the archive into the local cache, when the local cache is
#                                   empty (a new container); otherwise nothing. `make build`,
#                                   `make test` and `make precheck` call it first.
#   tools/ccache-share.sh save      the local cache into the archive, in the background, at most
#                                   once in GE_CCACHE_SHARE_EVERY minutes (default 30) by anyone
#                                   sharing it. tools/precheck.sh and tools/session-start.sh call
#                                   it after a good build. --now: in the foreground, whatever the
#                                   age.
#   tools/ccache-share.sh where     the archive's path, or nothing when no share is set.
#
# The folder is GE_CCACHE_SHARE; in a cloud session (CLAUDE_CODE_REMOTE=true) with none set it is
# the session's shared project folder, /mnt/project-files/.ccache, when that is mounted.
# GE_CCACHE_SHARE=off turns it off. Without ccache, or with no folder, every command does nothing.
# One archive per OS and CPU (`ccache-Linux-x86_64.tar`); entries are already compressed, so the
# archive is a plain tar. Two sessions saving at once each write a temporary file and rename it,
# so the archive is always one whole save.
#
# ge:summary Carries the local ccache between cloud containers as one archive in a shared folder
# ge:keywords ccache compiler-cache build-cache cloud-session restore save archive share
# ge:skill build
set -euo pipefail

share="${GE_CCACHE_SHARE:-}"
if [[ -z "$share" && "${CLAUDE_CODE_REMOTE:-}" == true && -d /mnt/project-files ]]; then
  share=/mnt/project-files/.ccache
fi
[[ -n "$share" && "$share" != off ]] || exit 0
command -v ccache >/dev/null || exit 0

archive="$share/ccache-$(uname -s)-$(uname -m).tar"
local_dir="$(ccache -k cache_dir)"
every="${GE_CCACHE_SHARE_EVERY:-30}"

# Entries in the local cache: none in a new container.
entries() { find "$local_dir" -type f -name '*[RM]' 2>/dev/null | head -n 1 | wc -l; }

case "${1:-}" in
  where) echo "$archive" ;;
  restore)
    [[ -f "$archive" ]] || exit 0
    (($(entries) == 0)) || exit 0
    mkdir -p "$local_dir"
    exec 9>"$local_dir/.share.lock"
    flock 9
    (($(entries) == 0)) || exit 0 # another restore finished while this one waited
    t=$SECONDS
    if tar -xf "$archive" -C "$local_dir"; then
      echo "ccache-share: the shared compiler cache restored ($(du -sh "$local_dir" | cut -f1)," \
        "$((SECONDS - t)) s) from $archive" >&2
    else
      echo "ccache-share: could not read $archive; building without it (delete it if it is damaged)" >&2
    fi
    ;;
  save)
    if [[ "${2:-}" != --now ]]; then
      if [[ -f "$archive" ]] && [[ -n "$(find "$archive" -mmin "-$every" 2>/dev/null)" ]]; then
        exit 0 # saved recently, by this session or another
      fi
      setsid nohup "$0" save --now </dev/null >/dev/null 2>&1 &
      exit 0
    fi
    (($(entries) > 0)) || exit 0
    mkdir -p "$share"
    tmp="$archive.$(hostname).$$.tmp"
    if tar -cf "$tmp" -C "$local_dir" --exclude=.share.lock --exclude='*.lock' . && mv -f "$tmp" "$archive"; then
      echo "ccache-share: saved $(du -sh "$archive" | cut -f1) to $archive" >&2
    else
      rm -f "$tmp"
      echo "ccache-share: could not write $archive; the next container builds without it" >&2
      exit 1
    fi
    ;;
  *)
    echo "ccache-share: unknown command '${1:-}'; use restore, save [--now] or where" >&2
    exit 2
    ;;
esac

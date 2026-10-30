#!/usr/bin/env bash
# Brings an Ubuntu machine up to what the tree needs: builds the engine and Fall Line, runs `make
# precheck`, `make test`, `make lint`, `make shaders-check` and every lab, headless, rendering on
# Mesa's software Vulkan driver (lavapipe) under a virtual X display (Xvfb). The one list of
# Linux packages: CI's jobs and the cloud session's start hook run it too.
#
#   sudo tools/setup-ubuntu.sh           install or upgrade what is missing or too old
#   tools/setup-ubuntu.sh --check        say what is missing or too old, change nothing (exit 1)
#   tools/setup-ubuntu.sh --list         print the apt packages it would ask for, one per line
#   --cache-dir=DIR                      keep the downloaded .deb files in DIR and install from
#                                        there when they are current (CI caches DIR between runs)
#   --https-mirrors                      switch the Ubuntu archive's sources from http:// to
#                                        https:// first (a network that only passes HTTPS, such as
#                                        the cloud session's proxy)
#   --quiet                              say nothing when there is nothing to do
#   sudo tools/setup-ubuntu.sh --verify  install, then build and run the whole suite as CI's
#                                        Linux x64 row does (xvfb-run, GE_GPU_REQUIRE=1)
#
# IDEMPOTENT AND FAST WHEN DONE. A fresh Ubuntu Server minimal install and a machine that has
# most of it already take the same command. Libraries count as present when dpkg has them
# installed; tools count when the command on PATH is new enough, wherever it came from (a CI
# image's own CMake or Node, a hand-built Clang). Only what is missing or too old is installed or
# upgraded; when nothing is, the script touches no network and returns in well under a second, so
# a CI job can run it unconditionally. It needs root (or sudo) only when it has work to do.
#
# What it brings, and why:
#   - the toolchain: GCC 13+ (the debug preset's ASan/UBSan), Clang 17+ (CI's other compiler),
#     CMake 3.25+, Ninja, ccache and LLD (every build compiles and links
#     through them: cmake/CompilerCache.cmake, cmake/Linker.cmake), make, pkg-config, git, curl;
#   - the gates: clang-format 18 and clang-tidy 18 (CI's major: another clang-format formats some
#     lines differently and fails fmt-check), Node.js 18+ (tools/*.mjs, no npm packages) and
#     Python 3.10+ (tools/release.py, standard library only); gh, the GitHub CLI;
#   - the platform and the GPU: SDL3's X11, Wayland and audio headers, lavapipe and the Vulkan
#     loader Dawn opens, Xvfb, and nasm for the AV1 encoder's assembly (LIB_PKGS below).
# SDL3, Dawn, libaom and libopus are not apt packages: CMake downloads them, pinned and
# checksummed, into .cache/deps/ on the first configure. Blender (`ge asset`) is optional and not
# installed here (GE_BLENDER, the `assets` skill).
#
# Ubuntu 24.04 LTS has every package. 22.04 LTS lacks CMake 3.25, GCC 13, Clang 18 and Node 18,
# so there the script adds Kitware's, the toolchain PPA's, apt.llvm.org's, NodeSource's and
# GitHub's apt sources (only when a tool needs one) and points the unversioned names at the new
# compilers with update-alternatives.
#
# ge:summary Installs or upgrades every Ubuntu package the tree needs; a no-op when all are there
# ge:keywords setup ubuntu apt dependencies packages install lavapipe xvfb toolchain bootstrap
# ge:keywords idempotent ci session-start
# ge:skill build
set -euo pipefail
CALLER_PWD="$(pwd)"
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

# The platform's and the GPU's libraries: present when dpkg has them installed.
LIB_PKGS=(
  libx11-dev libxext-dev libxcursor-dev libxrandr-dev libxi-dev libxss-dev libxfixes-dev
  libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev
  libasound2-dev libpulse-dev libpipewire-0.3-dev
  mesa-vulkan-drivers libvulkan1 xvfb xauth nasm ca-certificates
)

# The tools: `command minimum package-on-24.04 package-on-22.04 [exact]`. Present when the command
# on PATH reports at least the minimum (`exact`: that major and no other). A 22.04 package with a
# version in its name gets the unversioned command through update-alternatives.
TOOLS=(
  "cmake 3.25 cmake cmake"
  "ninja 1.10 ninja-build ninja-build"
  "make 4.0 make make"
  "gcc 13 gcc gcc-13"
  "g++ 13 g++ g++-13"
  "clang 17 clang clang-18"
  "clang++ 17 clang clang-18"
  "clang-format 18 clang-format clang-format-18 exact"
  "clang-tidy 18 clang-tidy clang-tidy-18 exact"
  "ccache 4.0 ccache ccache"
  "ld.lld 14 lld lld-18"
  "pkg-config 0.29 pkg-config pkg-config"
  "git 2.25 git git"
  "curl 7.68 curl curl"
  "python3 3.10 python3 python3"
  "node 18 nodejs nodejs"
  "gh 2.0 gh gh"
)

mode=install
quiet=0
cache_dir=""
https_mirrors=0
for arg in "$@"; do
  case "$arg" in
    --check) mode=check ;;
    --list) mode=list ;;
    --verify) mode=verify ;;
    --quiet) quiet=1 ;;
    --cache-dir=*) cache_dir="${arg#--cache-dir=}"
      [[ "$cache_dir" == /* ]] || cache_dir="$CALLER_PWD/$cache_dir" ;;
    --https-mirrors) https_mirrors=1 ;;
    -h | --help) sed -n '2,/^# ge:summary/{/^# ge:summary/d;s/^# \{0,1\}//;p}' "$0"; exit 0 ;;
    *) echo "setup-ubuntu: unknown flag '$arg'; use --check, --list, --verify, --cache-dir=DIR, --https-mirrors, --quiet or none (--help)" >&2; exit 2 ;;
  esac
done

say() { ((quiet)) || echo "$@"; }
die() { echo "setup-ubuntu: $*" >&2; exit 1; }

[[ -r /etc/os-release ]] || die "no /etc/os-release; this script is for Ubuntu 24.04 or 22.04"
# shellcheck disable=SC1091
. /etc/os-release
[[ "${ID:-}" == ubuntu ]] ||
  die "this is ${PRETTY_NAME:-not Ubuntu}; the script supports Ubuntu 24.04 and 22.04 (elsewhere install LIB_PKGS and TOOLS from this file by hand)"
legacy=0
case "${VERSION_ID:-}" in
  24.04) ;;
  22.04) legacy=1 ;;
  *) echo "setup-ubuntu: Ubuntu ${VERSION_ID:-?} is untested (24.04 and 22.04 are); treating it as 24.04" >&2 ;;
esac

# The first dotted version in a tool's own report of itself; empty when it is not on PATH.
version_of() {
  command -v "$1" >/dev/null || return 0
  local out
  case "$1" in
    gcc | g++) out="$("$1" -dumpfullversion 2>/dev/null)" ;;
    *) out="$("$1" --version 2>/dev/null | head -n 3)" ;;
  esac
  grep -oE '[0-9]+(\.[0-9]+)+' <<<"$out" | head -n 1
}

# 0 when the tool on PATH satisfies its row.
tool_ok() {  # command minimum [exact]
  local v
  v="$(version_of "$1")"
  [[ -n "$v" ]] || return 1
  if [[ "${3:-}" == exact ]]; then
    [[ "${v%%.*}" == "${2%%.*}" ]]
  else
    printf '%s\n%s\n' "$2" "$v" | sort -V -C
  fi
}

# What is missing or too old, as apt packages; and why, for the report.
scan() {
  want=()
  why=()
  local installed p row cmd min p24 p22 exact pkg
  # dpkg-query fails on a package it has never heard of; what it printed is still the answer.
  installed=" $({ dpkg-query -W -f='${db:Status-Status} ${Package}\n' "${LIB_PKGS[@]}" 2>/dev/null || true; } |
    awk '$1 == "installed" { sub(/:.*/, "", $2); printf "%s ", $2 }') "
  for p in "${LIB_PKGS[@]}"; do
    [[ "$installed" == *" $p "* ]] || { want+=("$p"); why+=("$p (not installed)"); }
  done
  for row in "${TOOLS[@]}"; do
    read -r cmd min p24 p22 exact <<<"$row"
    tool_ok "$cmd" "$min" "$exact" && continue
    pkg="$p24"
    ((legacy)) && pkg="$p22"
    [[ " ${want[*]} " == *" $pkg "* ]] || want+=("$pkg")
    local have
    have="$(version_of "$cmd")"
    why+=("$cmd ${have:-missing}, needs ${exact:+exactly }$min")
  done
}

scan

if [[ "$mode" == list ]]; then
  for row in "${TOOLS[@]}"; do
    read -r cmd min p24 p22 exact <<<"$row"
    ((legacy)) && echo "$p22" || echo "$p24"
  done | awk '!seen[$0]++'
  printf '%s\n' "${LIB_PKGS[@]}"
  exit 0
fi

if [[ "$mode" == check ]]; then
  if ((${#want[@]})); then
    printf 'setup-ubuntu: %s\n' "${why[@]}"
    echo "setup-ubuntu: run sudo tools/setup-ubuntu.sh to install: ${want[*]}"
    exit 1
  fi
  say "setup-ubuntu: up to date (${#LIB_PKGS[@]} libraries, ${#TOOLS[@]} tools)"
  exit 0
fi

if ((${#want[@]})); then
  printf 'setup-ubuntu: %s\n' "${why[@]}"
  SUDO=()
  if [[ "$(id -u)" != 0 ]]; then
    command -v sudo >/dev/null || die "not root and no sudo; rerun as root"
    SUDO=(sudo)
  fi
  cache_opt=()
  if [[ -n "$cache_dir" ]]; then
    # apt-get keeps what it downloads there, and takes a .deb already there instead of fetching
    # it again when its checksum matches the package lists.
    # Made as the user who ran sudo: a root-owned parent (CI's .cache/) would lock that user out
    # of everything else under it.
    if [[ -n "${SUDO_UID:-}" ]]; then
      sudo -u "#$SUDO_UID" mkdir -p "$cache_dir"
    else
      mkdir -p "$cache_dir"
    fi
    "${SUDO[@]}" mkdir -p "$cache_dir/partial"
    cache_dir="$(cd "$cache_dir" && pwd)"
    cache_opt=(-o "Dir::Cache::Archives=$cache_dir/" -o APT::Keep-Downloaded-Packages=true)
  fi
  apt() {
    "${SUDO[@]}" env DEBIAN_FRONTEND=noninteractive apt-get -o Acquire::Retries=3 \
      -o Acquire::http::Timeout=30 -o Dpkg::Use-Pty=0 "${cache_opt[@]}" "$@"
  }
  if ((https_mirrors)); then
    for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources; do
      [[ -f "$f" ]] && grep -qE 'http://(archive|security|[a-z]+\.archive)\.ubuntu\.com' "$f" || continue
      echo "== https mirrors: $f"
      "${SUDO[@]}" sed -i -E 's#http://((archive|security|[a-z]+\.archive)\.ubuntu\.com)#https://\1#g' "$f"
    done
  fi
  apt -qq update

  if ((legacy)); then
    # 22.04's extra sources, each added only when a package wanted needs it, keyring and list
    # written once, so a rerun adds nothing.
    added=0
    add_source() {  # name key-url line
      local key="/etc/apt/keyrings/$1.gpg" list="/etc/apt/sources.list.d/$1.list"
      [[ -f "$list" ]] && return 0
      echo "== apt source: $1"
      "${SUDO[@]}" install -d -m 0755 /etc/apt/keyrings
      curl -fsSL "$2" | gpg --dearmor | "${SUDO[@]}" tee "$key" >/dev/null
      echo "deb [signed-by=$key] $3" | "${SUDO[@]}" tee "$list" >/dev/null
      added=1
    }
    w=" ${want[*]} "
    apt install -y -qq --no-install-recommends ca-certificates curl gnupg software-properties-common
    [[ "$w" == *" cmake "* ]] && add_source kitware https://apt.kitware.com/keys/kitware-archive-latest.asc \
      "https://apt.kitware.com/ubuntu/ jammy main"
    [[ "$w" == *-18" "* ]] && add_source llvm-18 https://apt.llvm.org/llvm-snapshot.gpg.key \
      "https://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main"
    [[ "$w" == *" nodejs "* ]] && add_source nodesource https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key \
      "https://deb.nodesource.com/node_22.x nodistro main"
    [[ "$w" == *" gh "* ]] && add_source github-cli https://cli.github.com/packages/githubcli-archive-keyring.gpg \
      "https://cli.github.com/packages stable main"
    if [[ "$w" == *-13" "* ]] && ! grep -rqs ubuntu-toolchain-r /etc/apt/sources.list.d/; then
      echo "== apt source: ppa:ubuntu-toolchain-r/test"
      "${SUDO[@]}" add-apt-repository -y -n ppa:ubuntu-toolchain-r/test >/dev/null
      added=1
    fi
    if ((added)); then apt -qq update; fi
  fi

  echo "== apt-get install ${want[*]}"
  # `install` also upgrades a package already installed to the newest version the sources have.
  apt install -y -qq --no-install-recommends "${want[@]}"
  if [[ -n "$cache_dir" ]]; then
    # Drop the .debs no source offers any more, and hand the rest to the user the cache is saved
    # as (apt wrote them as root).
    apt -qq autoclean
    "${SUDO[@]}" rm -rf "$cache_dir/partial" "$cache_dir/lock"
    "${SUDO[@]}" chown -R "${SUDO_UID:-$(id -u)}:${SUDO_GID:-$(id -g)}" "$cache_dir"
  fi

  if ((legacy)); then
    # The unversioned names CMake, the Makefile and tools/lint.sh call, at the highest priority.
    for pair in gcc:gcc-13 g++:g++-13 clang:clang-18 clang++:clang++-18 \
      clang-format:clang-format-18 clang-tidy:clang-tidy-18 ld.lld:ld.lld-18; do
      [[ -x "/usr/bin/${pair#*:}" ]] || continue
      "${SUDO[@]}" update-alternatives --install "/usr/bin/${pair%%:*}" "${pair%%:*}" \
        "/usr/bin/${pair#*:}" 1000 >/dev/null
    done
  fi
  hash -r

  scan
  if ((${#want[@]})); then
    for row in "${TOOLS[@]}"; do
      read -r cmd min p24 p22 exact <<<"$row"
      tool_ok "$cmd" "$min" "$exact" ||
        echo "setup-ubuntu: $cmd is still $(version_of "$cmd") at $(command -v "$cmd" || echo '(not on PATH)'); another copy earlier on PATH shadows the package's, remove it or put /usr/bin first" >&2
    done
    die "still missing or too old after installing: ${why[*]}"
  fi
  echo "setup-ubuntu: up to date"
else
  say "setup-ubuntu: up to date (${#LIB_PKGS[@]} libraries, ${#TOOLS[@]} tools); nothing to do"
fi

if [[ "$mode" == verify ]]; then
  # Build as the user who ran sudo, so build/ and .cache/ are not owned by root.
  as_user=()
  if [[ "$(id -u)" == 0 && -n "${SUDO_USER:-}" && "${SUDO_USER}" != root ]]; then
    as_user=(sudo -u "$SUDO_USER" -H --preserve-env=PATH,HTTPS_PROXY,HTTP_PROXY,NO_PROXY)
  fi
  echo "== make build, then the whole suite as CI's Linux x64 row (a GPU or window skip fails)"
  "${as_user[@]}" make -C "$ROOT" build
  "${as_user[@]}" env GE_GPU_REQUIRE=1 xvfb-run -a make -C "$ROOT" test
  echo "setup-ubuntu: verified"
fi

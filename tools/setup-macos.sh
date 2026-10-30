#!/usr/bin/env bash
# Brings a Mac, from a fresh install, up to what the tree needs: builds the engine and Fall Line
# (debug with ASan/UBSan, or release), runs `make precheck`, `make test`, `make lint` and every lab,
# the GPU tests on the Mac's own Metal adapter. The one list of macOS tools, as
# tools/setup-ubuntu.sh is Linux's.
#
#   tools/setup-macos.sh             install or upgrade what is missing, broken or too old
#   tools/setup-macos.sh --check     say what is missing, broken or too old, change nothing (exit 1)
#   tools/setup-macos.sh --list      print the Homebrew formulas it asks for, one per line
#   --quiet                          say nothing when there is nothing to do
#   tools/setup-macos.sh --verify    install, then build and run the whole suite on the debug
#                                    preset (GE_GPU_REQUIRE=1: a GPU test that skips fails)
#
# RUN IT AS YOURSELF, NEVER WITH SUDO: Homebrew refuses root. It asks for your password once, when
# it installs Homebrew. It runs on the /bin/bash 3.2 every Mac has, since bash 4 is one of the
# things it installs.
#
# On a fresh Mac, in order:
#   1. Homebrew, by its official installer (https://brew.sh), which also installs Xcode's Command
#      Line Tools without Apple's dialog: Apple Clang (the compiler CMake picks as `c++`, the one
#      CI's macOS row builds with), the macOS SDK and its Metal framework, make and git. When
#      Homebrew is already there but the Command Line Tools are not, Apple's installer is opened
#      and the script asks to be run again once it is done;
#   2. Homebrew put on PATH for every new shell (`brew shellenv` in ~/.zprofile, or
#      ~/.bash_profile for a bash login shell), so its bash, python3 and the rest come before the
#      system's;
#   3. from Homebrew: CMake 3.25+, Ninja (the presets' generator), ccache, bash 4+ (tools/*.sh use
#      mapfile), clang-format and clang-tidy 18 (CI's major: another clang-format formats some lines
#      differently and fails fmt-check), Node.js 18+ (tools/*.mjs), Python 3.10+ (tools/release.py;
#      the Command Line Tools' python3 is older) and gh; nasm on an Intel Mac only (the AV1
#      encoder's x86 assembly). clang-format and clang-tidy 18 come from the keg-only llvm@18
#      formula; the script links only those two commands into Homebrew's bin, so Apple Clang stays
#      the compiler.
# SDL3, Dawn, libaom and libopus are not formulas here: CMake downloads them, pinned and
# checksummed, into .cache/deps/ on the first configure. Blender (`ge asset`) is optional and not
# installed here (GE_BLENDER, the `assets` skill).
#
# IDEMPOTENT AND FAST WHEN DONE. A tool counts as present when the command on PATH runs and
# reports at least the minimum, wherever it came from; one that is on PATH but fails to run (a
# Homebrew bottle whose library was upgraded under it) counts as broken and is reinstalled. Only
# what is missing, broken or too old is touched; when nothing is, the script calls neither brew nor
# the network and returns in well under a second.
#
# ge:summary Takes a freshly installed Mac to one that builds and tests the tree: Homebrew,
# ge:summary the Command Line Tools and every tool; a no-op when all are there
# ge:keywords setup macos homebrew brew dependencies install toolchain bootstrap idempotent
# ge:keywords apple-clang xcode command-line-tools metal llvm clang-format fresh-install
# ge:skill build
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

# `command minimum formula [exact]`. Present when the command on PATH runs and reports at least the
# minimum (`exact`: that major and no other). Formula `-` comes with the Command Line Tools.
TOOLS=(
  "make 3.81 -"
  "git 2.25 -"
  "cmake 3.25 cmake"
  "ninja 1.10 ninja"
  "ccache 4.0 ccache"
  "bash 4.0 bash"
  "clang-format 18 llvm@18 exact"
  "clang-tidy 18 llvm@18 exact"
  "python3 3.10 python"
  "node 18 node"
  "gh 2.0 gh"
)
[[ "$(uname -m)" == x86_64 ]] && TOOLS+=("nasm 2.13 nasm")
# The commands llvm@18 brings that go on PATH; its clang and clang++ stay in the keg.
LLVM_LINKS=(clang-format clang-tidy)
# CI's macos-15 image builds with Xcode 16's Apple Clang.
APPLE_CLANG_MIN=16
BREW_INSTALLER=https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh

mode=install
quiet=0
for arg in "$@"; do
  case "$arg" in
    --check) mode=check ;;
    --list) mode=list ;;
    --verify) mode=verify ;;
    --quiet) quiet=1 ;;
    -h | --help) sed -n '2,/^# ge:summary/{/^# ge:summary/d;s/^# \{0,1\}//;p;}' "$0"; exit 0 ;;
    *) echo "setup-macos: unknown flag '$arg'; use --check, --list, --verify, --quiet or none (--help)" >&2; exit 2 ;;
  esac
done

say() { ((quiet)) || echo "$@"; }
die() { echo "setup-macos: $*" >&2; exit 1; }

[[ "$(uname -s)" == Darwin ]] || die "this is $(uname -s), not macOS; on Ubuntu run sudo tools/setup-ubuntu.sh"
[[ "$(id -u)" != 0 ]] || die "run as yourself, not root or sudo: Homebrew refuses root"

if [[ "$mode" == list ]]; then
  for row in "${TOOLS[@]}"; do
    read -r cmd min formula exact <<<"$row"
    [[ "$formula" == - ]] || echo "$formula"
  done | awk '!seen[$0]++'
  exit 0
fi

# Homebrew, where its installer puts it (Apple silicon, then Intel), even when this shell's PATH
# lacks it, as it does on the first run.
find_brew() {
  BREW=""
  local b
  for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do
    [[ -x "$b" ]] && { BREW="$b"; break; }
  done
  [[ -n "$BREW" ]] || return 0
  BREW_PREFIX="${BREW%/bin/brew}"
  case ":$PATH:" in *":$BREW_PREFIX/bin:"*) ;; *) PATH="$BREW_PREFIX/bin:$BREW_PREFIX/sbin:$PATH" ;; esac
}
find_brew

# The Command Line Tools (or Xcode). Until they are installed, /usr/bin/git, make, c++ and python3
# are stubs that open Apple's installer when run, so nothing runs them before this says yes.
clt_ok() { xcode-select -p >/dev/null 2>&1 && xcrun --show-sdk-path >/dev/null 2>&1; }
apple_clang_major() {
  /usr/bin/c++ --version 2>/dev/null | sed -n 's/^Apple clang version \([0-9]*\).*/\1/p' | head -n 1
}

# The first dotted version in a tool's own report of itself; empty when it is not on PATH or fails
# to run.
version_of() {
  command -v "$1" >/dev/null || return 0
  local out
  out="$("$1" --version 2>/dev/null | head -n 3)" || true
  grep -oE '[0-9]+(\.[0-9]+)+' <<<"$out" | head -n 1 || true
}

# 0 when version $1 is at least $2 (dotted, numeric fields).
ver_ge() {
  local IFS=. i
  local -a a b
  read -r -a a <<<"$1"
  read -r -a b <<<"$2"
  for ((i = 0; i < ${#b[@]}; i++)); do
    ((10#${a[i]:-0} > 10#${b[i]})) && return 0
    ((10#${a[i]:-0} < 10#${b[i]})) && return 1
  done
  return 0
}

# 0 when the tool on PATH satisfies its row.
tool_ok() {  # command minimum [exact]
  local v
  v="$(version_of "$1")"
  [[ -n "$v" ]] || return 1
  if [[ "${3:-}" == exact ]]; then
    [[ "${v%%.*}" == "${2%%.*}" ]]
  else
    ver_ge "$v" "$2"
  fi
}

# The login shell's profile, and whether it puts Homebrew on PATH.
profile_file() {
  case "$(basename "${SHELL:-/bin/zsh}")" in
    bash) echo "$HOME/.bash_profile" ;;
    *) echo "$HOME/.zprofile" ;;
  esac
}
# /usr/local/bin is on a Mac's default PATH; /opt/homebrew/bin is not.
profile_ok() {
  [[ -z "$BREW" || "$BREW_PREFIX" == /usr/local ]] && return 0
  grep -qs 'brew shellenv' "$(profile_file)"
}

# Everything missing, broken or too old: what Apple's tools lack, the formulas to ask Homebrew
# for, and why, for the report.
scan() {
  apple=()
  want=()
  why=()
  local row cmd min formula exact have clt=1
  [[ -n "$BREW" ]] || why+=("Homebrew missing")
  if ! clt_ok; then
    clt=0
    apple+=("Xcode's Command Line Tools missing")
  else
    have="$(apple_clang_major)"
    [[ -n "$have" ]] && ((have >= APPLE_CLANG_MIN)) ||
      apple+=("Apple Clang ${have:-missing}, needs $APPLE_CLANG_MIN+ (update Xcode or the Command Line Tools in System Settings > Software Update)")
  fi
  profile_ok || why+=("$(profile_file) does not put Homebrew on PATH")
  for row in "${TOOLS[@]}"; do
    read -r cmd min formula exact <<<"$row"
    if [[ "$formula" == - ]]; then
      ((clt)) || continue  # a stub until the Command Line Tools are in; counted above
      tool_ok "$cmd" "$min" && continue
      apple+=("$cmd $(version_of "$cmd"), needs $min (it comes with the Command Line Tools; update them)")
      continue
    fi
    # Before the Command Line Tools, /usr/bin/python3 is a stub: never run it.
    if ((clt)) || [[ "$(command -v "$cmd" || true)" != /usr/bin/* ]]; then
      tool_ok "$cmd" "$min" "$exact" && continue
      have="$(version_of "$cmd")"
      [[ -z "$have" ]] && command -v "$cmd" >/dev/null && have="broken (at $(command -v "$cmd"), fails to run)"
    else
      have=""
    fi
    [[ " ${want[*]-} " == *" $formula "* ]] || want+=("$formula")
    why+=("$cmd ${have:-missing}, needs ${exact:+exactly }$min")
  done
}

# A compiler named by CXX that is not Apple Clang builds the tree untested; say so, change nothing.
warn_cxx() {
  [[ -n "${CXX:-}" ]] || return 0
  "$CXX" --version 2>/dev/null | head -n 1 | grep -q '^Apple clang' && return 0
  echo "setup-macos: CXX=$CXX is not Apple Clang ($("$CXX" --version 2>/dev/null | head -n 1)); CMake builds with it, and only Apple Clang is tested on macOS; unset CXX and delete build/ to use /usr/bin/c++" >&2
}

scan

if [[ "$mode" == check ]]; then
  if ((${#apple[@]} + ${#why[@]})); then
    ((${#apple[@]})) && printf 'setup-macos: %s\n' "${apple[@]}"
    ((${#why[@]})) && printf 'setup-macos: %s\n' "${why[@]}"
    echo "setup-macos: run tools/setup-macos.sh to install them"
    warn_cxx
    exit 1
  fi
  say "setup-macos: up to date (Apple Clang $(apple_clang_major), ${#TOOLS[@]} tools)"
  warn_cxx
  exit 0
fi

if ((${#apple[@]} + ${#why[@]} == 0)); then
  say "setup-macos: up to date (Apple Clang $(apple_clang_major), ${#TOOLS[@]} tools); nothing to do"
else
  ((${#apple[@]})) && printf 'setup-macos: %s\n' "${apple[@]}"
  ((${#why[@]})) && printf 'setup-macos: %s\n' "${why[@]}"

  # 1. Homebrew, and with it the Command Line Tools.
  if [[ -z "$BREW" ]]; then
    echo "== Homebrew's installer ($BREW_INSTALLER); it asks for your password"
    sudo -v || die "Homebrew's installer needs an administrator's password; run tools/setup-macos.sh again from an admin account"
    NONINTERACTIVE=1 /bin/bash -c "$(curl -fsSL "$BREW_INSTALLER")" ||
      die "Homebrew's installer failed (its output is above); fix what it names, then run tools/setup-macos.sh again"
    find_brew
    [[ -n "$BREW" ]] || die "Homebrew's installer finished but no brew is in /opt/homebrew or /usr/local; see https://docs.brew.sh/Installation"
  fi
  if ! clt_ok; then
    # Apple's installer is a dialog the person clicks through; nothing here can wait on it.
    xcode-select --install 2>/dev/null || true
    die "Apple's installer for the Command Line Tools is open (or run xcode-select --install); finish it, then run tools/setup-macos.sh again"
  fi
  scan
  if ((${#apple[@]})); then
    printf 'setup-macos: %s\n' "${apple[@]}"
    die "fix the Apple tools above, then run tools/setup-macos.sh again"
  fi

  # 2. Homebrew on PATH in every new shell.
  if ! profile_ok; then
    profile="$(profile_file)"
    echo "== $profile: eval \"\$($BREW shellenv)\""
    # shellcheck disable=SC2016  # the $(...) is for the profile to run, not this script
    printf '\n# Homebrew on PATH (tools/setup-macos.sh)\neval "$(%s shellenv)"\n' "$BREW" >>"$profile"
  fi

  # 3. The formulas: one Homebrew lacks is installed, one it has but outdated is upgraded, and one
  # it has and calls current is still broken, so it is reinstalled.
  if ((${#want[@]})); then
    export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 HOMEBREW_NO_ENV_HINTS=1
    "$BREW" update --quiet
    outdated=" $("$BREW" outdated --formula --quiet 2>/dev/null | sed 's#.*/##' | tr '\n' ' ' || true) "
    fresh=()
    stale=()
    again=()
    for f in "${want[@]}"; do
      if ! "$BREW" list --versions "$f" >/dev/null 2>&1; then fresh+=("$f")
      elif [[ "$outdated" == *" $f "* ]]; then stale+=("$f")
      else again+=("$f"); fi
    done
    if ((${#fresh[@]})); then echo "== brew install ${fresh[*]}"; "$BREW" install --quiet "${fresh[@]}"; fi
    if ((${#stale[@]})); then echo "== brew upgrade ${stale[*]}"; "$BREW" upgrade --quiet "${stale[@]}"; fi
    if ((${#again[@]})); then echo "== brew reinstall ${again[*]}"; "$BREW" reinstall --quiet "${again[@]}"; fi
  fi

  # llvm@18's two commands onto PATH, never over a name another formula or the person put there.
  if "$BREW" list --versions llvm@18 >/dev/null 2>&1; then
    keg="$BREW_PREFIX/opt/llvm@18/bin"
    for c in "${LLVM_LINKS[@]}"; do
      link="$BREW_PREFIX/bin/$c"
      [[ -L "$link" && "$(readlink "$link")" == *"/llvm@18/"* ]] && continue
      if [[ -e "$link" || -L "$link" ]]; then
        die "$link is already $(readlink "$link" || echo 'a file'), not llvm@18's $c; remove it (brew unlink the formula that owns it), then run tools/setup-macos.sh again"
      fi
      echo "== link $link -> $keg/$c"
      ln -s "$keg/$c" "$link"
    done
  fi
  hash -r

  scan
  if ((${#apple[@]} + ${#why[@]})); then
    for row in "${TOOLS[@]}"; do
      read -r cmd min formula exact <<<"$row"
      tool_ok "$cmd" "$min" "$exact" && continue
      have="$(version_of "$cmd")"
      echo "setup-macos: $cmd is still ${have:-missing or broken} at $(command -v "$cmd" || echo '(not on PATH)'); a copy earlier on PATH shadows Homebrew's, remove it or put $BREW_PREFIX/bin first" >&2
    done
    die "still missing, broken or too old after installing: ${apple[*]-} ${why[*]-}"
  fi
  echo "setup-macos: up to date; open a new terminal (or run: eval \"\$($BREW shellenv)\") so this shell finds Homebrew's tools"
fi
warn_cxx

if [[ "$mode" == verify ]]; then
  echo "== make build, then the whole suite on the Mac's Metal adapter (a GPU test that skips fails)"
  make -C "$ROOT" build
  GE_GPU_REQUIRE=1 make -C "$ROOT" test
  echo "setup-macos: verified"
fi

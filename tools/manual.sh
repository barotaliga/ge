#!/usr/bin/env bash
# THE PLAYER'S REFERENCE MANUAL (`make manual`, docs/console.md § The reference manual): every
# setting, command and launch flag of Shooter's Edge's window and its dedicated server, written
# from the programs' own registrations, never a hand-kept list:
#
#   1. sesv --manual=<file>              the dedicated server's console, once per game it serves
#                                         (QuakeWorld, then Quake II with -classic q2), offline;
#   2. se -hidden -manual <file>         the window's console once everything is registered, every
#                                         game's own layer loaded in turn (client/manual.hpp);
#   3. ge manual <sources> --out=...     the sources merged into one searchable page with filters
#                                         by game, kind, layer and program, and manual.json beside it.
#
# The window opens hidden, so it needs a GPU adapter (lavapipe draws on the CPU in a container;
# with no display, xvfb-run gives it one) and a game to load: GE_MANUAL_GAME is the source the
# classicenhance tool lays out as one (`setup`: a folder, a release zip or its https URL, refused
# unless it is a known release), by default Quake's shareware release, fetched once into the
# output's cache. Every other game's settings come without its files: the window loads each game's
# own layer in turn. MANUAL_OUT (default build/manual) is where the page goes; PRESET (default
# debug) the build used.
#
# ge:summary make manual: the player's reference manual from the window's and server's sources
# ge:keywords manual reference-manual user-manual make-manual release-asset sesv se ge-manual
# ge:skill console agent-interface
set -euo pipefail
cd "$(dirname "$0")/.."

preset="${PRESET:-debug}"
out="${MANUAL_OUT:-build/manual}"
bin="build/${preset}"
src="${out}/sources"

tools/configure.sh "${preset}" >/dev/null 2>&1 || cmake --preset "${preset}" >/dev/null
cmake --build "${bin}" --target ge shooters_edge shooters_edge_dedicated classicenhance

rm -rf "${src}"
mkdir -p "${src}"
# A home of its own: the window's settings and installs a player has are never read or written.
home="$(mktemp -d)"
trap 'rm -rf "${home}"' EXIT

"${bin}/sesv" --offline --root="${home}" --manual="${src}/sesv-qw.txt"
# No game's files needed: the server writes what its console registers even when no map opens.
"${bin}/sesv" -classic q2 --offline --root="${home}" --manual="${src}/sesv-q2.txt" 2>/dev/null
game="${GE_MANUAL_GAME:-https://archive.org/download/quakeshareware/QUAKE_SW.zip}"
"${bin}/classicenhance" setup "${game}" --basedir="${home}/game" --cache="${out}/cache" >/dev/null
display=()
if [ -z "${DISPLAY:-}" ] && [ "$(uname -s)" = Linux ] && command -v xvfb-run >/dev/null; then
  display=(xvfb-run -a)
fi
# No sound device: the console is all it reads.
HOME="${home}" XDG_DATA_HOME="${home}" SDL_AUDIO_DRIVER=dummy ${display[@]+"${display[@]}"} "${bin}/se" -hidden \
  -config "${home}/config.cfg" -classic q1 -basedir "${home}/game" -manual "${src}/se.txt"

version="$(cat VERSION 2>/dev/null || echo dev)"
"${bin}/ge" manual "${src}/se.txt" "${src}/sesv-qw.txt" "${src}/sesv-q2.txt" --out="${out}/user-manual.html" \
  --title="Shooter's Edge reference manual" \
  --intro="Every setting, console command and launch flag of the game and its dedicated server, per game." \
  --version="${version} ($(git rev-parse --short HEAD 2>/dev/null || echo local))"
# A static host serves the folder's index.html first: the same page.
cp "${out}/user-manual.html" "${out}/index.html"
echo "manual: ${out}/user-manual.html"

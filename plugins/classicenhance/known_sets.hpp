// THE KNOWN FILE SETS: a release named by the files the packs hold, not by the packs' own bytes.
// A player may keep a release's files in other packs than it shipped in: the full game's two
// packs repackaged into one `pak0.pak` (or a content zip, `pak0.pk3`), often with files of their
// own added beside. A set lists every file of one release (its path, its size and the first 128
// bits of its SHA-256: ample to tell a changed file, and a row fits a line); the packs are that
// release when, read merged (`Source::content`), they hold every one of its files unchanged.
// Files the set does not list are ignored, so a repack with more files in it still matches, and
// the packs as shipped match too, since together they hold the same files.
//
// - `the full game of release 1.06`: its first pack's files (the shareware pack's) and its
//   second's.
// - `the remastered release`: the paths of the full game's files as the 2021 remaster holds them
//   (its maps rebuilt, its sounds remade, its default config gone). The remaster's own new files
//   are not listed.
//
// One file of each is listed by its contents alone (a null path): the startup script, whose file
// name is one this tree does not spell (`make agents-check`). It matches a file of its size and
// digest at any path.
//
// A set marked `remade` holds its own remade assets: the pipeline leaves its sounds and models as
// the packs hold them and makes its textures' maps (height, normal, roughness, glow) from the
// pack's own pixels at their own size, upscaling nothing (pipeline.hpp).
//
// `ge tool classicenhance -- manifest <path>` prints a source's files as rows of a set.
//
// ge:summary The known file sets: a release found by every file the packs hold, extras ignored
// ge:keywords known-set manifest full-game registered repack single-pack pak0 pk3 remaster
// ge:keywords remade verify sha256 file-digest match_set extra-files
// ge:skill classicenhance
// ge:test tests/classicenhance_source_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::classicenhance {

struct KnownEntry {
  const char* path;    // as the packs store it; nullptr: matched by its contents at any path
  uint32_t size;       // bytes
  const char* sha128;  // the first 32 lower-case hex digits of its SHA-256
};

struct KnownSet {
  const char* id;    // as a message or the tool's JSON names it
  const char* what;  // in the engine's words, for the report
  std::span<const KnownEntry> files;
  bool remade = false;  // its assets are already remade: the pipeline enhances no sound or model
};

// The sets the source search matches, in order (known_files.cpp holds their rows).
extern const std::span<const KnownSet> kKnownSets;

// How a pack's files match a set.
struct SetMatch {
  const KnownSet* set = nullptr;              // the set matched whole, or the nearest when none is
  size_t held = 0;                            // its files the pack holds unchanged
  std::vector<std::string> missing, changed;  // its other files, by path ("a file of N bytes")
  bool whole() const { return set != nullptr && missing.empty() && changed.empty(); }
};

// The first of `sets` whose every file the pack `content` holds unchanged; when none, the one it
// holds most of (`set` null when the pack does not read or `sets` is empty).
SetMatch match_sets(std::span<const uint8_t> content, std::span<const KnownSet> sets);

// The first 32 hex digits of `bytes`' SHA-256, as a set's rows hold them.
std::string sha128_hex(std::span<const uint8_t> bytes);

}  // namespace ge::classicenhance

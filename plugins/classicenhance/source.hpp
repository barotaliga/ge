// WHERE THE CLASSIC CONTENT COMES FROM, AND WHICH RELEASE IT IS. The pipeline reads the player's
// own classic packs: the shareware pack (`pak0.pak`), which its licence lets anyone keep and pass
// on unchanged, or the full game the player owns. Everything the pipeline derives from them stays
// in the player's local cache (cache.hpp); nothing read or derived is committed or shipped.
//
// `open_source` takes a path the player gives and finds the pack in it:
//
// - **a release zip** (the shareware distribution as published): the zip holds the installer's
//   self-extracting LHA archive (`resource.1`), and the archive holds `id1\pak0.pak`. Both are
//   unpacked in memory; nothing is installed or run. A zip whose entries hold the pack directly
//   is read the same way.
// - **the installer's archive** taken out of the zip by hand (`resource.1`), read the same way.
// - **a pack file**: a `pak0.pak` from an installed game.
// - **a folder**: an installed game's folder (its `id1/pak0.pak`, names matched in any case), or
//   a folder holding a release zip or the pack itself.
//
// **The full game.** An installed folder may hold later packs beside the first (`pak1.pak`,
// `pak2.pak` ... in any case, read in order until one is missing), as the registered game's does.
// Each must read as a pack; a later pack's file of the same path is the one the game uses, so the
// pipeline reads them merged (`Source::content`): every file of every pack, the latest copy of a
// path winning. A later pack whose digest is in `kKnownPacks` is reported as that release; any
// other is accepted as the player's own copy and reported unverified. Assets are found by their
// path and kind (maps, sounds, models), never by a list of one release's files, so whatever the
// packs hold is enhanced.
//
// Whichever route, the first pack must read as a pack, or it is refused with its digest and
// nothing is written. Its SHA-256 is looked up in `kKnownPacks`: a known release is reported as
// that release, any other (the full game, another release, the player's own copy) is accepted and
// reported unverified. A release zip whose own digest is in `kKnownZips` is reported as such;
// another zip holding a pack is accepted for the pack.
//
// ge:summary Finds the classic packs in a zip, LHA archive, file or folder, verified by SHA-256
// ge:keywords source pack pak0 pak1 shareware registered release zip lha folder installed
// ge:keywords verify sha256 known unchanged licence refuse identity merge layered later-pack
// ge:skill classicenhance
// ge:test tests/classicenhance_source_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::classicenhance {

struct KnownFile {
  const char* sha256;  // lower-case hex
  uint32_t size;
  const char* what;
};

// The unchanged packs the pipeline reads.
inline constexpr KnownFile kKnownPacks[] = {
    {"35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af", 18689235,
     "the shareware pack of release 1.06 (the full release 1.06 installs the same pack)"},
};
// The release zips that carry one of them, as published.
inline constexpr KnownFile kKnownZips[] = {
    {"ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239", 9094045,
     "the shareware release 1.06 zip"},
};

enum class Route : uint8_t { release_zip, installer, pack_file, folder };
const char* route_name(Route r);

// A pack after the first in an installed folder (`pak1.pak` ...).
struct LaterPack {
  std::string name;  // as the folder spells it
  std::string path;
  std::string sha256;
  const KnownFile* known = nullptr;  // a known release's pack, or nullptr: the player's own copy
  std::vector<uint8_t> bytes;
};

struct Source {
  Route route = Route::pack_file;
  std::string path;                // the file the pack was read from (a zip or the pack itself)
  std::string zip_sha256;          // the zip's digest, when the route went through one
  const KnownFile* zip = nullptr;  // the zip as a known release, or nullptr
  std::vector<uint8_t> pack;       // the pack's bytes, unchanged
  std::string pack_sha256;
  const KnownFile* known = nullptr;  // the known pack it is
  std::vector<LaterPack> later;      // the later packs beside it, in order
  std::vector<uint8_t> merged;       // every pack's files, a later copy winning; empty with none
  std::string set_sha256;            // the cache's key: the pack's digest, or every pack's hashed
  // What the pipeline reads: the first pack alone, or every pack merged.
  const std::vector<uint8_t>& content() const { return merged.empty() ? pack : merged; }
};

// The pack at `path` (a zip, a pack, or a folder holding either), verified against `packs`;
// false with the reason, and the next step for the player, in `*error`. (Tests pass their own
// fixture's digest as `packs`; everything else keeps the default.)
bool open_source(const std::string& path, Source* out, std::string* error,
                 std::span<const KnownFile> packs = kKnownPacks);
// The pack inside a release zip's bytes (through its LHA archive when it has one), verified.
bool pack_from_zip(std::span<const uint8_t> zip, Source* out, std::string* error,
                   std::span<const KnownFile> packs = kKnownPacks);
// The later packs in `dir` (pak1.pak, pak2.pak ... in any case) read into `out->later`, and
// `out->merged` and `out->set_sha256` set; false when one does not read as a pack.
bool add_later_packs(const std::string& dir, Source* out, std::string* error,
                     std::span<const KnownFile> packs = kKnownPacks);

// Whether `out->pack` reads as a pack; sets `out->pack_sha256`, and `out->known` when it is one
// of `packs` (nullptr: the player's own copy, unverified).
bool verify_pack(Source* out, std::string* error, std::span<const KnownFile> packs = kKnownPacks);

}  // namespace ge::classicenhance

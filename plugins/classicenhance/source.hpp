// WHERE THE CLASSIC CONTENT COMES FROM, AND THE PROOF IT IS UNCHANGED. The pipeline reads one
// thing only: the classic shareware pack (`pak0.pak`) exactly as it was released, which its
// licence lets anyone keep and pass on unchanged. Everything the pipeline derives from it stays
// in the player's local cache (cache.hpp); nothing derived is committed or shipped.
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
// Whichever route, the pack's SHA-256 must be one of `kKnownPacks`: an unknown pack (another
// release, a modified or damaged copy) is refused with its digest, and nothing is written. A
// release zip whose own digest is in `kKnownZips` is reported as such; another zip holding a
// known pack is accepted for the pack.
//
// ge:summary Finds the classic shareware pack in a zip, LHA archive, file or folder; known hashes
// only ge:keywords source pack pak0 shareware release zip lha folder installed verify sha256 known
// ge:keywords unchanged licence refuse identity
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

struct Source {
  Route route = Route::pack_file;
  std::string path;                // the file the pack was read from (a zip or the pack itself)
  std::string zip_sha256;          // the zip's digest, when the route went through one
  const KnownFile* zip = nullptr;  // the zip as a known release, or nullptr
  std::vector<uint8_t> pack;       // the pack's bytes, unchanged
  std::string pack_sha256;
  const KnownFile* known = nullptr;  // the known pack it is
};

// The pack at `path` (a zip, a pack, or a folder holding either), verified against `packs`;
// false with the reason, and the next step for the player, in `*error`. (Tests pass their own
// fixture's digest as `packs`; everything else keeps the default.)
bool open_source(const std::string& path, Source* out, std::string* error,
                 std::span<const KnownFile> packs = kKnownPacks);
// The pack inside a release zip's bytes (through its LHA archive when it has one), verified.
bool pack_from_zip(std::span<const uint8_t> zip, Source* out, std::string* error,
                   std::span<const KnownFile> packs = kKnownPacks);
// Whether `out->pack` is one of `packs`; sets `out->pack_sha256` and `out->known`.
bool verify_pack(Source* out, std::string* error, std::span<const KnownFile> packs = kKnownPacks);

}  // namespace ge::classicenhance

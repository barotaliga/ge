// A MOD PACKAGE'S MANIFEST (`mod.manifest`, docs/mods.md § The package): what the package is,
// which game and ABI it needs, and its library for each platform with the file's SHA-256. Text,
// one `key = value` a line, `#` comments:
//
//   name = rocket-bounce                 kebab case: the package's name
//   version = 1.0.0                      major.minor.patch
//   game = shooters_edge                 the game id it is for
//   game_api = 1                         the game's hook-table versions it runs on: N or N..M
//   abi = 1.0                            the engine ABI (major.minor) it needs at least
//   file.linux-x64 = rocket_bounce.so 2c26b46b...   per platform: a path beside the manifest, its
//                                                   SHA-256
//
// The platforms: linux-x64, linux-arm64, macos-x64, macos-arm64, windows-x64, windows-arm64
// (`this_platform` names the running one; cmake/Mods.cmake names the one a build is for, and the
// in-tree sample mod's test holds the two to each other). A key this reader does not know is a
// warning, not an error, so a newer manifest still loads where its additions are optional.
//
// ge:summary The mod manifest: its text format, the reader, the checks, the writer, platforms
// ge:keywords mod manifest package mod-manifest name version game game-api abi platform files
// ge:keywords sha256 parse_manifest check_manifest write_manifest this_platform file_for
// ge:skill mod
// ge:test tests/mod_manifest_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::mod {

inline constexpr std::string_view kManifestName = "mod.manifest";

struct ManifestFile {
  std::string platform;  // "linux-x64"
  std::string path;      // relative to the manifest's directory, '/'-separated
  std::string sha256;    // 64 lower-case hex characters
};

struct Manifest {
  std::string name, version, game;
  uint32_t game_api_min = 0, game_api_max = 0;
  uint32_t abi_major = 0, abi_minor = 0;
  std::vector<ManifestFile> files;
};

struct ManifestCheck {
  std::vector<std::string> errors;    // each says what is wrong, where, and what to do
  std::vector<std::string> warnings;  // keys this reader does not know
  bool ok() const { return errors.empty(); }
};

// Reads the text and checks every field (a missing key, a bad name, version, range, platform,
// path or hash, a platform listed twice). `source` names the file in the messages.
ManifestCheck parse_manifest(std::string_view text, std::string_view source, Manifest& out);

// The manifest as text, in the key order above (what `ge mod build` writes).
std::string write_manifest(const Manifest& m);

// The platform this code was compiled for, in the manifest's words ("" when it has none).
std::string_view this_platform();
// Every platform name a manifest may list.
bool known_platform(std::string_view p);

// The file listed for `platform`, or null.
const ManifestFile* file_for(const Manifest& m, std::string_view platform);

}  // namespace ge::mod

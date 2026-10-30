// SHA-256 (FIPS 180-4) of bytes and of a file, as lower-case hex: what a mod package's manifest
// names each file by, so the loader refuses a library that is not the one the package was built
// with (a corrupt download, a stale build beside a new manifest). Integrity, not trust: signing is
// a later row (docs/mods.md § The package).
//
// ge:summary SHA-256 of bytes and of a file as hex, for the mod manifest's file hashes
// ge:keywords mod sha256 sha-256 hash checksum integrity manifest file-hash sha256_hex
// ge:keywords sha256_file
// ge:skill mod
// ge:test tests/mod_manifest_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ge::mod {

// The digest of `n` bytes as 64 lower-case hex characters.
std::string sha256_hex(const void* data, size_t n);
// The digest of a file's bytes, or nothing when it cannot be read.
std::optional<std::string> sha256_file(const std::filesystem::path& path);

}  // namespace ge::mod

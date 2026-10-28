// A ZIP ARCHIVE read from memory, the second container of the virtual file system beside the pack
// (pak.hpp; a content zip renamed .pk3 is the same thing): the end record found from the back,
// the central directory checked whole before anything is read (every entry's local header and
// data inside the file, every method one this reader takes), then an entry's bytes unpacked
// (stored, or deflated through assets/inflate.hpp) with its CRC-32 checked. No zip64, no
// encryption, no spanning; each is refused naming the entry. Entry names are only matched,
// never used as paths on disk.
//
// ge:summary Reads a zip archive from memory: the checked directory and stored or deflated entries
// ge:keywords zip archive central directory deflate stored crc32 entry unpack refuse
// ge:skill vfs
// ge:test tests/vfs_zip_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::vfs {

struct ZipEntry {
  std::string name;  // as stored ('/' or '\\' separators, case kept)
  uint16_t method = 0;  // 0 stored, 8 deflated
  uint32_t crc = 0, packed = 0, size = 0;
  uint32_t data = 0;  // offset of the entry's packed bytes in the archive
};

struct Zip {
  std::vector<ZipEntry> entries;
};

// The directory of the zip in `bytes`; false with the reason in `*error`.
bool read_zip(std::span<const uint8_t> bytes, Zip* out, std::string* error);
// The entry whose name equals `name` ignoring ASCII case and treating '\\' as '/', or nullptr.
const ZipEntry* zip_find(const Zip& zip, std::string_view name);
// An entry's bytes, unpacked and checked against its CRC-32; false with the reason.
bool zip_unpack(std::span<const uint8_t> bytes, const ZipEntry& entry, std::vector<uint8_t>* out,
                std::string* error);

// The CRC-32 a zip entry is checked by.
uint32_t crc32_of(std::span<const uint8_t> bytes);
// A zip of stored entries (name, bytes), for tests and fixtures.
std::vector<uint8_t> write_stored_zip(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

// Names compare the way the archives' own systems did: ASCII case folded, '\\' as '/'.
bool same_name(std::string_view a, std::string_view b);

}  // namespace ge::vfs

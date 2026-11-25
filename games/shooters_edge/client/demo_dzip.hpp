// DZIP ARCHIVES (`.dz`): the archive single-player demos of Quake are shared in, read so
// a downloaded `.dz` plays like the `.dem` inside it. The archive: a 12-byte header ("DZ", the
// major and minor version, the directory's offset and its count), the directory's entries (where
// each file's bytes are, their stored and original sizes, the name, the type, a checksum of the
// original bytes), and each file's bytes as their own zlib stream. A demo is not kept as it was
// recorded: it is first rewritten as a smaller stream (every entity's last values remembered and
// only what changed sent, times and view angles as differences), and `dzip_unpack` writes the
// recording back from that, block for block, its checksum (the complement of the usual CRC-32)
// proving it the recorded bytes.
//
// - WHAT IS READ: major versions 2 and 3 (from 2000 on), ordinary files (a readme beside the demo),
//   stored files, directories (skipped) and demos of protocols 15 and 666, the stream's raw tail
//   where the archiver gave up rewriting. A pack inside the archive and the first version's
//   layout (major 1, mid 2000) are refused in words.
// - THE ONE FILE: as `vfs::unpack_one`, with `pick` the files that count; none or several is
//   refused, naming them.
//
// `dzip_write` is the fixture writer: an archive of files whose bytes are given already in the
// archive's form (a demo's rewritten stream for type 5), each a zlib stream of stored blocks.
//
// ge:summary Dzip archives (.dz): the directory, zlib entries, the demo written back as .dem
// ge:keywords dzip dz archive demo-archive speed-run dem protocol-15 protocol-666 crc32 zlib
// ge:keywords is_dzip dzip_unpack dzip_list dzip_write intermediate-stream baseline entity-model
// ge:skill shooter vfs
// ge:test tests/shooters_edge_demo_dzip_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::client {

// Whether `bytes` open as a Dzip archive ("DZ" and a major version of 1 to 3).
bool is_dzip(std::span<const uint8_t> bytes);

struct DzipEntry {
  std::string name;   // '/' separated
  uint32_t type = 0;  // 0 file, 5 and 6 a demo, 7 a directory, 8 stored, 3 a pack
  uint32_t data = 0, packed = 0, size = 0, middle = 0;  // offset, stored, original, before zlib
  uint32_t check = 0;  // the complement of the original bytes' CRC-32
};

// The archive's entries; false with the reason.
bool dzip_list(std::span<const uint8_t> bytes, std::vector<DzipEntry>& out, std::string& error);
// One entry's original bytes (a demo written back), checked against its checksum.
bool dzip_entry(std::span<const uint8_t> bytes, const DzipEntry& entry, std::vector<uint8_t>& out,
                std::string& error);
// The one file in the archive `pick` accepts (every file without one), its bytes and name.
bool dzip_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, std::string& name,
                 std::string& error, const std::function<bool(std::string_view)>& pick = {});

struct DzipFile {
  std::string name;
  uint32_t type = 0;              // 0 a file, 5 a demo (then `stored` is the rewritten stream)
  std::vector<uint8_t> stored;    // the bytes before zlib
  std::vector<uint8_t> original;  // the file's own bytes, for its size and checksum
};
// An archive of `files` (major version 3), for tests and fixtures.
std::vector<uint8_t> dzip_write(const std::vector<DzipFile>& files);

}  // namespace shooters_edge::client

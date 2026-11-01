// A TAR ARCHIVE read from memory: 512-byte headers (the old form, ustar, and the GNU and pax
// extensions for long names and sizes), each header's checksum checked and each file's data
// inside the archive, the archive ending at two zero blocks or its last byte. Only regular files
// are listed; directories, links and devices are skipped, and names are only matched, never used
// as paths on disk. A packed tar (`.tar.gz`, `.tar.xz`, `.tar.zst`) is unpacked first by
// unpack.hpp, which hands this reader the plain archive.
//
// ge:summary Reads a tar archive from memory: its regular files' names and data; the fixture writer
// ge:keywords tar ustar pax gnu longname archive tarball tgz entry header checksum read_tar
// ge:keywords write_tar
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ge::vfs {

struct TarEntry {
  std::string name;  // as stored, '/' separated
  size_t data = 0;   // offset of the file's bytes in the archive
  size_t size = 0;
};

// Whether the first 512 bytes of `bytes` are a tar header (its checksum holds).
bool is_tar(std::span<const uint8_t> bytes);
// The regular files of the tar in `bytes`; false with the reason (the header's offset) when a
// header's checksum fails or a file runs past the archive.
bool read_tar(std::span<const uint8_t> bytes, std::vector<TarEntry>& out, std::string& error);
// A ustar archive of `files` (name, bytes), for tests and fixtures.
std::vector<uint8_t> write_tar(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

}  // namespace ge::vfs

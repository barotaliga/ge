// RAR ARCHIVES (`.rar`): the archive many downloaded recordings of the classic games come in,
// read through the unRAR library (cmake/Archives.cmake; its licence lets any program extract RAR
// archives and forbids only rebuilding the RAR compressor from it). Both generations open: RAR 4
// (`52 61 72 21 1A 07 00`, the archives of the 2000s) and RAR 5 (`52 61 72 21 1A 07 01 00`).
//
// - THE LIBRARY OPENS A PATH, NOT BYTES: each call writes the archive to a private temporary file,
//   reads it there and deletes it, so the caller still hands over bytes like every other layer.
// - WHAT IS REFUSED, in words: an encrypted archive or file (there is no password to give), a
//   volume of a set (`.part2.rar`, `.r00`: only a whole archive opens), a file whose checksum
//   fails, and a file past `limit` bytes.
// - NAMES come back as UTF-8 with '/' separators.
//
// `write_rar` is the fixture writer: a RAR 5 archive of stored files (no compression), written
// from the format's public description, so the tests make their archives without the RAR program.
//
// ge:summary RAR archives (RAR 4 and 5) read through the unRAR library; a stored RAR 5 writer
// ge:keywords rar rar4 rar5 unrar downloaded-demo archive is_rar read_rar rar_unpack write_rar
// ge:keywords multi-volume encrypted-archive
// ge:skill vfs
// ge:test tests/vfs_rar_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ge::vfs {

struct RarEntry {
  std::string name;  // UTF-8, '/' separated
  uint64_t size = 0;
  uint32_t crc = 0;  // CRC-32 of the file's bytes (0 when the archive keeps another hash)
  bool dir = false;
};

// Whether `bytes` open with a RAR 4 or RAR 5 signature.
bool is_rar(std::span<const uint8_t> bytes);
// The archive's entries, in order; false with the reason.
bool read_rar(std::span<const uint8_t> bytes, std::vector<RarEntry>& out, std::string& error);
// Entry `index`'s bytes (as `read_rar` numbered it), its checksum held; false with the reason.
bool rar_unpack(std::span<const uint8_t> bytes, size_t index, std::vector<uint8_t>& out,
                std::string& error, size_t limit = size_t{1} << 31);
// A RAR 5 archive of `files` stored uncompressed, for tests and fixtures. A name ending in '/'
// is a directory.
std::vector<uint8_t> write_rar(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);

}  // namespace ge::vfs

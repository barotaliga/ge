// AN LHA ARCHIVE read from memory, including one inside a self-extracting DOS program: the first
// header whose method tag and checksum are right is the archive's start, and the headers after it
// are walked one by one to the end marker. Headers of levels 0, 1 and 2 (a directory's name joined
// to the file's with '/'); the methods `-lh0-` (stored), `-lhd-` (a directory) and `-lh4-` to
// `-lh7-` (LZ77 over a 4, 8, 32 or 64 KB window, its matches and literals in Huffman codes rebuilt
// every block). Each file's CRC-16 is checked after it is unpacked. Entry names are only matched,
// never used as paths on disk.
//
// ge:summary Reads an LHA archive (self-extracting or not): levels 0-2, -lh0- and -lh4- to -lh7-
// ge:keywords lha lzh lh5 lh0 lh6 lh7 archive sfx self-extracting huffman lz77 crc16 unpack
// ge:keywords read_lha lha_unpack write_lha
// ge:skill vfs
// ge:test tests/vfs_lha_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::vfs {

struct LhaEntry {
  std::string name;   // as stored, usually with '\\' separators
  char method = '0';  // `-lh?-`'s: '0' stored, 'd' a directory, '4' to '7' compressed
  uint32_t packed = 0, size = 0;
  uint16_t crc = 0;   // CRC-16 of the unpacked bytes
  uint64_t data = 0;  // offset of the packed bytes
};

// Whether `bytes` open with an LHA header (not a self-extractor's program): the method tag at byte
// 2 and the header's checksum (levels 0 and 1) or its sizes (level 2) holding.
bool is_lha(std::span<const uint8_t> bytes);
// The entries of the archive in `bytes`, found anywhere in them (a self-extractor's program comes
// first); false with the reason.
bool read_lha(std::span<const uint8_t> bytes, std::vector<LhaEntry>* out, std::string* error);
// An entry's bytes, unpacked and checked against its CRC-16; false with the reason.
bool lha_unpack(std::span<const uint8_t> bytes, const LhaEntry& entry, std::vector<uint8_t>* out,
                std::string* error);
// An LHA archive of `files` (name, bytes), for tests and fixtures: `program`'s bytes first (a
// self-extractor's), then headers of `level` (0, 1 or 2) and each file by `method` ('0' stored,
// '4' to '7' in one block of fixed but complete codes), then the end marker. Names may hold '\\'.
std::vector<uint8_t> write_lha(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, char method = '5',
    int level = 0, std::span<const uint8_t> program = {});
// The CRC-16 LHA stores (polynomial 0x8005 reflected, starting at 0).
uint16_t crc16(std::span<const uint8_t> bytes);

}  // namespace ge::vfs

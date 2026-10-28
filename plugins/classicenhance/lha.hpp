// AN LHA ARCHIVE read from memory, including one inside a self-extracting DOS program: the first
// header whose method tag and checksum are right is the archive's start, and the headers after it
// are walked one by one to the end marker. Headers of level 0 and 1; the methods `-lh0-` (stored)
// and `-lh5-` (LZ77 over an 8 KB window, its matches and literals in Huffman codes rebuilt every
// block). Each file's CRC-16 is checked after it is unpacked. Entry names are only matched,
// never used as paths on disk.
//
// ge:summary Reads an LHA archive (self-extracting or not): level 0/1 headers, -lh0- and -lh5-
// ge:keywords lha lzh lh5 lh0 archive sfx self-extracting huffman lz77 crc16 unpack
// ge:skill classicenhance
// ge:test tests/classicenhance_source_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::classicenhance {

struct LhaEntry {
  std::string name;   // as stored, usually with '\\' separators
  char method = '0';  // the digit of `-lh?-`: '0' stored, '5' compressed
  uint32_t packed = 0, size = 0;
  uint16_t crc = 0;   // CRC-16 of the unpacked bytes
  uint64_t data = 0;  // offset of the packed bytes
};

// The entries of the archive in `bytes`, found anywhere in them (a self-extractor's program comes
// first); false with the reason.
bool read_lha(std::span<const uint8_t> bytes, std::vector<LhaEntry>* out, std::string* error);
// An entry's bytes, unpacked and checked against its CRC-16; false with the reason.
bool lha_unpack(std::span<const uint8_t> bytes, const LhaEntry& entry, std::vector<uint8_t>* out,
                std::string* error);
// The CRC-16 LHA stores (polynomial 0x8005 reflected, starting at 0).
uint16_t crc16(std::span<const uint8_t> bytes);

}  // namespace ge::classicenhance

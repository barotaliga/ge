// XZ STREAMS AND 7Z ARCHIVES, the two LZMA containers: an `.xz` file (one stream or several
// following one another, each block LZMA2 under the branch filters, its check verified) unpacked
// whole, and a `.7z` archive's directory read and one of its files unpacked (any coder 7z writes:
// LZMA, LZMA2, PPMd, the branch filters, BCJ2, delta, copy; its CRC checked). Encrypted 7z
// archives are refused, naming the file. The writers make `.xz` and LZMA-coded `.7z` files for
// the tests and the labs. cmake/Archives.cmake builds the LZMA SDK that does the coding.
//
// Every refusal names what is wrong and the next action (docs/architecture.md § Diagnostics).
//
// ge:summary xz streams unpacked and packed; 7z archives listed, one file unpacked, written
// ge:keywords xz lzma lzma2 7z seven-zip sevenzip archive unpack pack ppmd bcj delta crc64
// ge:keywords xz_unpack xz_pack read_7z seven_unpack write_7z zip_lzma_unpack
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::vfs {

// The first six bytes of an xz stream and of a 7z archive.
inline constexpr uint8_t kXzMagic[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
inline constexpr uint8_t kSevenMagic[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

// Every stream in the xz file `bytes` unpacked into `out`; false with the reason when it is
// damaged, cut short, or would unpack to more than `limit` bytes.
bool xz_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, std::string& error,
               size_t limit = size_t{1} << 31);
// `bytes` as an xz file at preset `level` (0..9) with a CRC-64 check.
std::vector<uint8_t> xz_pack(std::span<const uint8_t> bytes, int level = 9);

// A zip entry of method 14 (LZMA: a 2-byte version, a 2-byte properties size, the 5 properties,
// the raw stream) unpacked to `size` bytes; false with the reason.
bool zip_lzma_unpack(std::span<const uint8_t> packed, size_t size, std::vector<uint8_t>& out,
                     std::string& error);

struct SevenEntry {
  std::string name;  // UTF-8, '/' separated
  uint64_t size = 0;
  bool dir = false;
};

// The files of the 7z archive `bytes`; false with the reason when it is not one or is damaged.
bool read_7z(std::span<const uint8_t> bytes, std::vector<SevenEntry>& out, std::string& error);
// File `index` (of read_7z's list) unpacked and checked; false with the reason.
bool seven_unpack(std::span<const uint8_t> bytes, size_t index, std::vector<uint8_t>& out,
                  std::string& error);
// A 7z archive of one file, LZMA-coded at preset `level` (0..9), its CRC kept.
std::vector<uint8_t> write_7z(const std::string& name, std::span<const uint8_t> bytes,
                              int level = 9);

}  // namespace ge::vfs

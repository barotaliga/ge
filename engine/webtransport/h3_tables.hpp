// The published tables of HTTP/3's header compression, shared by h3.cpp and its test.
//
// ge:summary The declarations of QPACK's static table and HPACK's Huffman code
// ge:keywords webtransport qpack static-table hpack huffman-code
// ge:skill net
// ge:test tests/webtransport_h3_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace ge::webtransport::h3 {

struct StaticEntry {
  const char* name;
  const char* value;
};
constexpr size_t kStaticTableSize = 99;
extern const StaticEntry kStaticTable[kStaticTableSize];

struct HuffmanCode {
  uint32_t code;  // aligned to the least significant bit
  uint8_t bits;
};
extern const HuffmanCode kHuffman[257];  // 256 is EOS

}  // namespace ge::webtransport::h3

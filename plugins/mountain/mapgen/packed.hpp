// THE PACKED DATA the bakes write into generated sources (real_faces/, real_hints/): whole
// numbers as zig-zag varints (n >= 0 to 2n, n < 0 to -2n-1; base-128, low seven bits first, the
// top bit set on every byte but a value's last), the bytes as base64. A writer and a reader of
// the varints, and base64 both ways; nothing here knows what the numbers mean.
//
// ge:summary The bakes' packing: zig-zag varints and base64, written and read
// ge:keywords packed varint zigzag base64 encode decode generated-data
// ge:skill mountain
// ge:test tests/mountain_real_face_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::mountain::mapgen::packed {

// The bytes `text` (base64, padded or not) stands for, or nothing for a character outside the
// alphabet or one after the padding.
std::optional<std::vector<uint8_t>> from_base64(std::string_view text);
// `bytes` as base64, padded.
std::string to_base64(std::span<const uint8_t> bytes);

// Whole numbers written as zig-zag varints.
struct Writer {
  std::vector<uint8_t> bytes;
  void put(int64_t v);
};

// Whole numbers read back: `get` sets `ok` false (and returns 0) past the end or on a varint too
// long for 64 bits.
struct Reader {
  std::span<const uint8_t> bytes;
  size_t at = 0;
  bool ok = true;
  int64_t get();
  bool done() const { return at >= bytes.size(); }
};

}  // namespace ge::mountain::mapgen::packed

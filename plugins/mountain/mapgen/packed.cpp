// The definitions behind packed.hpp.
//
// ge:summary The bakes' packing: the varints' and base64's definitions
// ge:keywords packed varint zigzag base64
// ge:skill mountain
// ge:test tests/mountain_real_face_test.cpp
#include "mountain/mapgen/packed.hpp"

namespace ge::mountain::mapgen::packed {

namespace {

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// The value of a base64 character, or -1.
int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

}  // namespace

std::optional<std::vector<uint8_t>> from_base64(std::string_view text) {
  std::vector<uint8_t> bytes;
  bytes.reserve(text.size() * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  size_t pad = 0;
  for (const char ch : text) {
    if (ch == '=') {
      pad++;
      continue;
    }
    const int v = sextet(ch);
    if (pad > 0 || v < 0) return std::nullopt;
    acc = (acc << 6) | uint32_t(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      bytes.push_back(uint8_t((acc >> bits) & 0xff));
    }
  }
  return bytes;
}

std::string to_base64(std::span<const uint8_t> bytes) {
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32_t b0 = bytes[i];
    const uint32_t b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
    const uint32_t b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
    const uint32_t v = (b0 << 16) | (b1 << 8) | b2;
    out += kAlphabet[(v >> 18) & 63];
    out += kAlphabet[(v >> 12) & 63];
    out += i + 1 < bytes.size() ? kAlphabet[(v >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[v & 63] : '=';
  }
  return out;
}

void Writer::put(int64_t v) {
  uint64_t z = v >= 0 ? uint64_t(v) * 2 : uint64_t(-(v + 1)) * 2 + 1;
  while (z >= 0x80) {
    bytes.push_back(uint8_t((z & 0x7f) | 0x80));
    z >>= 7;
  }
  bytes.push_back(uint8_t(z));
}

int64_t Reader::get() {
  uint64_t z = 0;
  int shift = 0;
  uint8_t b = 0;
  do {
    if (at >= bytes.size() || shift > 63) {
      ok = false;
      return 0;
    }
    b = bytes[at++];
    z |= uint64_t(b & 0x7f) << shift;
    shift += 7;
  } while (b & 0x80);
  return (z & 1) ? -int64_t(z >> 1) - 1 : int64_t(z >> 1);
}

}  // namespace ge::mountain::mapgen::packed

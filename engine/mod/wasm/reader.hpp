// The byte reader under the WebAssembly module's reader (module.cpp, validate.cpp): LEB128
// integers, fixed-width little-endian words, names; the first failure is kept with its byte
// offset in the file, and every later read returns zero.
//
// ge:summary The WebAssembly reader's bytes: LEB128, fixed words, names, the first failure
// ge:keywords wasm leb128 reader byte-offset uleb sleb
// ge:skill mod
// ge:test tests/mod_wasm_test.cpp
#pragma once

#include <cstdint>
#include <string>

namespace ge::mod::wasm {

inline std::string hex_byte(uint32_t b) {
  static const char* d = "0123456789abcdef";
  std::string s = "0x";
  if (b > 0xff) s += d[(b >> 12) & 15], s += d[(b >> 8) & 15];
  s += d[(b >> 4) & 15];
  s += d[b & 15];
  return s;
}

class Reader {
 public:
  Reader(const uint8_t* p, size_t n, size_t base = 0) : p_(p), n_(n), base_(base) {}

  bool ok() const { return error_.empty(); }
  bool done() const { return at_ >= n_; }
  size_t left() const { return n_ - at_; }
  size_t pos() const { return base_ + at_; }
  void skip(size_t k) { at_ = k > left() ? n_ : at_ + k; }

  // False, with the first failure kept.
  bool fail(const std::string& what) {
    if (error_.empty())
      error_ = "the module at byte " + std::to_string(pos()) + ": " + what +
               "; next: rebuild the mod for wasm32 with the engine's template (ge mod build)";
    at_ = n_;
    return false;
  }
  const std::string& error() const { return error_; }
  // False, with another reader's failure kept as this one's.
  bool adopt(const Reader& other) {
    if (error_.empty()) error_ = other.error_.empty() ? "a bad body" : other.error_;
    at_ = n_;
    return false;
  }

  uint8_t byte() {
    if (at_ >= n_) {
      fail("the bytes end early");
      return 0;
    }
    return p_[at_++];
  }
  const uint8_t* take(size_t k) {
    if (k > left()) {
      fail("the bytes end early");
      return nullptr;
    }
    const uint8_t* q = p_ + at_;
    at_ += k;
    return q;
  }
  uint64_t fixed(int bytes) {
    uint64_t v = 0;
    for (int k = 0; k < bytes; k++) v |= static_cast<uint64_t>(byte()) << (8 * k);
    return v;
  }
  uint32_t u32() { return static_cast<uint32_t>(leb(32, false)); }
  int32_t s32() { return static_cast<int32_t>(leb(32, true)); }
  int64_t s64() { return static_cast<int64_t>(leb(64, true)); }
  // A block type's signed 33-bit index.
  int64_t s33() { return static_cast<int64_t>(leb(33, true)); }
  std::string name() {
    const uint32_t k = u32();
    const uint8_t* q = take(k);
    return q ? std::string(reinterpret_cast<const char*>(q), k) : std::string();
  }

 private:
  uint64_t leb(int bits, bool is_signed) {
    uint64_t v = 0;
    int shift = 0;
    const int max_bytes = (bits + 6) / 7;
    for (int k = 0; k < max_bytes; k++) {
      const uint8_t b = byte();
      if (!ok()) return 0;
      v |= static_cast<uint64_t>(b & 0x7f) << shift;
      shift += 7;
      if ((b & 0x80) == 0) {
        if (k == max_bytes - 1) {
          // The last byte's unused bits must be the sign's (signed) or zero (unsigned).
          const int used = bits - 7 * (max_bytes - 1);
          const unsigned rest = (b & 0x7fu) >> (used - (is_signed ? 1 : 0));
          const unsigned want = is_signed && (b & (1u << (used - 1))) ? (0x7fu >> (used - 1)) : 0u;
          if (rest != want) {
            fail("an integer whose last byte has bits past its width");
            return 0;
          }
        }
        if (is_signed && shift < 64 && (b & 0x40)) v |= ~uint64_t{0} << shift;
        if (bits < 64 && !is_signed && (v >> bits) != 0) {
          fail("an integer past its width");
          return 0;
        }
        return v;
      }
    }
    fail("an integer longer than its width allows");
    return 0;
  }

  const uint8_t* p_;
  size_t n_, at_ = 0, base_;
  std::string error_;
};

}  // namespace ge::mod::wasm

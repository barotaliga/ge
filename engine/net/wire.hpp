// THE WIRE'S BYTES: a writer and a reader over a caller's buffer, little endian, with LEB128
// varints, that never step past the buffer's end. A write that does not fit sets `overflow`
// and writes nothing more; a read past the end sets `bad` and reads zeros, so a parser checks
// once at the end instead of after every field, and a hostile packet can never read outside
// it. Also the 16-bit sequence arithmetic every ack and reliable id uses (the numbers wrap).
//
// ge:summary The wire's bytes: bounded writer and reader, little endian, varints, sequence math
// ge:keywords net wire bytes writer reader serialize deserialize varint leb128 little-endian
// ge:keywords sequence wrap seq-newer bitstream buffer
// ge:skill net
// ge:test tests/net_wire_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ge::net {

struct Writer {
  uint8_t* data;
  size_t capacity;
  size_t size = 0;
  bool overflow = false;
};

inline Writer make_writer(uint8_t* data, size_t capacity) { return Writer{data, capacity}; }

inline void write_bytes(Writer& w, const void* bytes, size_t n) {
  if (w.overflow || n > w.capacity - w.size) {
    w.overflow = true;
    return;
  }
  if (n) std::memcpy(w.data + w.size, bytes, n);
  w.size += n;
}
inline void write_u8(Writer& w, uint8_t v) { write_bytes(w, &v, 1); }
inline void write_u16(Writer& w, uint16_t v) {
  const uint8_t b[2] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)};
  write_bytes(w, b, 2);
}
inline void write_u32(Writer& w, uint32_t v) {
  uint8_t b[4];
  for (int i = 0; i < 4; i++) b[i] = static_cast<uint8_t>(v >> (8 * i));
  write_bytes(w, b, 4);
}
inline void write_u64(Writer& w, uint64_t v) {
  uint8_t b[8];
  for (int i = 0; i < 8; i++) b[i] = static_cast<uint8_t>(v >> (8 * i));
  write_bytes(w, b, 8);
}
// Seven bits a byte, low first, the top bit set on every byte but the last.
inline void write_varint(Writer& w, uint64_t v) {
  uint8_t b[10];
  size_t n = 0;
  do {
    b[n] = static_cast<uint8_t>(v & 0x7f);
    v >>= 7;
    if (v) b[n] |= 0x80;
    n++;
  } while (v);
  write_bytes(w, b, n);
}
// Zeros up to `size` bytes in all (a request padded so a reply is never bigger than it).
inline void write_pad_to(Writer& w, size_t size) {
  static const uint8_t zeros[64] = {};
  while (!w.overflow && w.size < size) {
    const size_t n = size - w.size < sizeof zeros ? size - w.size : sizeof zeros;
    write_bytes(w, zeros, n);
  }
}
// The bytes a varint of `v` takes.
inline size_t varint_size(uint64_t v) {
  size_t n = 1;
  while (v >>= 7) n++;
  return n;
}

struct Reader {
  const uint8_t* data;
  size_t size;
  size_t at = 0;
  bool bad = false;
};

inline Reader make_reader(const uint8_t* data, size_t size) { return Reader{data, size}; }
inline size_t remaining(const Reader& r) { return r.bad ? 0 : r.size - r.at; }

inline bool read_bytes(Reader& r, void* out, size_t n) {
  if (r.bad || n > r.size - r.at) {
    r.bad = true;
    if (n) std::memset(out, 0, n);
    return false;
  }
  if (n) std::memcpy(out, r.data + r.at, n);
  r.at += n;
  return true;
}
// A view of the next `n` bytes, or null (and `bad`) when there are fewer.
inline const uint8_t* read_view(Reader& r, size_t n) {
  if (r.bad || n > r.size - r.at) {
    r.bad = true;
    return nullptr;
  }
  const uint8_t* p = r.data + r.at;
  r.at += n;
  return p;
}
inline uint8_t read_u8(Reader& r) {
  uint8_t v = 0;
  read_bytes(r, &v, 1);
  return v;
}
inline uint16_t read_u16(Reader& r) {
  uint8_t b[2];
  read_bytes(r, b, 2);
  return static_cast<uint16_t>(b[0] | b[1] << 8);
}
inline uint32_t read_u32(Reader& r) {
  uint8_t b[4];
  read_bytes(r, b, 4);
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(b[i]) << (8 * i);
  return v;
}
inline uint64_t read_u64(Reader& r) {
  uint8_t b[8];
  read_bytes(r, b, 8);
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(b[i]) << (8 * i);
  return v;
}
// At most ten bytes; an eleventh, or a tenth with more than the 64th bit, is `bad`.
inline uint64_t read_varint(Reader& r) {
  uint64_t v = 0;
  for (int i = 0; i < 10; i++) {
    const uint8_t b = read_u8(r);
    if (r.bad) return 0;
    if (i == 9 && b > 1) break;
    v |= static_cast<uint64_t>(b & 0x7f) << (7 * i);
    if (!(b & 0x80)) return v;
  }
  r.bad = true;
  return 0;
}

// SEQUENCES WRAP at 2^16: `a` is newer than `b` when it is ahead by less than half the circle.
inline bool seq_newer(uint16_t a, uint16_t b) {
  return a != b && static_cast<uint16_t>(a - b) < 0x8000;
}
// How far `a` is ahead of `b` (negative when behind), on the circle.
inline int seq_diff(uint16_t a, uint16_t b) {
  return static_cast<int>(static_cast<int16_t>(static_cast<uint16_t>(a - b)));
}

}  // namespace ge::net

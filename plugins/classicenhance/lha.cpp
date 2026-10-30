// The LHA reader. `-lh5-` as the format defines it: each block starts with its symbol count
// (16 bits) and three code tables sent as code lengths: the lengths' own code (19 symbols, sent
// in 3-bit fields with a unary escape, a 2-bit run of zeros after the third), the literal and
// length code (510 symbols: 256 bytes and match lengths 3..256, its lengths sent through the
// first code with runs of zeros), and the distance code (14 symbols, distance bit counts). A
// table of one symbol is sent as a count of 0 and the symbol, and decodes without reading bits.
// Codes are canonical and read most significant bit first, so each table here is a 16-bit lookup.
//
// ge:summary The LHA headers, the -lh5- decoder and CRC-16
// ge:keywords lha lh5 decode huffman canonical table block crc16 header level
// ge:skill classicenhance
// ge:test tests/classicenhance_source_test.cpp
#include "classicenhance/lha.hpp"

#include <array>

namespace ge::classicenhance {

namespace {

constexpr int kNC = 510;  // literals and match lengths
constexpr int kNT = 19;   // the code lengths' code
constexpr int kNP = 14;   // distance bit counts (an 8 KB window)

uint16_t u16(std::span<const uint8_t> b, size_t at) { return uint16_t(b[at] | (b[at + 1] << 8)); }
uint32_t u32(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | (uint32_t(b[at + 1]) << 8) | (uint32_t(b[at + 2]) << 16) |
         (uint32_t(b[at + 3]) << 24);
}

// Bits most significant first; past the end it reads zeros and counts them.
struct BitIn {
  std::span<const uint8_t> in;
  size_t at = 0;
  uint64_t acc = 0;
  int have = 0;
  uint64_t past = 0;

  void fill() {
    while (have <= 56) {
      uint64_t b = 0;
      if (at < in.size()) {
        b = in[at++];
      } else {
        past += 8;
      }
      acc |= b << (56 - have);
      have += 8;
    }
  }
  uint32_t peek(int n) {
    if (have < n) fill();
    return uint32_t(acc >> (64 - n));
  }
  void skip(int n) {
    if (have < n) fill();
    acc <<= n;
    have -= n;
  }
  uint32_t take(int n) {
    if (n == 0) return 0;
    const uint32_t v = peek(n);
    skip(n);
    return v;
  }
};

// A canonical code: a symbol and its length for every 16-bit prefix, or one symbol of length 0.
struct Code {
  std::vector<uint16_t> sym;  // 65536 entries
  std::vector<uint8_t> len;
  int single = -1;

  bool build(const uint8_t* lengths, int n) {
    single = -1;
    std::array<uint32_t, 18> count{};
    for (int i = 0; i < n; i++) count[lengths[i]]++;
    std::array<uint32_t, 18> start{};
    uint32_t next = 0;
    for (int l = 1; l <= 16; l++) {
      start[l] = next;
      next += count[l] << (16 - l);
    }
    if (next != 65536) return false;  // over- or under-subscribed
    sym.assign(65536, 0);
    len.assign(65536, 0);
    for (int i = 0; i < n; i++) {
      const int l = lengths[i];
      if (l == 0) continue;
      const uint32_t span = 1u << (16 - l);
      for (uint32_t k = 0; k < span; k++) {
        sym[start[l] + k] = uint16_t(i);
        len[start[l] + k] = uint8_t(l);
      }
      start[l] += span;
    }
    return true;
  }
  void one(int s) { single = s; }
  int decode(BitIn& bits) const {
    if (single >= 0) return single;
    const uint32_t k = bits.peek(16);
    bits.skip(len[k]);
    return sym[k];
  }
};

bool read_pt(BitIn& bits, int nn, int nbit, int special, Code& code) {
  std::array<uint8_t, kNT> lengths{};
  const int n = int(bits.take(nbit));
  if (n == 0) {
    const int c = int(bits.take(nbit));
    if (c >= nn) return false;
    code.one(c);
    return true;
  }
  if (n > nn) return false;
  int i = 0;
  while (i < n) {
    int c = int(bits.peek(3));
    if (c == 7) {
      bits.skip(3);
      while (bits.take(1) == 1) {
        if (++c > 16) return false;
      }
    } else {
      bits.skip(3);
    }
    lengths[size_t(i++)] = uint8_t(c);
    if (i == special) {
      int z = int(bits.take(2));
      while (z-- > 0 && i < nn) lengths[size_t(i++)] = 0;
    }
  }
  return code.build(lengths.data(), nn);
}

bool read_c(BitIn& bits, const Code& pt, Code& code) {
  std::array<uint8_t, kNC> lengths{};
  const int n = int(bits.take(9));
  if (n == 0) {
    const int c = int(bits.take(9));
    if (c >= kNC) return false;
    code.one(c);
    return true;
  }
  if (n > kNC) return false;
  int i = 0;
  while (i < n) {
    int c = pt.decode(bits);
    if (c <= 2) {
      int run = c == 0 ? 1 : c == 1 ? int(bits.take(4)) + 3 : int(bits.take(9)) + 20;
      if (i + run > kNC) return false;
      while (run-- > 0) lengths[size_t(i++)] = 0;
    } else {
      lengths[size_t(i++)] = uint8_t(c - 2);
    }
  }
  return code.build(lengths.data(), kNC);
}

bool unpack_lh5(std::span<const uint8_t> in, uint32_t size, std::vector<uint8_t>& out) {
  out.clear();
  out.reserve(size);
  BitIn bits{in};
  Code pt;
  Code c_code;
  Code p_code;
  uint32_t left = 0;
  while (out.size() < size) {
    if (left == 0) {
      left = bits.take(16);
      if (left == 0) return false;
      if (!read_pt(bits, kNT, 5, 3, pt) || !read_c(bits, pt, c_code) ||
          !read_pt(bits, kNP, 4, -1, p_code)) {
        return false;
      }
    }
    left--;
    const int c = c_code.decode(bits);
    if (c < 256) {
      out.push_back(uint8_t(c));
    } else {
      const size_t length = size_t(c) - 253;
      int p = p_code.decode(bits);
      if (p != 0) p = (1 << (p - 1)) + int(bits.take(p - 1));
      const size_t back = size_t(p) + 1;
      if (back > out.size() || out.size() + length > size) return false;
      const size_t from = out.size() - back;
      for (size_t k = 0; k < length; k++) out.push_back(out[from + k]);
    }
    if (bits.past > 128) return false;  // read well past the packed bytes: damaged
  }
  return true;
}

// The entry whose header starts at `at`; false when the bytes there are not a header.
bool header_at(std::span<const uint8_t> b, size_t at, LhaEntry* e, uint64_t* next) {
  if (at + 24 > b.size()) return false;
  const size_t h = b[at];
  if (h < 22 || at + h + 2 > b.size()) return false;
  if (b[at + 2] != '-' || b[at + 3] != 'l' || b[at + 4] != 'h' || b[at + 6] != '-') return false;
  const int level = b[at + 20];
  if (level > 1) return false;
  uint8_t sum = 0;
  for (size_t i = 0; i < h; i++) sum = uint8_t(sum + b[at + 2 + i]);
  if (sum != b[at + 1]) return false;
  e->method = char(b[at + 5]);
  e->packed = u32(b, at + 7);
  e->size = u32(b, at + 11);
  const size_t name_len = b[at + 21];
  if (22 + name_len + 2 > h + 2) return false;
  e->name.assign(reinterpret_cast<const char*>(b.data() + at + 22), name_len);
  e->crc = u16(b, at + 22 + name_len);
  uint64_t data = at + h + 2;
  if (level == 1) {
    // Extended headers follow the base one, each sized by the two bytes before it, and the
    // packed size counts them.
    uint64_t ext = u16(b, at + h);
    while (ext != 0) {
      if (data + ext > b.size() || e->packed < ext) return false;
      e->packed -= uint32_t(ext);
      const uint64_t here = data;
      data += ext;
      ext = u16(b, size_t(here + ext - 2));
    }
  }
  if (data + e->packed > b.size()) return false;
  e->data = data;
  *next = data + e->packed;
  return true;
}

}  // namespace

uint16_t crc16(std::span<const uint8_t> bytes) {
  static const std::array<uint16_t, 256> table = [] {
    std::array<uint16_t, 256> t{};
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xa001 : c >> 1;
      t[i] = uint16_t(c);
    }
    return t;
  }();
  uint16_t crc = 0;
  for (const uint8_t v : bytes) crc = uint16_t(table[(crc ^ v) & 0xff] ^ (crc >> 8));
  return crc;
}

bool read_lha(std::span<const uint8_t> bytes, std::vector<LhaEntry>* out, std::string* error) {
  out->clear();
  size_t at = 0;
  LhaEntry e;
  uint64_t next = 0;
  for (; at + 24 <= bytes.size(); at++) {
    if (bytes[at + 2] == '-' && header_at(bytes, at, &e, &next)) break;
  }
  if (at + 24 > bytes.size()) {
    *error = "lha: no archive header (-lh?-) with a good checksum anywhere in the " +
             std::to_string(bytes.size()) + " bytes";
    return false;
  }
  while (true) {
    out->push_back(e);
    at = size_t(next);
    if (at >= bytes.size() || bytes[at] == 0) break;  // the end marker, or the file's end
    if (!header_at(bytes, at, &e, &next)) {
      *error = "lha: the header after '" + out->back().name + "' (byte " + std::to_string(at) +
               ") is damaged or of a level other than 0 and 1";
      return false;
    }
  }
  return true;
}

bool lha_unpack(std::span<const uint8_t> bytes, const LhaEntry& entry, std::vector<uint8_t>* out,
                std::string* error) {
  const std::span<const uint8_t> packed = bytes.subspan(size_t(entry.data), entry.packed);
  if (entry.method == '0') {
    out->assign(packed.begin(), packed.end());
    if (entry.packed != entry.size) {
      *error = "lha: stored entry '" + entry.name + "' has different packed and unpacked sizes";
      return false;
    }
  } else if (entry.method == '5') {
    if (!unpack_lh5(packed, entry.size, *out)) {
      *error = "lha: entry '" + entry.name + "' does not unpack to its " +
               std::to_string(entry.size) + " bytes; the archive is damaged";
      return false;
    }
  } else {
    *error = "lha: entry '" + entry.name + "' uses -lh" + std::string(1, entry.method) +
             "-; only -lh0- and -lh5- are read";
    return false;
  }
  if (crc16(*out) != entry.crc) {
    *error = "lha: entry '" + entry.name + "' fails its CRC-16; the archive is damaged";
    return false;
  }
  return true;
}

}  // namespace ge::classicenhance

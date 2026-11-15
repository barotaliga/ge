// The LHA fixture writer: a test's archive of any level and method. Matches are found greedily in
// the window (capped at 8 KB, so the search stays quick), and every block (65535 symbols) sends
// codes chosen once and complete: the literal and length code 8 bits for symbols 0 and 1 and 9 for
// the rest, the distance code 3, 4 or 5 bits as the method's count of distance symbols needs, the
// code lengths' own code two 1-bit codes (for lengths 10 and 11).
//
// ge:summary The LHA writer for tests: levels 0-2, -lh0- and -lh4- to -lh7- in fixed codes
// ge:keywords lha-write lh5-encode fixture sfx level-2 extended-header write_lha
// ge:skill vfs
// ge:test tests/vfs_lha_test.cpp
#include <algorithm>
#include <array>

#include "vfs/lha.hpp"

namespace ge::vfs {

namespace {

struct BitOut {
  std::vector<uint8_t> bytes;
  uint32_t acc = 0;
  int have = 0;
  void put(uint32_t v, int n) {
    for (int i = n - 1; i >= 0; i--) {
      acc = (acc << 1) | ((v >> i) & 1);
      if (++have == 8) {
        bytes.push_back(uint8_t(acc));
        acc = 0;
        have = 0;
      }
    }
  }
  void flush() {
    if (have > 0) put(0, 8 - have);
  }
};

// The distance code's lengths for `np` symbols, complete: 3 and 4 bits, or 4 and 5.
std::vector<int> distance_lengths(int np) {
  std::vector<int> l(size_t(np), 4);
  if (np == 13) std::fill_n(l.begin(), 3, 3);
  if (np == 14) std::fill_n(l.begin(), 2, 3);
  if (np == 17) l[15] = l[16] = 5;
  return l;
}

// The canonical codes of `lengths`, shortest first, then by symbol.
std::vector<uint32_t> canonical(const std::vector<int>& lengths) {
  std::vector<uint32_t> codes(lengths.size());
  uint32_t code = 0;
  for (int l = 1; l <= 16; l++) {
    for (size_t s = 0; s < lengths.size(); s++) {
      if (lengths[s] == l) codes[s] = code++;
    }
    code <<= 1;
  }
  return codes;
}

std::vector<uint8_t> encode(const std::vector<uint8_t>& in, int np, int pbit) {
  struct Sym {
    int c, p;  // p < 0: a literal
  };
  const size_t window = std::min<size_t>(size_t{1} << (np - 1), 8192);
  std::vector<Sym> syms;
  for (size_t i = 0; i < in.size();) {
    size_t best = 0, dist = 0;
    for (size_t back = 1; back <= std::min(i, window); back++) {
      size_t n = 0;
      while (n < 256 && i + n < in.size() && in[i + n] == in[i - back + n]) n++;
      if (n > best) {
        best = n;
        dist = back;
      }
    }
    if (best >= 3) {
      syms.push_back({int(best) + 253, int(dist) - 1});
      i += best;
    } else {
      syms.push_back({in[i], -1});
      i++;
    }
  }
  BitOut b;
  const std::vector<int> pl = distance_lengths(np);
  const std::vector<uint32_t> pc = canonical(pl);
  // A block holds at most 65535 symbols (its count is 16 bits); each sends the codes again.
  for (size_t first = 0; first < syms.size(); first += 65535) {
    const size_t count = std::min<size_t>(65535, syms.size() - first);
    b.put(uint32_t(count), 16);
    // The lengths' code: 12 lengths sent, 0 for 0..9 (a run of three zeros after the third), 1
    // for 10 and 11.
    b.put(12, 5);
    for (int i = 0; i < 3; i++) b.put(0, 3);
    b.put(3, 2);
    for (int i = 6; i < 10; i++) b.put(0, 3);
    b.put(1, 3);
    b.put(1, 3);
    // The literal/length code's 510 lengths through it: 8 is sent as 10 (code 0), 9 as 11 (code 1).
    b.put(510, 9);
    for (int i = 0; i < 510; i++) b.put(i < 2 ? 0 : 1, 1);
    b.put(uint32_t(np), pbit);
    for (const int l : pl) b.put(uint32_t(l), 3);
    for (size_t i = first; i < first + count; i++) {
      const Sym& s = syms[i];
      if (s.c < 2) {
        b.put(uint32_t(s.c), 8);
      } else {
        b.put(uint32_t(4 + s.c - 2), 9);
      }
      if (s.p < 0) continue;
      int bits = 0;
      while ((s.p >> bits) != 0) bits++;
      b.put(pc[size_t(bits)], pl[size_t(bits)]);
      if (bits > 1) b.put(uint32_t(s.p - (1 << (bits - 1))), bits - 1);
    }
  }
  b.flush();
  return b.bytes;
}

void put16(std::vector<uint8_t>& o, uint32_t v) {
  o.push_back(uint8_t(v));
  o.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& o, uint32_t v) {
  put16(o, v & 0xFFFF);
  put16(o, v >> 16);
}

}  // namespace

std::vector<uint8_t> write_lha(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, char method, int level,
    std::span<const uint8_t> program) {
  static constexpr std::array<int, 4> kNp = {13, 14, 16, 17};
  std::vector<uint8_t> o(program.begin(), program.end());
  for (const auto& [name, data] : files) {
    const int k = method - '4';
    const std::vector<uint8_t> packed =
        method == '0' ? data : encode(data, kNp[size_t(k)], k < 2 ? 4 : 5);
    const uint16_t crc = crc16(data);
    // Level 1 and 2 keep the directory in an extended header, separated by 0xFF.
    std::string dir;
    std::string file = name;
    if (level > 0) {
      const size_t cut = name.find_last_of('\\');
      if (cut != std::string::npos) {
        dir = name.substr(0, cut);
        std::replace(dir.begin(), dir.end(), '\\', char(0xFF));
        file = name.substr(cut + 1);
      }
    }
    std::vector<uint8_t> h;
    if (level == 2) {
      std::vector<std::pair<uint8_t, std::string>> parts = {{0x01, file}};
      if (!dir.empty()) parts.emplace_back(0x02, dir);
      // Each header's size goes in the two bytes before it: the first's in the fixed part.
      std::vector<uint8_t> chain;
      for (size_t i = 0; i < parts.size(); i++) {
        const uint32_t size = uint32_t(parts[i].second.size() + 3);
        if (i > 0) put16(chain, size);
        chain.push_back(parts[i].first);
        chain.insert(chain.end(), parts[i].second.begin(), parts[i].second.end());
      }
      put16(chain, 0);
      const uint32_t first = uint32_t(parts[0].second.size() + 3);
      const uint32_t total = 26 + uint32_t(chain.size());
      put16(h, total);
      for (const char c : std::string("-lh") + method + "-") h.push_back(uint8_t(c));
      put32(h, uint32_t(packed.size()));
      put32(h, uint32_t(data.size()));
      put32(h, 0);
      h.push_back(0x20);
      h.push_back(2);
      put16(h, crc);
      h.push_back('U');
      put16(h, first);
      h.insert(h.end(), chain.begin(), chain.end());
      o.insert(o.end(), h.begin(), h.end());
      o.insert(o.end(), packed.begin(), packed.end());
      continue;
    }
    for (const char c : std::string("-lh") + method + "-") h.push_back(uint8_t(c));
    std::vector<uint8_t> chain;  // level 1's extended headers, counted in the packed size
    if (level == 1 && !dir.empty()) {
      chain.push_back(0x02);
      chain.insert(chain.end(), dir.begin(), dir.end());
      put16(chain, 0);
    }
    put32(h, uint32_t(packed.size() + chain.size()));
    put32(h, uint32_t(data.size()));
    put32(h, 0);
    h.push_back(0x20);
    h.push_back(uint8_t(level));
    h.push_back(uint8_t(file.size()));
    h.insert(h.end(), file.begin(), file.end());
    put16(h, crc);
    if (level == 1) {
      h.push_back('U');
      put16(h, chain.empty() ? 0 : uint32_t(dir.size() + 3));
    }
    uint8_t sum = 0;
    for (const uint8_t v : h) sum = uint8_t(sum + v);
    o.push_back(uint8_t(h.size()));
    o.push_back(sum);
    o.insert(o.end(), h.begin(), h.end());
    o.insert(o.end(), chain.begin(), chain.end());
    o.insert(o.end(), packed.begin(), packed.end());
  }
  o.push_back(0);
  return o;
}

}  // namespace ge::vfs

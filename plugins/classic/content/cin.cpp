// The .cin reader and the fixture writer (cin.hpp).
//
// ge:summary The .cin reader (trees from the counts, frames decoded) and the fixture writer
// ge:keywords cin cinematic huffman decode encode fixture
// ge:skill classic
// ge:test tests/classic_content_cin_test.cpp
#include "classic/content/cin.hpp"

#include <algorithm>
#include <cstring>

namespace ge::classic::content {
namespace {

constexpr size_t kHeader = 20;
constexpr size_t kCounts = 65536;
constexpr uint32_t kMostBlock = 0x20000;  // a picture block's largest size

uint32_t u32(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 |
         uint32_t(b[at + 3]) << 24;
}

void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; i++) out.push_back(uint8_t(v >> (8 * i)));
}

// One context's tree from its row of counts: `kids` filled from node 256 on; the root returned
// (a leaf, 255, when fewer than two counts are non-zero).
uint16_t build_tree(std::span<const uint8_t> row, std::array<std::array<uint16_t, 2>, 255>& kids) {
  std::array<uint32_t, 511> count{};
  std::array<bool, 511> used{};
  for (int i = 0; i < 256; i++) count[i] = row[i];
  int made = 0;
  const auto smallest = [&](int nodes) {
    int best = -1;
    for (int n = 0; n < nodes; n++)
      if (!used[n] && count[n] != 0 && (best < 0 || count[n] < count[best])) best = n;
    return best;
  };
  while (made < 255) {
    const int nodes = 256 + made;
    const int a = smallest(nodes);
    if (a < 0) break;
    used[a] = true;
    const int b = smallest(nodes);
    if (b < 0) break;
    used[b] = true;
    kids[made] = {uint16_t(a), uint16_t(b)};
    count[nodes] = count[a] + count[b];
    made++;
  }
  return made == 0 ? uint16_t(255) : uint16_t(255 + made);
}

// Channel `ch` of sample `i` of a frame's sound at `at`: 16-bit signed, 8-bit mono unsigned,
// 8-bit stereo signed.
float sample_of(const Cin& c, const uint8_t* at, int64_t i, int ch) {
  const uint8_t* s =
      at + size_t(i) * size_t(c.sample_bytes * c.channels) + size_t(ch * c.sample_bytes);
  if (c.sample_bytes == 2) return float(int16_t(uint16_t(s[0] | s[1] << 8))) / 32768.0f;
  if (c.channels == 1) return (float(s[0]) - 128.0f) / 128.0f;
  return float(int8_t(s[0])) / 128.0f;
}

std::string fault(std::string_view where, const std::string& what, size_t at,
                  std::string_view next) {
  return std::string(where) + ": " + what + " at byte " + std::to_string(at) +
         "; next: " + std::string(next);
}

}  // namespace

bool open_cin(std::span<const uint8_t> bytes, std::string_view where, Cin* out,
              std::string* error) {
  Cin& c = *out;
  c = Cin{};
  if (bytes.size() < kHeader + kCounts) {
    *error = fault(where,
                   "the file is shorter than a cinematic's header and counts (" +
                       std::to_string(kHeader + kCounts) + " bytes)",
                   bytes.size(), "check the file is a Quake II .cin, whole");
    return false;
  }
  c.width = int(u32(bytes, 0));
  c.height = int(u32(bytes, 4));
  c.rate = int(u32(bytes, 8));
  c.sample_bytes = int(u32(bytes, 12));
  c.channels = int(u32(bytes, 16));
  if (c.width < 1 || c.width > 640 || c.height < 1 || c.height > 480) {
    *error = fault(where,
                   "the picture is " + std::to_string(c.width) + "x" + std::to_string(c.height) +
                       ", outside 1x1 to 640x480",
                   0, "check the file is a Quake II .cin");
    return false;
  }
  if (c.rate != 0 && (c.rate < 8000 || c.rate > 22050 || c.sample_bytes < 1 || c.sample_bytes > 2 ||
                      c.channels < 1 || c.channels > 2)) {
    *error =
        fault(where,
              "the sound is " + std::to_string(c.rate) + " Hz, " + std::to_string(c.sample_bytes) +
                  " bytes a sample, " + std::to_string(c.channels) +
                  " channels; a cinematic's is 8000 to 22050 "
                  "Hz, 1 or 2 bytes, 1 or 2 channels",
              8, "check the file is a Quake II .cin");
    return false;
  }
  for (int p = 0; p < 256; p++)
    c.root[p] = build_tree(bytes.subspan(kHeader + size_t(p) * 256, 256), c.kids[p]);
  c.bytes = bytes;
  c.at = kHeader + kCounts;
  return true;
}

CinStep next_cin_frame(Cin& c, CinFrame* out, std::string* error) {
  if (c.ended) return CinStep::end;
  const std::span<const uint8_t> b = c.bytes;
  const auto end = [&] {
    c.ended = true;
    return CinStep::end;
  };
  const auto bad = [&](const std::string& what, size_t at) {
    c.ended = true;
    *error = fault("cinematic", what + " in frame " + std::to_string(c.frame), at,
                   "the file is damaged; play it from a whole copy");
    return CinStep::fault;
  };
  if (c.at + 4 > b.size()) return end();
  const uint32_t command = u32(b, c.at);
  if (command >= 2) return end();
  size_t at = c.at + 4;
  if (command == 1) {
    if (at + 768 > b.size()) return bad("the palette runs past the file's end", at);
    std::memcpy(c.palette.data(), b.data() + at, 768);
    at += 768;
  }
  if (at + 4 > b.size()) return bad("the picture's size runs past the file's end", at);
  const uint32_t size = u32(b, at);
  at += 4;
  if (size < 4 || size > kMostBlock || at + size > b.size())
    return bad("the picture block's size " + std::to_string(size) +
                   " is outside 4 to 131072 or past the file's end",
               at - 4);
  const size_t pixels = size_t(c.width) * size_t(c.height);
  out->pixels.resize(pixels);
  const uint8_t* bits = b.data() + at + 4;
  const size_t nbits = size_t(size - 4) * 8;
  size_t bit = 0;
  uint16_t prev = 0;
  for (size_t i = 0; i < pixels; i++) {
    uint16_t node = c.root[prev];
    if (node < 256) return bad("a pixel follows a value whose tree has fewer than two values", at);
    while (node >= 256) {
      if (bit >= nbits) return bad("the picture's bits end before its pixels", at);
      const int one = (bits[bit >> 3] >> (bit & 7)) & 1;
      bit++;
      node = c.kids[prev][node - 256][one];
    }
    out->pixels[i] = uint8_t(node);
    prev = node;
  }
  at += size;
  out->palette = c.palette;
  out->left.clear();
  out->right.clear();
  if (c.rate > 0) {
    const int64_t first = c.frame * c.rate / kCinFps;
    const int64_t count = (c.frame + 1) * c.rate / kCinFps - first;
    const size_t frame_bytes = size_t(c.sample_bytes) * size_t(c.channels);
    if (at + size_t(count) * frame_bytes > b.size())
      return bad("the sound runs past the file's end", at);
    out->left.resize(size_t(count));
    out->right.resize(size_t(count));
    for (int64_t k = 0; k < count; k++) {
      out->left[size_t(k)] = sample_of(c, b.data() + at, k, 0);
      out->right[size_t(k)] = sample_of(c, b.data() + at, k, c.channels - 1);
    }
    at += size_t(count) * frame_bytes;
  }
  c.at = at;
  c.frame++;
  return CinStep::frame;
}

void cin_sound(const Cin& cin, std::vector<float>* left, std::vector<float>* right) {
  left->clear();
  right->clear();
  if (cin.rate <= 0) return;
  Cin walk = cin;
  const std::span<const uint8_t> b = walk.bytes;
  const size_t frame_bytes = size_t(walk.sample_bytes) * size_t(walk.channels);
  for (int64_t k = 0;; k++) {
    size_t at = walk.at;
    if (at + 4 > b.size() || u32(b, at) >= 2) return;
    at += u32(b, at) == 1 ? 4 + 768 : 4;
    if (at + 4 > b.size()) return;
    const uint32_t size = u32(b, at);
    at += 4 + size_t(size);
    const int64_t count = (k + 1) * walk.rate / kCinFps - k * walk.rate / kCinFps;
    if (at + size_t(count) * frame_bytes > b.size()) return;
    for (int64_t i = 0; i < count; i++) {
      left->push_back(sample_of(walk, b.data() + at, i, 0));
      right->push_back(sample_of(walk, b.data() + at, i, walk.channels - 1));
    }
    walk.at = at + size_t(count) * frame_bytes;
  }
}

std::vector<uint8_t> write_cin(int width, int height, std::span<const std::vector<uint8_t>> frames,
                               std::span<const uint8_t> palette, int rate, int channels,
                               std::span<const int16_t> sound) {
  std::vector<uint8_t> out;
  put32(out, uint32_t(width));
  put32(out, uint32_t(height));
  put32(out, uint32_t(rate));
  put32(out, rate > 0 ? 2u : 0u);
  put32(out, rate > 0 ? uint32_t(channels) : 0u);
  // The counts: how often each value follows each, scaled into 1..255 so every tree is whole.
  std::vector<uint64_t> seen(65536, 0);
  for (const std::vector<uint8_t>& f : frames) {
    uint8_t prev = 0;
    for (uint8_t v : f) {
      seen[size_t(prev) * 256 + v]++;
      prev = v;
    }
  }
  std::vector<uint8_t> counts(65536);
  for (int p = 0; p < 256; p++) {
    uint64_t most = 1;
    for (int v = 0; v < 256; v++) most = std::max(most, seen[size_t(p) * 256 + size_t(v)]);
    for (int v = 0; v < 256; v++) {
      const uint64_t n = seen[size_t(p) * 256 + size_t(v)];
      counts[size_t(p) * 256 + size_t(v)] = uint8_t(std::max<uint64_t>(1, n * 254 / most));
    }
  }
  out.insert(out.end(), counts.begin(), counts.end());
  // Each context's code for each value: the path from its root, bits in stream order.
  std::vector<std::vector<std::vector<uint8_t>>> code(256, std::vector<std::vector<uint8_t>>(256));
  for (int p = 0; p < 256; p++) {
    std::array<std::array<uint16_t, 2>, 255> kids{};
    const uint16_t root =
        build_tree(std::span<const uint8_t>(counts).subspan(size_t(p) * 256, 256), kids);
    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> walk{{root, {}}};
    while (!walk.empty()) {
      auto [node, path] = std::move(walk.back());
      walk.pop_back();
      if (node < 256) {
        code[size_t(p)][node] = std::move(path);
        continue;
      }
      for (uint8_t one = 0; one < 2; one++) {
        std::vector<uint8_t> next = path;
        next.push_back(one);
        walk.emplace_back(kids[node - 256][one], std::move(next));
      }
    }
  }
  int64_t k = 0;
  size_t sample = 0;
  for (const std::vector<uint8_t>& f : frames) {
    put32(out, k == 0 ? 1u : 0u);
    if (k == 0)
      for (int i = 0; i < 768; i++) out.push_back(i < int(palette.size()) ? palette[size_t(i)] : 0);
    std::vector<uint8_t> block;
    put32(block, uint32_t(f.size()));
    size_t bit = 0;
    uint8_t prev = 0;
    for (uint8_t v : f) {
      for (uint8_t one : code[prev][v]) {
        if ((bit & 7) == 0) block.push_back(0);
        block.back() = uint8_t(block.back() | one << (bit & 7));
        bit++;
      }
      prev = v;
    }
    put32(out, uint32_t(block.size()));
    out.insert(out.end(), block.begin(), block.end());
    if (rate > 0) {
      const int64_t count = (k + 1) * rate / kCinFps - k * rate / kCinFps;
      for (int64_t i = 0; i < count * channels; i++) {
        const int16_t s = sample < sound.size() ? sound[sample] : 0;
        sample++;
        out.push_back(uint8_t(uint16_t(s) & 0xff));
        out.push_back(uint8_t(uint16_t(s) >> 8));
      }
    }
    k++;
  }
  put32(out, 2);
  return out;
}

}  // namespace ge::classic::content

// The link's coder (link_code.hpp): the XOR with the reference through a bit tree per context,
// range coded, flushed short, its length coded first.
//
// ge:summary The link's coder: XOR with the reference, a bit tree per context, a short flush
// ge:keywords proxy link-codec code_datagram decode_datagram flush trailing-zeros
// ge:skill net
// ge:test tests/proxy_test.cpp
#include "proxy/link_code.hpp"

#include <cstring>

namespace ge::proxy {

namespace {

int context_at(std::span<const uint8_t> ref, size_t k) {
  return k < ref.size() ? ref[k] >> 4 : kCodeContexts - 1;
}

// The writer's low moved to the value in [low, low + range) with the most trailing zero bits, so
// its flush ends in zero bytes the reader does without.
void settle(replay::RangeWriter& w) {
  for (int k = 32; k > 0; k--) {
    const uint64_t mask = (uint64_t{1} << k) - 1;
    const uint64_t v = (w.low + mask) & ~mask;
    if (v >= w.low && v < w.low + w.range) {
      w.low = v;
      return;
    }
  }
}

}  // namespace

LinkModels::LinkModels() { tree.fill(replay::kProbHalf); }

size_t code_datagram(std::span<const uint8_t> d, std::span<const uint8_t> ref, LinkModels& models,
                     uint8_t* out, size_t capacity) {
  LinkModels m = models;
  std::vector<uint8_t> body;
  body.reserve(d.size() + 8);
  replay::RangeWriter w;
  replay::rc_begin(w, body);
  replay::put_int(w, m.size, static_cast<int32_t>(d.size()) - static_cast<int32_t>(ref.size()));
  for (size_t k = 0; k < d.size(); k++) {
    const uint8_t x = static_cast<uint8_t>(d[k] ^ (k < ref.size() ? ref[k] : 0));
    replay::Prob* tree = m.tree.data() + static_cast<size_t>(context_at(ref, k)) * 256;
    uint32_t node = 1;
    for (int i = 7; i >= 0; i--) {
      const int bit = (x >> i) & 1;
      replay::put_bit(w, tree[node], bit);
      node = (node << 1) | static_cast<uint32_t>(bit);
    }
  }
  settle(w);
  replay::rc_end(w);
  size_t end = body.size();
  while (end > 1 && body[end - 1] == 0) end--;
  const size_t n = end - 1;
  if (n >= d.size() || n > capacity) return 0;
  if (n > 0) std::memcpy(out, body.data() + 1, n);
  models = m;
  return n;
}

bool decode_datagram(std::span<const uint8_t> coded, std::span<const uint8_t> ref,
                     LinkModels& models, std::vector<uint8_t>& out, size_t most) {
  // The first byte the coder wrote (0), then what was kept; the reader reads zeros past it.
  std::vector<uint8_t> body(coded.size() + 1 + 5, 0);
  if (!coded.empty()) std::memcpy(body.data() + 1, coded.data(), coded.size());
  LinkModels m = models;
  replay::RangeReader r;
  replay::rc_begin(r, body.data(), body.size());
  const int64_t size = int64_t{replay::get_int(r, m.size)} + static_cast<int64_t>(ref.size());
  if (size < 0 || static_cast<uint64_t>(size) > most) return false;
  out.assign(static_cast<size_t>(size), 0);
  for (size_t k = 0; k < out.size(); k++) {
    replay::Prob* tree = m.tree.data() + static_cast<size_t>(context_at(ref, k)) * 256;
    uint32_t node = 1;
    for (int i = 0; i < 8; i++)
      node = (node << 1) | static_cast<uint32_t>(replay::get_bit(r, tree[node]));
    out[k] = static_cast<uint8_t>((node - 256) ^ (k < ref.size() ? ref[k] : 0));
  }
  models = m;
  return true;
}

}  // namespace ge::proxy

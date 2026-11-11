// The field codec's coder (engine/replay/codec.hpp): integers of every size, bytes and raw bits
// come back as written; a run of small residuals costs a fraction of their raw bytes; a cut stream
// is flagged; and the coder's output is the same bytes everywhere (a fixed size and FNV-1a digest
// of a seeded run: a change here is a change of format, which needs a new replay version).
//
// ge:summary Tests: the range coder and its integer and byte models round-trip, compress, are fixed
// ge:keywords test field-codec range-coder round-trip int byte raw truncated digest determinism
// ge:skill replay testing
// ge:test tests/replay_codec_test.cpp
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/prng.hpp"
#include "replay/codec.hpp"

namespace gr = ge::replay;

namespace {

// The seeded run's bytes as this coder writes them (the_output_is_the_same_bytes_everywhere).
constexpr size_t kWantSize = 5985;
constexpr uint64_t kWantDigest = 11726745568728421902ull;

uint64_t fnv1a(const std::vector<uint8_t>& b) {
  uint64_t h = 1469598103934665603ull;
  for (const uint8_t c : b) h = (h ^ c) * 1099511628211ull;
  return h;
}

// A seeded run mixing every kind of value: what the fixed-bytes test pins.
std::vector<int32_t> seeded_ints(uint32_t seed, int count) {
  ge::core::Rng rng = ge::core::make_rng(seed);
  std::vector<int32_t> v;
  for (int i = 0; i < count; i++) {
    const double pick = ge::core::next(rng);
    if (pick < 0.6)
      v.push_back(ge::core::int_in(rng, -3, 3));
    else if (pick < 0.9)
      v.push_back(ge::core::int_in(rng, -300, 300));
    else
      v.push_back(ge::core::int_in(rng, -2000000000, 2000000000));
  }
  return v;
}

}  // namespace

TEST(integers_of_every_size_come_back) {
  std::vector<int32_t> values = {
      0, 1, -1, 2, -2, 7, 8, -8, 255, 256, -32768, 32767, INT32_MAX, INT32_MIN, INT32_MIN + 1};
  const auto more = seeded_ints(7, 2000);
  values.insert(values.end(), more.begin(), more.end());
  std::vector<uint8_t> bytes;
  gr::RangeWriter w;
  gr::rc_begin(w, bytes);
  gr::IntModel m;
  for (const int32_t v : values) gr::put_int(w, m, v);
  gr::rc_end(w);
  gr::RangeReader r;
  gr::rc_begin(r, bytes.data(), bytes.size());
  gr::IntModel back;
  bool same = true;
  for (const int32_t v : values) same = same && gr::get_int(r, back) == v;
  CHECK(same);
  CHECK(!r.bad);
}

TEST(bytes_and_raw_bits_come_back_beside_integers) {
  const std::string text =
      "the quick brown fox jumps over the lazy dog, twice: the quick brown fox";
  std::vector<uint8_t> bytes;
  gr::RangeWriter w;
  gr::rc_begin(w, bytes);
  gr::ByteModel bm;
  gr::IntModel im;
  gr::Prob flag = gr::kProbHalf;
  uint8_t prev = 0;
  for (const char c : text) {
    gr::put_byte(w, bm, prev, static_cast<uint8_t>(c));
    prev = static_cast<uint8_t>(c);
    gr::put_int(w, im, c - 'a');
    gr::put_bit(w, flag, c == ' ' ? 1 : 0);
    gr::put_raw(w, static_cast<uint32_t>(c) * 77u, 17);
  }
  gr::rc_end(w);
  gr::RangeReader r;
  gr::rc_begin(r, bytes.data(), bytes.size());
  gr::ByteModel bm2;
  gr::IntModel im2;
  gr::Prob flag2 = gr::kProbHalf;
  prev = 0;
  bool same = true;
  for (const char c : text) {
    const uint8_t b = gr::get_byte(r, bm2, prev);
    prev = b;
    same = same && b == static_cast<uint8_t>(c);
    same = same && gr::get_int(r, im2) == c - 'a';
    same = same && gr::get_bit(r, flag2) == (c == ' ' ? 1 : 0);
    same = same && gr::get_raw(r, 17) == (static_cast<uint32_t>(c) * 77u & 0x1FFFFu);
  }
  CHECK(same);
  CHECK(!r.bad);
}

TEST(small_residuals_cost_a_fraction_of_their_bytes) {
  // 10000 residuals of a well-predicted field: mostly 0, sometimes ±1 or ±2. As int16 they are
  // 20000 bytes; the model should need well under a tenth of that.
  ge::core::Rng rng = ge::core::make_rng(11);
  std::vector<uint8_t> bytes;
  gr::RangeWriter w;
  gr::rc_begin(w, bytes);
  gr::IntModel m;
  for (int i = 0; i < 10000; i++) {
    const double p = ge::core::next(rng);
    gr::put_int(w, m, p < 0.8 ? 0 : p < 0.95 ? (p < 0.875 ? 1 : -1) : (p < 0.975 ? 2 : -2));
  }
  gr::rc_end(w);
  CHECK_MSG(bytes.size() < 2000, "10000 small residuals took " + std::to_string(bytes.size()) +
                                     " bytes; next: the integer model stopped adapting");
}

TEST(a_cut_stream_is_flagged) {
  const auto values = seeded_ints(3, 500);
  std::vector<uint8_t> bytes;
  gr::RangeWriter w;
  gr::rc_begin(w, bytes);
  gr::IntModel m;
  for (const int32_t v : values) gr::put_int(w, m, v);
  gr::rc_end(w);
  gr::RangeReader r;
  gr::rc_begin(r, bytes.data(), bytes.size() / 2);
  gr::IntModel back;
  for (size_t i = 0; i < values.size(); i++) gr::get_int(r, back);
  CHECK(r.bad);
}

TEST(the_output_is_the_same_bytes_everywhere) {
  const auto values = seeded_ints(2026, 4000);
  std::vector<uint8_t> bytes;
  gr::RangeWriter w;
  gr::rc_begin(w, bytes);
  gr::IntModel m;
  gr::ByteModel bm;
  for (const int32_t v : values) {
    gr::put_int(w, m, v);
    gr::put_byte(w, bm, static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 3));
  }
  gr::rc_end(w);
  CHECK_MSG(bytes.size() == kWantSize && fnv1a(bytes) == kWantDigest,
            "the coder wrote " + std::to_string(bytes.size()) + " bytes, digest " +
                std::to_string(fnv1a(bytes)) +
                "; next: a deliberate change of the coder is a new replay format version: "
                "update kWantSize and kWantDigest with it");
}

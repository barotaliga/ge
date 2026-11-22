// The deflater (core/deflate.hpp): every stream it writes inflates back to its input.
//
// ge:summary The deflater's streams read back by the inflater: text, noise, runs, empty
// ge:keywords deflate inflate round-trip
// ge:skill determinism
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/deflate.hpp"
#include "core/inflate.hpp"

namespace {

void round_trip(const std::vector<uint8_t>& in) {
  const std::vector<uint8_t> z = ge::core::deflate_raw(in);
  const auto back = ge::core::inflate_raw(z, in.size());
  CHECK(back.has_value());
  if (back) CHECK(*back == in);
}

std::vector<uint8_t> bytes_of(const std::string& s) { return {s.begin(), s.end()}; }

}  // namespace

TEST(empty_and_short_inputs_read_back) {
  round_trip({});
  round_trip({7});
  round_trip({1, 2});
  round_trip(bytes_of("abc"));
}

TEST(text_reads_back_smaller) {
  std::string text;
  for (int i = 0; i < 200; i++)
    text += "models/items/armor/body/tris.md2 " + std::to_string(i % 13);
  const std::vector<uint8_t> in = bytes_of(text);
  const std::vector<uint8_t> z = ge::core::deflate_raw(in);
  CHECK(z.size() * 4 < in.size());
  round_trip(in);
}

TEST(runs_and_long_matches_read_back) {
  std::vector<uint8_t> in(70000, 0);
  for (size_t i = 0; i < in.size(); i += 997) in[i] = static_cast<uint8_t>(i);
  round_trip(in);
  std::vector<uint8_t> two(600, 'x');
  round_trip(two);
}

TEST(noise_reads_back) {
  std::vector<uint8_t> in(5000);
  uint32_t x = 12345;
  for (uint8_t& b : in) {
    x = x * 1103515245u + 12345u;
    b = static_cast<uint8_t>(x >> 16);
  }
  round_trip(in);
}

TEST(the_same_input_gives_the_same_stream) {
  const std::vector<uint8_t> in = bytes_of("configstrings and baselines, configstrings again");
  CHECK(ge::core::deflate_raw(in) == ge::core::deflate_raw(in));
}

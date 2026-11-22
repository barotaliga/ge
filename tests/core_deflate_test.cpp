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

TEST(adler32_is_the_rfc_one) {
  CHECK(ge::core::adler32(bytes_of("Wikipedia").data(), 9) == 0x11E60398u);
}

TEST(a_stream_fed_in_pieces_reads_back_as_zlib_and_gzip) {
  for (const auto wrap : {ge::core::DeflateWrap::zlib, ge::core::DeflateWrap::gzip}) {
    ge::core::DeflateStream s;
    s.wrap = wrap;
    std::vector<uint8_t> out, all;
    for (int i = 0; i < 5; i++) {
      const std::vector<uint8_t> piece =
          bytes_of("frame " + std::to_string(i) + " players and entities");
      all.insert(all.end(), piece.begin(), piece.end());
      const size_t before = out.size();
      ge::core::deflate_sync(s, piece, out);
      // A sync flush ends each piece with the empty stored block's lengths.
      CHECK(out.size() > before + 4);
      CHECK(out[out.size() - 2] == 0xFF && out[out.size() - 1] == 0xFF);
    }
    ge::core::deflate_sync(s, {}, out);  // an empty piece: a flush alone
    ge::core::deflate_finish(s, out);
    const auto back = wrap == ge::core::DeflateWrap::zlib ? ge::core::inflate_zlib(out)
                                                          : ge::core::inflate_gzip(out);
    CHECK(back.has_value());
    if (back) CHECK(*back == all);
  }
}

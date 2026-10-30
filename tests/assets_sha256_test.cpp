// SHA-256 against FIPS 180-4's published vectors (the empty message, "abc", the two-block
// message), and a stream fed in odd pieces giving the same digest as the whole buffer.
//
// ge:summary Tests SHA-256 against the standard's vectors and streamed in pieces
// ge:keywords test sha256 hash digest vector stream
// ge:skill assets
#include <string>
#include <vector>

#include "assets/sha256.hpp"
#include "check.hpp"

namespace {

std::span<const uint8_t> bytes_of(const std::string& s) {
  return {reinterpret_cast<const uint8_t*>(s.data()), s.size()};
}

}  // namespace

TEST(sha256_matches_the_standards_vectors) {
  CHECK(ge::assets::sha256_hex({}) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(ge::assets::sha256_hex(bytes_of("abc")) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(ge::assets::sha256_hex(
            bytes_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(sha256_streamed_in_pieces_is_the_whole) {
  std::vector<uint8_t> data(1000);
  for (size_t i = 0; i < data.size(); i++) data[i] = uint8_t(i * 7 + 3);
  ge::assets::Sha256 s;
  size_t at = 0;
  for (size_t piece = 1; at < data.size(); piece = piece * 3 % 97 + 1) {
    const size_t n = std::min(piece, data.size() - at);
    s.update(std::span(data).subspan(at, n));
    at += n;
  }
  const auto streamed = s.finish();
  CHECK(ge::assets::to_hex(streamed) == ge::assets::sha256_hex(data));
}

// Tests: SHA-1 (engine/net/sha1.hpp) against the standard's examples.
//
// ge:summary Tests: SHA-1 and its hex against the standard's examples
// ge:keywords test sha1 digest
// ge:skill net testing
#include "check.hpp"
#include "net/sha1.hpp"

TEST(sha1_matches_the_standard_s_examples) {
  CHECK(ge::net::sha1_hex("abc") == "A9993E364706816ABA3E25717850C26C9CD0D89D");
  CHECK(ge::net::sha1_hex("") == "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
  CHECK(ge::net::sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "84983E441C3BD26EBAAE4AA1F95129E5E54670F1");
}

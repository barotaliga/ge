// engine/net/crypto.hpp: XChaCha20-Poly1305 against the published test vector (the XChaCha draft's
// appendix A.3.1), a changed byte anywhere refused with the text left as it came, keys derived
// apart, and keys as hex and from the environment.
//
// ge:summary Test: the network's seal and open against the vector, tampering, derivation, hex keys
// ge:keywords test net crypto aead xchacha20 poly1305 vector tamper derive hex environment
// ge:skill net testing
#include "net/crypto.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

std::vector<uint8_t> hex(const char* s) {
  std::vector<uint8_t> out;
  for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
    out.push_back(static_cast<uint8_t>(std::stoi(std::string(s + i, 2), nullptr, 16)));
  }
  return out;
}

Key key_of(uint8_t first) {
  Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(first + i);
  return k;
}

}  // namespace

TEST(crypto_seals_the_published_xchacha20_poly1305_vector) {
  const char* text =
      "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the "
      "future, sunscreen would be it.";
  std::vector<uint8_t> data(text, text + std::strlen(text));
  const std::vector<uint8_t> ad = hex("50515253c0c1c2c3c4c5c6c7");
  const std::vector<uint8_t> nonce = hex("404142434445464748494a4b4c4d4e4f5051525354555657");
  const Key key = key_of(0x80);
  uint8_t tag[kTagBytes];
  seal(data.data(), data.size(), tag, key, nonce.data(), ad.data(), ad.size());
  const std::vector<uint8_t> want = hex(
      "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb731c7f1b0b4aa6440bf3a82f"
      "4eda7e39ae64c6708c54c216cb96b72e1213b4522f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f"
      "76b2383565d3fff921f9664c97637da9768812f615c68b13b52e");
  CHECK(data == want);
  CHECK(std::memcmp(tag, hex("c0875924c1c7987947deafd8780acf49").data(), kTagBytes) == 0);
  CHECK(open(data.data(), data.size(), tag, key, nonce.data(), ad.data(), ad.size()));
  CHECK(std::memcmp(data.data(), text, data.size()) == 0);
}

TEST(crypto_refuses_a_changed_byte_and_leaves_the_text) {
  const Key key = key_of(1);
  uint8_t nonce[kNonceBytes];
  counter_nonce(nonce, 77, 6);
  const uint8_t ad[5] = {1, 2, 3, 4, 5};
  uint8_t text[32];
  for (int i = 0; i < 32; i++) text[i] = static_cast<uint8_t>(i);
  uint8_t tag[kTagBytes];
  seal(text, sizeof text, tag, key, nonce, ad, sizeof ad);
  uint8_t sealed[32];
  std::memcpy(sealed, text, sizeof text);

  // A flipped bit in the text, the tag, the associated data, the key or the nonce: none opens,
  // and the text stays sealed as it was.
  text[7] ^= 1;
  CHECK(!open(text, sizeof text, tag, key, nonce, ad, sizeof ad));
  text[7] ^= 1;
  CHECK(std::memcmp(text, sealed, sizeof text) == 0);
  tag[0] ^= 0x80;
  CHECK(!open(text, sizeof text, tag, key, nonce, ad, sizeof ad));
  tag[0] ^= 0x80;
  uint8_t other_ad[5] = {1, 2, 3, 4, 6};
  CHECK(!open(text, sizeof text, tag, key, nonce, other_ad, sizeof other_ad));
  CHECK(!open(text, sizeof text, tag, key_of(2), nonce, ad, sizeof ad));
  uint8_t other_nonce[kNonceBytes];
  counter_nonce(other_nonce, 77, 7);  // the same counter, another kind
  CHECK(!open(text, sizeof text, tag, key, other_nonce, ad, sizeof ad));
  CHECK(std::memcmp(text, sealed, sizeof text) == 0);
  CHECK(open(text, sizeof text, tag, key, nonce, ad, sizeof ad));
  CHECK(text[31] == 31);
}

TEST(crypto_derives_keys_apart) {
  const Key master = key_of(9);
  const Key a = derive_key(master, "hostkey", 1);
  CHECK(derive_key(master, "hostkey", 1) == a);
  CHECK(derive_key(master, "hostkey", 2) != a);
  CHECK(derive_key(master, "other", 1) != a);
  CHECK(derive_key(key_of(10), "hostkey", 1) != a);
  CHECK(a != master);
}

TEST(crypto_reads_keys_as_hex_and_from_the_environment) {
  const Key k = key_of(0xa0);
  Key back{};
  std::string error;
  CHECK(key_from_hex(key_to_hex(k), back, error) && back == k);
  CHECK(!key_from_hex("00ff", back, error) && error.find("64 hex digits") != std::string::npos);
  std::string bad = key_to_hex(k);
  bad[10] = 'g';
  CHECK(!key_from_hex(bad, back, error) && error.find("character 10") != std::string::npos);
  CHECK(!key_from_env("GE_NET_TEST_KEY_THAT_IS_NEVER_SET", back, error));
  CHECK(error.find("GE_NET_TEST_KEY_THAT_IS_NEVER_SET is not set") != std::string::npos);
}

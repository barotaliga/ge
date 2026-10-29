// The network's cryptography (net/crypto.hpp) over Monocypher: the only file that includes it.
//
// ge:summary The network's cryptography over Monocypher: seal, open, derive, hex keys, wipe
// ge:keywords net crypto monocypher crypto_aead_lock crypto_aead_unlock crypto_blake2b_keyed
// ge:keywords xchacha20 poly1305 blake2b getenv
// ge:skill net
// ge:test tests/net_crypto_test.cpp
#include "net/crypto.hpp"

#include <monocypher.h>

#include <cstdlib>
#include <cstring>

namespace ge::net {

void seal(uint8_t* text, size_t size, uint8_t* tag, const Key& key, const uint8_t* nonce,
          const uint8_t* ad, size_t ad_size) {
  crypto_aead_lock(text, tag, key.data(), nonce, ad, ad_size, text, size);
}

bool open(uint8_t* text, size_t size, const uint8_t* tag, const Key& key, const uint8_t* nonce,
          const uint8_t* ad, size_t ad_size) {
  // Monocypher checks the tag before it writes a byte: a failure leaves `text` as it came.
  return crypto_aead_unlock(text, tag, key.data(), nonce, ad, ad_size, text, size) == 0;
}

Key derive_key(const Key& master, std::string_view context, uint64_t id) {
  uint8_t message[16] = {};
  std::memcpy(message, context.data(), context.size() < 8 ? context.size() : 8);
  for (int i = 0; i < 8; i++) message[8 + i] = static_cast<uint8_t>(id >> (8 * i));
  Key out{};
  crypto_blake2b_keyed(out.data(), out.size(), master.data(), master.size(), message,
                       sizeof message);
  return out;
}

void counter_nonce(uint8_t* nonce, uint64_t counter, uint8_t kind) {
  std::memset(nonce, 0, kNonceBytes);
  for (int i = 0; i < 8; i++) nonce[i] = static_cast<uint8_t>(counter >> (8 * i));
  nonce[8] = kind;
}

namespace {

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

bool key_from_hex(std::string_view hex, Key& out, std::string& error) {
  if (hex.size() != 64) {
    error = "a key is 64 hex digits; this one has " + std::to_string(hex.size()) + " characters";
    return false;
  }
  for (size_t i = 0; i < 32; i++) {
    const int hi = hex_digit(hex[2 * i]);
    const int lo = hex_digit(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) {
      error = "a key is 64 hex digits; character " + std::to_string(2 * i + (hi < 0 ? 0 : 1)) +
              " is not one";
      return false;
    }
    out[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  return true;
}

std::string key_to_hex(const Key& key) {
  static const char kDigits[] = "0123456789abcdef";
  std::string s(64, '0');
  for (size_t i = 0; i < 32; i++) {
    s[2 * i] = kDigits[key[i] >> 4];
    s[2 * i + 1] = kDigits[key[i] & 15];
  }
  return s;
}

bool key_from_env(const char* name, Key& out, std::string& error) {
  const char* value = std::getenv(name);
  if (!value || !*value) {
    error = std::string("net: ") + name +
            " is not set; set it to 64 hex digits (a GitHub secret on CI, the environment or a "
            "gitignored .env locally, never the tree); skill: net";
    return false;
  }
  std::string why;
  if (!key_from_hex(value, out, why)) {
    error = std::string("net: ") + name + ": " + why + "; skill: net";
    return false;
  }
  return true;
}

void wipe(void* secret, size_t size) { crypto_wipe(secret, size); }

}  // namespace ge::net

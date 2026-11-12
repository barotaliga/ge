// The network's cryptography (net/crypto.hpp) over Monocypher: the only file that includes it.
//
// ge:summary The network's cryptography over Monocypher: seal, open, derive, hex keys, wipe
// ge:keywords net crypto monocypher crypto_aead_lock crypto_aead_unlock crypto_blake2b_keyed
// ge:keywords xchacha20 poly1305 blake2b getenv crypto_argon2 argon2id crypto_blake2b
// ge:keywords crypto_eddsa_sign crypto_eddsa_check crypto_x25519 crypto_eddsa_to_x25519
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

void password_hash(uint8_t out[32], std::string_view secret, const uint8_t* salt, uint32_t kib,
                   uint32_t passes, void* work) {
  crypto_argon2_config config{};
  config.algorithm = CRYPTO_ARGON2_ID;
  config.nb_blocks = kib < 8 ? 8 : kib;  // a block is 1 KiB; Argon2 needs at least 8
  config.nb_passes = passes < 1 ? 1 : passes;
  config.nb_lanes = 1;
  crypto_argon2_inputs inputs{};
  inputs.pass = reinterpret_cast<const uint8_t*>(secret.data());
  inputs.pass_size = static_cast<uint32_t>(secret.size());
  inputs.salt = salt;
  inputs.salt_size = static_cast<uint32_t>(kSaltBytes);
  crypto_argon2(out, 32, work, config, inputs, crypto_argon2_no_extras);
}

Key digest(std::string_view data) {
  Key out{};
  crypto_blake2b(out.data(), out.size(), reinterpret_cast<const uint8_t*>(data.data()),
                 data.size());
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

SignKeys sign_keys(const Key& seed) {
  SignKeys k;
  Key copy = seed;  // the key pair wipes the seed it is given
  crypto_eddsa_key_pair(k.secret.data(), k.pub.data(), copy.data());
  return k;
}

Signature sign(const SignKeys& keys, const uint8_t* message, size_t size) {
  Signature s{};
  crypto_eddsa_sign(s.data(), keys.secret.data(), message, size);
  return s;
}

bool sign_check(const Key& pub, const Signature& signature, const uint8_t* message, size_t size) {
  return crypto_eddsa_check(signature.data(), pub.data(), message, size) == 0;
}

Key box_secret(const Key& seed) {
  // The signing scalar is the first half of BLAKE2b-512 of the seed; X25519 trims it the same way,
  // so this secret's public key is the signing key's, converted.
  uint8_t wide[64];
  crypto_blake2b(wide, sizeof wide, seed.data(), seed.size());
  Key out{};
  std::memcpy(out.data(), wide, 32);
  crypto_wipe(wide, sizeof wide);
  return out;
}

Key box_public(const Key& secret) {
  Key out{};
  crypto_x25519_public_key(out.data(), secret.data());
  return out;
}

Key box_public_of(const Key& sign_public) {
  Key out{};
  crypto_eddsa_to_x25519(out.data(), sign_public.data());
  return out;
}

Key box_key(const Key& my_secret, const Key& their_public, const Key& asker_public,
            const Key& answerer_public) {
  uint8_t mix[96];
  crypto_x25519(mix, my_secret.data(), their_public.data());
  std::memcpy(mix + 32, asker_public.data(), 32);
  std::memcpy(mix + 64, answerer_public.data(), 32);
  Key out{};
  crypto_blake2b(out.data(), out.size(), mix, sizeof mix);
  crypto_wipe(mix, sizeof mix);
  return out;
}

}  // namespace ge::net

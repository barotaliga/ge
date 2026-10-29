// THE NETWORK'S CRYPTOGRAPHY, the few calls security needs (docs/networking.md § Security): a
// 32-byte key, sealing and opening bytes with XChaCha20-Poly1305 (authenticated encryption with
// associated data: what is sealed cannot be read, and neither it nor the associated data can be
// changed, without the key), and deriving a key from another with keyed BLAKE2b. Monocypher
// (cmake/Crypto.cmake) does the work; crypto.cpp is the only file that includes it.
//
// Every call is integer arithmetic on bytes: the same inputs seal to the same bytes on every
// platform, so a secure session over the simulated network keeps its digest. Nothing here draws
// randomness or reads a clock: the nonces are the caller's (a packet's counter, a token's drawn
// bytes) and a NONCE IS NEVER USED TWICE WITH ONE KEY.
//
// Keys reach a program from the environment (`key_from_env`: 64 hex digits), never from the tree.
//
// ge:summary The network's cryptography: XChaCha20-Poly1305 seal and open, BLAKE2b key derivation
// ge:keywords net crypto cryptography aead xchacha20 poly1305 seal open encrypt decrypt mac tag
// ge:keywords blake2b kdf derive key nonce monocypher wipe hex environment secret
// ge:skill net
// ge:test tests/net_crypto_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ge::net {

using Key = std::array<uint8_t, 32>;
constexpr size_t kNonceBytes = 24;
constexpr size_t kTagBytes = 16;  // what sealing adds

// Seal `size` bytes of `text` in place with `key` and `nonce` (kNonceBytes), binding `ad` (read,
// not sealed) to them; the tag (kTagBytes) is written to `tag`.
void seal(uint8_t* text, size_t size, uint8_t* tag, const Key& key, const uint8_t* nonce,
          const uint8_t* ad, size_t ad_size);
// Open what `seal` made, in place. False, with `text` left as it came, when the tag does not
// match: the wrong key, the wrong nonce, or a byte of `text` or `ad` changed.
bool open(uint8_t* text, size_t size, const uint8_t* tag, const Key& key, const uint8_t* nonce,
          const uint8_t* ad, size_t ad_size);

// A key for `id` under `context` (an 8-character label) from `master`: keyed BLAKE2b of the
// label and the id. One master key derives every host's, and a host's key derives no other.
Key derive_key(const Key& master, std::string_view context, uint64_t id);

// The nonce of a packet counted on one stream: the counter (little endian), then `kind`, then
// zeros. A stream's counter never repeats, and two kinds on one key never share a nonce.
void counter_nonce(uint8_t* nonce, uint64_t counter, uint8_t kind);

// 64 hex digits to a key; false (and `error` says why) for anything else.
bool key_from_hex(std::string_view hex, Key& out, std::string& error);
std::string key_to_hex(const Key& key);
// The key in the environment variable `name` (64 hex digits); false with `error` naming the
// variable and what to set it to when it is unset or malformed.
bool key_from_env(const char* name, Key& out, std::string& error);

// Overwrite a secret before its memory is let go.
void wipe(void* secret, size_t size);

}  // namespace ge::net

// THE NETWORK'S CRYPTOGRAPHY, the few calls security needs (docs/networking.md § Security): a
// 32-byte key, signatures and key exchange, sealing and opening bytes with XChaCha20-Poly1305
// (authenticated encryption with associated data: what is sealed cannot be read, and neither it nor
// the associated data can be changed, without the key), and deriving a key from another with keyed
// BLAKE2b. Monocypher (cmake/Crypto.cmake) does the work; crypto.cpp is the only file that includes
// it.
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
// ge:keywords argon2 argon2id password_hash password-hash salt digest
// ge:keywords ed25519 eddsa signature sign sign_check x25519 key-exchange box_key
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

// A password's slow hash: Argon2id over `secret` and `salt` (kSaltBytes) into 32 bytes, costing
// `kib` KiB of `work` (the caller's, at least kib * 1024 bytes, so a store hashes without
// allocating) and `passes` passes over it. The cost is what makes a stolen store slow to guess
// from; the `accounts` lab times it at 19 and 64 MiB.
constexpr size_t kSaltBytes = 16;
void password_hash(uint8_t out[32], std::string_view secret, const uint8_t* salt, uint32_t kib,
                   uint32_t passes, void* work);

// A fast hash: BLAKE2b of `data` into 32 bytes. For a secret that is already long and random (a
// one-time code, a recovery code), where a slow hash buys nothing.
Key digest(std::string_view data);

// SIGNATURES (Ed25519 over BLAKE2b): a key pair grown from a 32-byte seed, a 64-byte signature
// anyone holding the public key checks. A master signs its tickets with one (account/ticket.hpp).
using Signature = std::array<uint8_t, 64>;
struct SignKeys {
  std::array<uint8_t, 64> secret{};  // the seed, then the public key
  Key pub{};
};
SignKeys sign_keys(const Key& seed);
Signature sign(const SignKeys& keys, const uint8_t* message, size_t size);
bool sign_check(const Key& pub, const Signature& signature, const uint8_t* message, size_t size);

// KEY EXCHANGE (X25519). The same seed's exchange secret pairs with `box_public_of(keys.pub)`, so
// one published key both checks a master's signatures and seals a request to it. `box_key` is
// the key two sides share: BLAKE2b of the raw exchange and both public keys, the asker's first.
Key box_secret(const Key& seed);
Key box_public(const Key& secret);          // the exchange public key of an exchange secret
Key box_public_of(const Key& sign_public);  // the exchange public key of a signing key
Key box_key(const Key& my_secret, const Key& their_public, const Key& asker_public,
            const Key& answerer_public);

// Overwrite a secret before its memory is let go.
void wipe(void* secret, size_t size);

}  // namespace ge::net

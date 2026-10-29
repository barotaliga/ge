// CONNECT TOKENS (docs/networking.md § Security, M17i): how a host knows a client was let in by
// the matchmaker, and how the two get keys no one else has. The matchmaker (the game's service,
// M17j) holds one MASTER key and derives each host's from it (`host_key`: keyed BLAKE2b of the
// host's id), so a host can open the tokens made for it and can neither open nor make any other
// host's. For each client it lets in, it makes a token:
//
//   what the client is given (over the matchmaker's own secure channel, kTokenBytes):
//     client id (64), host id (64), expiry (64), the key to the host (256), the key to the client
//     (256), and the SEALED part, which the client cannot read and forwards unopened
//   the sealed part (kSealedTokenBytes, carried in the handshake's request and response):
//     host id (64), expiry (64), nonce (192) | sealed: client id, both keys, the user's 32 bytes |
//     tag (128). The protocol id, the host id and the expiry are its associated data.
//
// Opening it proves the matchmaker made it (no one without the host's key can make a tag that
// opens), for this host, for this game, and that it is unchanged; the expiry is checked against the
// host's clock. The two keys then seal every packet each way (node.hpp). A token stolen in flight
// is no use without its keys, and one replayed from another address is refused by the host's table
// of tokens seen (node.cpp).
//
// The token's times are on the matchmaker's and the host's shared clock (Unix microseconds in a
// real game; the virtual clock in a lab): `NodeSecurity::clock_offset_us` turns a node's clock
// into it. The randomness (the nonce and the two keys) is the caller's: the operating system's
// in a real game (`os_random_bytes`), a seeded stream in a lab.
//
// ge:summary Connect tokens: made by the matchmaker under a host's derived key, opened by the host
// ge:keywords net token connect-token matchmaker host-key master-key expiry client-id user-data
// ge:keywords sealed private-token signed security authentication session-keys replay
// ge:skill net
// ge:test tests/net_token_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "net/crypto.hpp"

namespace ge::net {

constexpr size_t kTokenUserBytes = 32;
constexpr size_t kSealedTokenPrivate = 8 + 32 + 32 + kTokenUserBytes;
constexpr size_t kSealedTokenBytes = 8 + 8 + kNonceBytes + kSealedTokenPrivate + kTagBytes;
constexpr size_t kTokenBytes = 8 + 8 + 8 + 32 + 32 + kSealedTokenBytes;
// The random bytes making a token takes: its nonce and the two keys.
constexpr size_t kTokenRandomBytes = kNonceBytes + 32 + 32;

// What the matchmaker grants.
struct TokenGrant {
  uint64_t client_id = 0;  // who the client is to the game (an account's id, M17j)
  uint64_t host_id = 0;    // the host it may join
  int64_t expire_us = 0;   // on the shared clock: no handshake starts after it
  std::array<uint8_t, kTokenUserBytes> user{};  // the game's: what the host learns of the client
};

// The token as the client holds it.
struct ConnectToken {
  uint64_t client_id = 0;
  uint64_t host_id = 0;
  int64_t expire_us = 0;
  Key to_host{};    // seals what the client sends
  Key to_client{};  // seals what the host sends
  std::array<uint8_t, kSealedTokenBytes> sealed{};
};

// The token as the host opened it.
struct OpenedToken {
  uint64_t client_id = 0;
  uint64_t host_id = 0;
  int64_t expire_us = 0;
  Key to_host{}, to_client{};
  std::array<uint8_t, kTokenUserBytes> user{};
  std::array<uint8_t, kTagBytes> tag{};  // names this token in the host's table of tokens seen
};

// The host's key under the matchmaker's master key.
Key host_key(const Key& master, uint64_t host_id);

// The matchmaker's half: a token for `grant` under `master`, for the game `protocol`, from
// kTokenRandomBytes of `random`.
ConnectToken token_make(const Key& master, uint32_t protocol, const TokenGrant& grant,
                        const uint8_t* random);
// The host's half: open a sealed part with its own key (host_key). False when it does not open
// (made under another key, for another game, or changed); the expiry is the caller's to check.
bool token_open(const Key& key, uint32_t protocol, const uint8_t* sealed, OpenedToken& out);

// The client's copy as bytes (kTokenBytes), for the matchmaker to send and the client to read.
void token_write(const ConnectToken& t, uint8_t* out);
void token_read(const uint8_t* in, ConnectToken& t);

// `size` bytes from the operating system's random source (net/udp_socket.hpp's os_entropy): what
// a real matchmaker makes a token from. A lab draws them from its seed instead.
void os_random_bytes(uint8_t* out, size_t size);

}  // namespace ge::net

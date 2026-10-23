// THE RENDEZVOUS'S MESSAGES, stated once for the server (rendezvous.hpp) and the two ends that
// use it (reach.hpp), and not installed: a game reaches a host through reach.hpp. Every message is
// the magic "geRV", a kind byte and its fields, little endian; an address is its family byte, 16
// bytes and its port. None is bigger than the message that asked for it.
//
//   to the server                     from the server
//   register  session u64, port u16   registered  public address
//   join      session u64, nonce u64  peer        address, nonce u64, relay address, key u64
//                                     unknown     session u64
//   to the relay's port               between the two ends
//   bind      key u64, role           punch       nonce u64
//
// A join answers the client with the host's public address and the host with the client's, both
// with the same nonce, relay port and key. A register's port is the one the host's router forwards
// to it (0, the padding's, when none: portmap.hpp). STUN shares the server's port (stun.hpp).
//
// ge:summary The rendezvous's messages: register, join, peer, punch, bind, encoded and read
// ge:keywords net rendezvous reach wire register join peer punch relay bind nonce encode decode
// ge:skill net
// ge:test tests/net_reach_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "net/address.hpp"
#include "net/wire.hpp"

namespace ge::net::reach_wire {

constexpr uint8_t kMagic[4] = {'g', 'e', 'R', 'V'};

enum Kind : uint8_t {
  kRegister = 1,
  kJoin = 2,
  kBind = 3,
  kPunch = 4,
  kRegistered = 20,
  kPeer = 21,
  kUnknown = 22,
};

enum Role : uint8_t { kHost = 0, kClient = 1 };

// The largest message: peer (magic, kind, two addresses, two u64s).
constexpr size_t kMaxSize = 4 + 1 + 19 + 8 + 19 + 8;

inline void write_start(Writer& w, Kind k) {
  write_bytes(w, kMagic, 4);
  write_u8(w, k);
}

// The datagram is the rendezvous's: its kind, with `r` past it; false for anything else.
inline bool read_start(Reader& r, Kind& k) {
  const uint8_t* m = read_view(r, 4);
  if (!m || std::memcmp(m, kMagic, 4) != 0) return false;
  k = static_cast<Kind>(read_u8(r));
  return !r.bad;
}

inline void write_address(Writer& w, const Address& a) {
  write_u8(w, static_cast<uint8_t>(a.family));
  write_bytes(w, a.bytes, 16);
  write_u16(w, a.port);
}

inline Address read_address(Reader& r) {
  Address a;
  const uint8_t f = read_u8(r);
  a.family = f <= static_cast<uint8_t>(Family::sim) ? static_cast<Family>(f) : Family::none;
  read_bytes(r, a.bytes, 16);
  a.port = read_u16(r);
  return a;
}

}  // namespace ge::net::reach_wire

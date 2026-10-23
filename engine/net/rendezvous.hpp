// THE RENDEZVOUS: the small public server two players behind home routers use to find each other.
// A host REGISTERS its session (the server sees the public address its NAT gave it, and says so);
// a client JOINS by the session's id, and the server tells each the other's public address with
// a nonce, so both can PUNCH: send to each other at once, each opening its own NAT to the other
// (reach.hpp does it). Where no hole opens (a symmetric NAT against a port-restricted or
// symmetric one), the pair falls back on the server's RELAY: its own port for that pair,
// bound by both ends with the pair's key, forwarding every datagram from one to the other
// unread. It answers STUN on its own port (stun.hpp), so a node can learn its public address
// without registering.
//
// It is the server half of M17e; a game runs it beside its lobby service, or on any public
// machine. Like the node, it takes the time from the caller, opens its relays on its transport's
// kind (a UDP port, or a socket on the simulated network), and keeps a fixed table sized at init:
//
//   rendezvous_init(rv, config, transport)
//   every frame: rendezvous_update(rv, now)
//
// No reply is bigger than the datagram that asked for it (an ask is padded to the biggest answer),
// so it cannot be used to amplify a flood; STUN's answer alone is 12 bytes over its 20-byte
// request, as at every STUN server. A relay forwards only between the two addresses that bound it
// with the key.
//
// ge:summary The rendezvous server: sessions registered, joins answered, STUN, the relay ports
// ge:keywords net rendezvous nat-traversal hole-punching introduce register join relay turn
// ge:keywords stun public-address matchmaking-server master-server punch
// ge:skill net
// ge:test tests/net_reach_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"

namespace ge::net {

struct RendezvousConfig {
  Address public_address;              // the server as players reach it (none: its transport's)
  int max_sessions = 1024;             // registered sessions held at once
  int max_relays = 64;                 // relay ports open at once
  int64_t session_life_us = 30000000;  // a session not registered again by then is dropped
  int64_t relay_idle_us = 30000000;    // a relay with no traffic this long is closed
  uint16_t first_relay_port = 50000;   // the simulated network's relay ports (UDP: the OS picks)
  uint64_t seed = 0;                   // the relays' keys (a real server passes os_entropy())
};

struct RendezvousStats {
  uint64_t stun = 0;        // STUN binding requests answered
  uint64_t registered = 0;  // register messages answered
  uint64_t joined = 0;      // joins introduced to their host
  uint64_t unknown = 0;     // joins for a session nobody registered
  uint64_t relays = 0;      // relay ports opened
  uint64_t no_relay = 0;    // joins introduced without one, every relay taken
  uint64_t relayed = 0, relayed_bytes = 0;
  uint64_t refused = 0;  // anything else: a stranger at a relay, a short or unknown message
};

struct RendezvousSession {
  uint64_t id = 0;
  Address host;
  int64_t last_us = 0;
};

struct Relay {
  bool open = false;
  Transport transport;
  Address address;  // as players reach it
  uint64_t session = 0, nonce = 0, key = 0;
  Address host, client;  // bound ends
  bool host_bound = false, client_bound = false;
  int64_t last_us = 0;
};

struct Rendezvous {
  RendezvousConfig config;
  Transport* transport = nullptr;
  core::Rng rng;
  std::vector<RendezvousSession> sessions;
  std::vector<Relay> relays;  // max_relays, sized at init
  uint16_t next_port = 0;
  RendezvousStats stats;
  uint8_t buffer[kMaxDatagram];
};

void rendezvous_init(Rendezvous& rv, const RendezvousConfig& config, Transport& transport);
// Answer everything on the server's port, forward everything on the relays', drop the stale.
void rendezvous_update(Rendezvous& rv, int64_t now_us);
// Close every relay port (the server's own transport is the caller's).
void rendezvous_close(Rendezvous& rv);
int rendezvous_open_relays(const Rendezvous& rv);

}  // namespace ge::net

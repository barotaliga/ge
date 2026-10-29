// THE SIMULATED NETWORK: nodes with simulated addresses (`sim:<node>:<port>`) exchanging
// datagrams over directed links, each under its own `LinkConditions`, on a virtual clock the
// caller advances. Every fate is drawn off the network's own seeded stream in the order packets
// are sent, and deliveries come out in (arrival, send order), so a session over it, bots and
// fake traffic included, replays to the bit. It is how everything in engine/net is verified
// headless: the labs, the tests and `ge net sim` run whole sessions of up to 32 peers through
// it in a fraction of real time.
//
// A datagram to an address nobody has bound is dropped and counted, as a real network would. A
// datagram to `sim_broadcast(port)` is a LAN's broadcast: a copy to every socket bound on that
// port but the sender's on the sender's own LAN (behind the same NAT, its router's LAN face
// included; or the open network, for a node behind none), each over its own link.
//
// NATS. A node may sit behind a simulated NAT (`sim_add_nat`), as a player at home does behind
// their router: a datagram it sends past the NAT leaves from the NAT's public node on a port the
// NAT maps it to, and a datagram arriving at that port reaches it only if the NAT's kind lets it
// through (RFC 4787's mapping and filtering behaviours):
//
//   full_cone        one mapping per inside socket; anyone may send to it
//   restricted       one mapping; only from a node the inside socket has sent to
//   port_restricted  one mapping; only from an address (node and port) it has sent to
//   symmetric        a mapping per destination address; only that address may answer
//
// A mapping lives `mapping_us` after the last datagram out through it. A port forward
// (`sim_nat_forward`, what UPnP, NAT-PMP or PCP asks a router for: sim_router.hpp) lets anything
// in to one inside address, and everything that address sends out leaves from the forwarded port,
// whatever the kind, as a router's static mapping does. The router's own LAN face
// (`NatConfig::gateway`) is reached from inside without crossing the NAT. What a NAT refuses is
// counted in `nat_dropped`.
//
// ge:summary The simulated network: conditioned directed links on a virtual clock, seeded
// ge:keywords net network simulated sim-network virtual-network fake link deliver datagram
// ge:keywords virtual-clock deterministic headless partition broadcast nat full-cone
// ge:keywords restricted-cone port-restricted symmetric-nat mapping filtering port-forward
// ge:keywords gateway static-mapping
// ge:skill net
// ge:test tests/net_sim_network_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/prng.hpp"
#include "net/address.hpp"
#include "net/conditions.hpp"

namespace ge::net {

// The largest datagram any transport carries: an Ethernet frame's payload. The engine's own
// packets stay within kMaxPacket (packet.hpp).
constexpr size_t kMaxDatagram = 1472;

struct Datagram {
  Address from, to;
  int64_t at_us = 0;  // when it arrived
  uint16_t size = 0;
  uint8_t data[kMaxDatagram];
};

struct SimStats {
  uint64_t sent = 0, delivered = 0, lost = 0, queue_dropped = 0, duplicated = 0, unbound = 0;
  uint64_t bytes_sent = 0, bytes_delivered = 0;
  uint64_t nat_dropped = 0;  // arrived at a NAT's public node and refused (NATS)
};

enum class NatKind : uint8_t { full_cone, restricted, port_restricted, symmetric };
const char* nat_kind_name(NatKind k);
bool nat_kind(std::string_view name, NatKind& out);

// No node: a NAT without a LAN face of its own.
constexpr uint32_t kSimNoNode = 0xfffffffeu;

struct NatConfig {
  NatKind kind = NatKind::port_restricted;
  uint32_t public_node = 0;       // the address the outside sees
  std::vector<uint32_t> inside;   // the nodes behind it
  uint32_t gateway = kSimNoNode;  // the router's LAN face, reached from inside directly
  int64_t mapping_us = 30000000;  // a mapping's life after its last datagram out
  uint16_t first_port = 40000;    // the first public port it hands out
};

struct SimNat {
  NatConfig config;
  struct Mapping {
    Address inside;
    Address remote;  // symmetric: the one destination it was made for
    uint16_t port = 0;
    int64_t last_out_us = 0;
    std::vector<Address> sent_to;  // the filter's memory
  };
  std::vector<Mapping> maps;
  struct Forward {
    uint16_t port = 0;
    Address inside;
    int64_t until_us = 0;
  };
  std::vector<Forward> forwards;
  uint16_t next_port = 0;
};

struct SimNetwork {
  core::Rng rng{0};
  int64_t now_us = 0;
  LinkConditions defaults;
  struct Link {
    LinkConditions conditions;
    LinkState state;
  };
  // Keyed by from_node << 32 | to_node; looked up, never iterated.
  std::unordered_map<uint64_t, Link> links;
  struct Pending {
    int64_t at_us;
    uint64_t order;  // send order, the tie-break: the heap's key is unique
    uint32_t slot;   // into `store`
  };
  std::vector<Pending> heap;  // a min-heap on (at_us, order)
  std::vector<Datagram> store;
  std::vector<uint32_t> free_slots;
  uint64_t next_order = 0;
  struct Socket {
    Address address;
    std::vector<uint32_t> inbox;  // slots into `store`, in arrival order
    size_t head = 0;
  };
  std::vector<Socket> sockets;
  std::vector<SimNat> nats;
  SimStats stats;
};

// The simulated network's broadcast node: a datagram to it reaches every socket on its port.
constexpr uint32_t kSimBroadcast = 0xffffffffu;
inline Address sim_broadcast(uint16_t port) { return sim_address(kSimBroadcast, port); }

SimNetwork make_sim_network(uint32_t seed, const LinkConditions& defaults);

// Bind a socket at `address` (family sim); false when it is already bound.
bool sim_bind(SimNetwork& net, const Address& address);
void sim_unbind(SimNetwork& net, const Address& address);

// The conditions from one node to another (one direction), or both ways.
void sim_set_link(SimNetwork& net, uint32_t from_node, uint32_t to_node, const LinkConditions& c);
void sim_set_path(SimNetwork& net, uint32_t a, uint32_t b, const LinkConditions& c);
const LinkConditions& sim_link(const SimNetwork& net, uint32_t from_node, uint32_t to_node);

// Put `config.inside` behind a NAT; its index.
int sim_add_nat(SimNetwork& net, const NatConfig& config);
// Forward the NAT's `public_port` to `inside` until `until_us`; false when the port is mapped.
bool sim_nat_forward(SimNetwork& net, int nat, const Address& inside, uint16_t public_port,
                     int64_t until_us);
// Take a forward down; false when the port is not forwarded.
bool sim_nat_unforward(SimNetwork& net, int nat, uint16_t public_port);
// The NAT `node` sits behind, or -1.
int sim_nat_of(const SimNetwork& net, uint32_t node);

// Put a datagram on the wire at the network's clock; its fate is decided now. False when it is
// larger than kMaxDatagram (nothing is sent).
bool sim_send(SimNetwork& net, const Address& from, const Address& to, const uint8_t* data,
              size_t size);
// Move the clock to `now_us` (never back) and deliver every datagram due by then.
void sim_advance(SimNetwork& net, int64_t now_us);
// The next datagram delivered to `at`, oldest first; false when none is waiting.
bool sim_receive(SimNetwork& net, const Address& at, Datagram& out);
// The same into a caller's buffer: the size, or -1 when none is waiting; a datagram larger than
// `capacity` is dropped.
int sim_receive_bytes(SimNetwork& net, const Address& at, uint8_t* buffer, size_t capacity,
                      Address& from);

}  // namespace ge::net

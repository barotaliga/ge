// A NODE: one end of the game's network, a host, a client or both (a listen server; a peer of a
// mesh). It owns up to `max_peers` connections (the game's maximum, at most 32), does the
// handshake, keeps them alive, times them out, and turns what arrives into EVENTS the game
// reads once a frame:
//
//   connected(peer)   disconnected(peer, reason)   message(peer, channel, bytes)
//   connect_failed(peer, reason)
//
// THE HANDSHAKE: request (padded to kMaxPacket) → challenge (a cookie, the client's address and
// salt hashed with the host's secret; the host keeps nothing) → response (the cookie, padded) →
// accept (the session key, the slot) or deny (full, closed). A spoofed address never sees the
// challenge, so it cannot take a slot, and no reply is bigger than the packet that asked for it.
// Requests and responses are sent again every `retry_us` until `connect_timeout_us`.
//
// A frame's use:
//   node_update(node, now)     receive everything, time out the silent, fill the events
//   for (const Event& e : node.events) ...  (event_data(node, e) for a message's bytes)
//   node_send(node, peer, channel, bytes, n) ...
//   node_flush(node, now)      write and send each peer's packets (a keepalive when idle)
//
// SECURITY (M17i, `NodeConfig::security`; docs/networking.md § Security): a client connects with
// a CONNECT TOKEN (token.hpp) the matchmaker made, `node_connect_secure`; the host opens it with
// its own key, refuses one that does not open, has expired, names another host or was seen from
// another address, and binds the cookie to it. The token's two keys then seal every accept, deny,
// payload and disconnect (packet.hpp): a forged packet does not open, and a replayed one is
// refused by the peer's window of the last 256 counters. Without security the cookie and the
// session key are keyed hashes, not cryptography: enough on a LAN, and what the labs ran before.
//
// FLOOD GUARDS (`NodeConfig::guards`, off by default): the handshakes a host answers a second in
// all and from one address, and the payloads a connected peer may send a second. What exceeds
// them is dropped before any work is done on it (and counted in NodeStats).
//
// The clock is the caller's; the salts and the secret come off `seed` (a real node passes
// os_entropy()).
//
// ge:summary A node: host, client or peer; handshake, up to 32 peers, timeouts, events, flush
// ge:keywords net node host server client peer session connect accept deny handshake challenge
// ge:keywords cookie salt keepalive timeout disconnect event max-peers slot listen-server
// ge:keywords mesh p2p broadcast multiplayer security secure token replay replay-window
// ge:keywords flood-guard rate-limit throttle sealed aead
// ge:skill net
// ge:test tests/net_node_test.cpp tests/net_node_secure_test.cpp
#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "net/connection.hpp"
#include "net/token.hpp"
#include "net/transport.hpp"

namespace ge::net {

constexpr int kMaxPeers = 32;

// Connect tokens and sealed packets: off unless `on`. A host needs its id and its key (host_key of
// the matchmaker's master key, from the environment); a client needs only its tokens.
struct NodeSecurity {
  bool on = false;
  uint64_t host_id = 0;
  Key key{};
  int64_t clock_offset_us = 0;  // the node's clock plus this is the tokens' clock
};

// The host's flood guards; a rate of 0 turns its guard off. A bucket holds `burst` and fills at
// the rate.
struct NodeGuards {
  double handshakes_per_s = 0;  // requests and responses from strangers, all addresses together
  double handshake_burst = 0;
  double address_per_s = 0;  // the same from one address
  double address_burst = 0;
  double packets_per_s = 0;  // payloads from one connected peer
  double packet_burst = 0;
};
// What the labs and a game's dedicated server start from: a client sends at most a packet a tick
// (and resends inside it), so 400 a second is a 120 Hz client's three times over.
NodeGuards default_guards();

struct NodeConfig {
  uint32_t protocol = 0;      // protocol_id(game, version)
  int max_peers = kMaxPeers;  // 1..32: the slots, incoming and outgoing together
  bool accept = false;        // take requests from nodes this one did not connect to (a host)
  ChannelConfig channels;
  int64_t timeout_us = 5000000;          // a connected peer silent this long is dropped
  int64_t keepalive_us = 100000;         // an idle peer still gets a packet this often
  int64_t connect_timeout_us = 5000000;  // a connect not accepted by then fails
  int64_t retry_us = 100000;             // a request or a response sent again
  uint64_t seed = 0;                     // the salts' and the secret's stream
  // Keep the datagrams that are not this protocol's (a rendezvous's or a STUN server's answer on
  // the node's own socket, reach.hpp) in `foreign` for the caller instead of only counting them.
  bool keep_foreign = false;
  NodeSecurity security;
  NodeGuards guards;
};

enum class EventType : uint8_t { connected, disconnected, message, connect_failed };
enum class Reason : uint8_t {
  none,
  timed_out,          // the peer went silent
  remote_closed,      // the peer said disconnect
  denied_full,        // the host had no free slot
  denied_closed,      // the host takes no one now
  connect_timed_out,  // no accept before connect_timeout_us
};
const char* event_name(EventType t);
const char* reason_name(Reason r);

struct Event {
  EventType type;
  int peer;
  int channel;            // message
  Reason reason;          // disconnected, connect_failed
  uint32_t offset, size;  // message: its bytes in the node's event buffer
};

enum class PeerState : uint8_t { free, requesting, responding, connected };

// A rate's bucket: tokens it holds and when it was last filled.
struct Bucket {
  uint64_t who = 0;  // the address's hash, for an address's bucket
  double tokens = 0;
  int64_t at_us = 0;
};

struct Peer {
  PeerState state = PeerState::free;
  Address address;
  bool outgoing = false;  // this node connected to it
  uint64_t salt = 0;      // the connecting side's
  uint64_t cookie = 0;
  int64_t started_us = 0, last_recv_us = 0, last_send_us = 0, last_try_us = 0;
  Connection conn;
  // Security: the token's keys (the send key is to_host on the client and to_client on the host),
  // the client's id and user bytes, the sealed token a client sends again, the packet counters.
  bool secure = false;
  Key send_key{}, recv_key{};
  uint64_t client_id = 0;
  std::array<uint8_t, kTokenUserBytes> user{};
  std::array<uint8_t, kSealedTokenBytes> token{};
  uint64_t send_counter = 0;
  uint64_t recv_newest = 0;  // the newest counter opened; bit i of recv_seen: newest - i
  uint64_t recv_seen[4] = {};
  Bucket packets;
};

// What a node dropped before it reached a peer: the tools read it beside each peer's LinkStats.
struct NodeStats {
  uint64_t foreign = 0;      // another protocol, or not a packet
  uint64_t strangers = 0;    // from an address with no peer, not a request
  uint64_t bad_cookies = 0;  // a response whose cookie the host did not make
  uint64_t bad_keys = 0;     // a payload or disconnect with the wrong session key
  uint64_t denied = 0;       // requests turned away (full or closed)
  // Security and the flood guards.
  uint64_t bad_tokens = 0;       // a token that did not open, or was for another host
  uint64_t expired_tokens = 0;   // a token past its expiry
  uint64_t replayed_tokens = 0;  // a token (or its client) already seen from another address
  uint64_t forged = 0;           // a sealed packet that did not open
  uint64_t replayed = 0;         // a sealed packet whose counter was opened before, or too old
  uint64_t throttled = 0;        // a stranger's handshake over a guard's rate
  uint64_t flooded = 0;          // a peer's payload over its guard's rate
};

// A token the host has opened: its tag, the address it came from and its expiry.
struct SeenToken {
  std::array<uint8_t, kTagBytes> tag{};
  Address from;
  int64_t expire_us = 0;
  bool used = false;
};
constexpr int kSeenTokens = 256;
constexpr int kAddressBuckets = 256;

// A datagram on the node's socket that was not its protocol's (NodeConfig::keep_foreign).
struct Foreign {
  Address from;
  uint32_t offset, size;  // its bytes in `foreign_bytes`
};

struct Node {
  NodeConfig config;
  Transport* transport = nullptr;
  uint64_t secret = 0;
  core::Rng rng{0};
  bool closed = false;  // a host that takes no one more (denies `closed`)
  std::vector<Peer> peers;
  std::vector<Event> events;
  std::vector<uint8_t> event_bytes;
  std::vector<Foreign> foreign;  // this update's, when keep_foreign
  std::vector<uint8_t> foreign_bytes;
  NodeStats stats;
  std::vector<SeenToken> seen;     // security on a host: kSeenTokens, the oldest replaced
  std::vector<Bucket> addresses;   // the guards on: kAddressBuckets, by the address's hash
  Bucket handshakes;
  uint8_t buffer[kMaxDatagram];
};

// Size the node for `config` (every connection's buffers now) on an open transport.
void node_init(Node& node, const NodeConfig& config, Transport& transport);
// Start connecting to `to`: the peer's slot, or -1 when every slot is taken.
int node_connect(Node& node, const Address& to, int64_t now_us);
// The same with the matchmaker's token: every packet to and from the peer is sealed with its keys.
int node_connect_secure(Node& node, const Address& to, const ConnectToken& token, int64_t now_us);
// Point a peer still connecting at another address and start its handshake over there, its
// timeout with it: where a NAT's other port answered from, or a relay (reach.hpp). False when the
// peer is not an outgoing one still connecting.
bool node_retarget(Node& node, int peer, const Address& to, int64_t now_us);
void node_update(Node& node, int64_t now_us);
// Queue a message to a connected peer; false when it is not connected or the channel refused it
// (too big, a reliable window full: connection.hpp).
bool node_send(Node& node, int peer, int channel, const uint8_t* data, size_t size);
// To every connected peer but `except` (-1: none); the count queued.
int node_broadcast(Node& node, int channel, const uint8_t* data, size_t size, int except = -1);
void node_flush(Node& node, int64_t now_us);
// Tell the peer (three disconnect packets, for the loss) and free its slot; no event follows.
void node_disconnect(Node& node, int peer, int64_t now_us);

const uint8_t* event_data(const Node& node, const Event& e);
int node_connected_count(const Node& node);
inline bool peer_connected(const Node& node, int peer) {
  return peer >= 0 && peer < static_cast<int>(node.peers.size()) &&
         node.peers[peer].state == PeerState::connected;
}

}  // namespace ge::net

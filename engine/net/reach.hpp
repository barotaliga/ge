// REACHING A HOST behind a home router: the two ends of the rendezvous (rendezvous.hpp), driven
// on the node's own socket so the NAT mapping the server sees is the one the game's packets use.
//
// THE HOST registers its session with the server every `register_us` (keeping its mapping and
// the server's record alive) and learns its public address. When the server introduces a client,
// the host PUNCHES: a few small datagrams to the client's public address, which open the host's
// own NAT to that address, and it binds the pair's relay port in case the punch never gets
// through. Its node accepts the client's connect, direct or relayed, like any other.
//
// THE CLIENT joins by the session's id (asking again until the server introduces it, even past
// an `unknown`: the host's own register may be on its way), then connects its
// node straight to the host's public address. A punch carrying its nonce from another address
// (the host's NAT gave that pair its own port) moves the connect there. A connect not
// accepted within `direct_us` moves to the relay: the client binds it and connects through it.
// The outcome is `connected` with its path, direct or relay, or `failed` with why.
//
//   host:   reach_host_init(h, config, node)     every frame, after node_update: reach_host_update
//   client: reach_client_init(c, config, node); reach_client_start(c, now)
//           every frame, after node_update: reach_client_update(c, now)
//
// Both read the node's foreign datagrams, so its NodeConfig::keep_foreign must be set.
//
// ge:summary Reaching a host through NATs: register, join, punch, the relay fallback, the path
// ge:keywords net reach nat-traversal hole-punching punch rendezvous relay fallback direct path
// ge:keywords peer-reflexive connect join register public-address turn ice
// ge:skill net
// ge:test tests/net_reach_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "net/address.hpp"
#include "net/node.hpp"

namespace ge::net {

struct ReachConfig {
  Address rendezvous;               // the server
  uint64_t session = 0;             // the game's id for the host's session
  int64_t register_us = 10000000;   // the host registers again this often
  int64_t ask_us = 500000;          // a register or join not answered is sent again this often
  int64_t punch_every_us = 100000;  // the host's punches to a client this far apart
  int punches = 30;                 // and this many of them
  int64_t bind_every_us = 5000000;  // a relay bound again this often (its NAT mapping kept)
  int64_t direct_us = 3000000;      // a client's direct connect given this long
  int64_t give_up_us = 12000000;    // and the whole reach this long
  int64_t introduced_life_us = 60000000;  // a host forgets an introduction this long after
  uint64_t seed = 0;                      // the client's nonce
};

enum class ReachPath : uint8_t { none, direct, relay };
const char* reach_path_name(ReachPath p);

struct ReachStats {
  uint64_t asks = 0, punches = 0, binds = 0, retargets = 0;
};

// A client the server introduced to the host.
struct Introduced {
  uint64_t nonce = 0;
  Address client;  // its public address as the server saw it
  Address relay;   // none when the server had no relay port
  uint64_t key = 0;
  int punches_left = 0;
  int64_t next_punch_us = 0, last_bind_us = 0, at_us = 0;
};

struct ReachHost {
  ReachConfig config;
  Node* node = nullptr;
  bool registered = false;
  Address public_address;  // as the server saw it
  int64_t last_register_us = 0, last_ask_us = 0;
  bool asked = false;
  std::vector<Introduced> introduced;
  ReachStats stats;
};

enum class ReachState : uint8_t { idle, joining, direct, relaying, connected, failed };
const char* reach_state_name(ReachState s);
enum class ReachWhy : uint8_t { none, unknown_session, no_answer, no_relay, timed_out };
const char* reach_why_name(ReachWhy w);

struct ReachClient {
  ReachConfig config;
  Node* node = nullptr;
  ReachState state = ReachState::idle;
  ReachWhy why = ReachWhy::none;
  ReachPath path = ReachPath::none;
  uint64_t nonce = 0;
  bool unknown = false;  // the server last said no such session
  int peer = -1;         // the node's slot for the host
  Address host_public;   // as the server saw it
  Address host_address;  // where the connect is aimed now
  Address relay;
  uint64_t key = 0;
  int64_t started_us = 0, last_ask_us = 0, direct_since_us = 0, last_bind_us = 0;
  int64_t connected_us = 0;  // how long the reach took, once connected
  ReachStats stats;
};

void reach_host_init(ReachHost& h, const ReachConfig& config, Node& node);
void reach_host_update(ReachHost& h, int64_t now_us);

void reach_client_init(ReachClient& c, const ReachConfig& config, Node& node);
// Ask the server for the session's host; the reach starts now.
void reach_client_start(ReachClient& c, int64_t now_us);
void reach_client_update(ReachClient& c, int64_t now_us);

}  // namespace ge::net

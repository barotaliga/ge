// THE HUB'S POLLER: what a community's hub knows of a game's live nodes (docs/hub.md § How the
// hub learns the state). It reads every configured master's list (net/master.hpp) on a period and
// asks each listed node for its status record (net/status.hpp), cookie round first, so the index
// holds one HubNode a node: where it is, its kind, the last record it answered and how often the
// record changed. A node the masters stop listing stays until its status has also gone quiet,
// so a master's restart does not empty the hub. One socket for everything; the caller passes the
// time, so the same poller runs on the simulated network in labs and on a UDP socket in the hub's
// process.
//
// The periods follow the design: the list every `list_us`, each node's status every `status_us`,
// or every `busy_us` while a human plays there (the pages want those fresh). A node whose last
// answer is older than `drop_us` and that no master lists is dropped.
//
// ge:summary The hub's poller: the masters' lists and every node's status record, as an index
// ge:keywords hub poller index master list status record cookie busy refresh drop node
// ge:keywords server-list live
// ge:skill net
// ge:test tests/hub_poller_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "net/address.hpp"
#include "net/master.hpp"
#include "net/status.hpp"
#include "net/transport.hpp"

namespace ge::hub {

struct PollerConfig {
  std::vector<net::Address> masters;
  uint32_t protocol = 0;  // the game's (net/packet.hpp's protocol_id)
  uint8_t kinds = net::kAllKinds;
  int64_t list_us = 60000000;    // every master's list this often
  int64_t status_us = 30000000;  // each node's status this often
  int64_t busy_us = 4000000;     // ... and this often while a human plays there
  int64_t retry_us = 1000000;    // an ask not answered is sent again this often
  int tries = 3;                 // ... this many times, then it waits for its next turn
  int64_t drop_us = 90000000;    // unlisted and silent this long: dropped
  int max_nodes = 4096;
  uint64_t seed = 0;
};

struct HubNode {
  net::Address address;
  net::NodeKind kind = net::NodeKind::game;
  uint32_t version = 0;
  bool listed = false;    // on the last whole list
  bool have = false;      // a record has come
  net::Status status;     // the last record
  int64_t first_us = 0;   // when the hub first heard of it
  int64_t heard_us = -1;  // the last record's arrival (-1: none yet)
  int64_t due_us = 0;     // the next ask
  int64_t asked_us = -1;  // the last ask (-1: none outstanding)
  int tries = 0;          // asks sent for the outstanding one
  uint64_t nonce = 0, cookie = 0;
  uint64_t record_hash = 0;  // the last record's bytes, hashed: a change bumps `changes`
  uint64_t changes = 0;      // records that differed from the one before
  int64_t phase_us = -1;     // when the record's phase last changed
  bool browsed = false;      // read by the game's own protocol (browse.hpp), never asked here
};

struct PollerStats {
  uint64_t lists = 0, list_entries = 0;
  uint64_t asks = 0, cookies = 0, records = 0, changes = 0;
  uint64_t added = 0, dropped = 0;
  uint64_t stray = 0;  // datagrams that were nobody's answer
};

struct Poller {
  PollerConfig config;
  net::MasterQuery query;
  bool querying = false;
  int64_t next_list_us = 0;
  int64_t listed_us = -1;  // the last whole list's arrival
  std::vector<HubNode> nodes;
  PollerStats stats;
  uint64_t serial = 0;  // bumps on every change the pages show: a node added, dropped or changed
  uint64_t rng = 0;
};

void poller_init(Poller& p, const PollerConfig& config, int64_t now_us);
// The asks due now: the list when its period is up, each node's status when its turn is.
void poller_update(Poller& p, net::Transport& t, int64_t now_us);
// One datagram that came to the poller's socket; false when it was nobody's answer.
bool poller_read(Poller& p, net::Transport& t, const uint8_t* data, size_t size,
                 const net::Address& from, int64_t now_us);
// Everything waiting on the socket read, then the asks due: one call a frame.
void poller_pump(Poller& p, net::Transport& t, int64_t now_us);

// The status key a TV node names the game server it broadcasts under (tv/net_link.hpp writes it).
inline constexpr std::string_view kSourceKey = "source";
// A stream the hub added to a game server's record because a TV node's record names that server
// as its source (`kSourceKey`): the server says nothing of the broadcast itself (a classic
// server, or ours with a TV node watching it). Never on the wire; put back after every update.
inline constexpr uint8_t kStreamNamedByNode = 0x80;
// Every TV node's broadcast laid on the record of the server it names (`kStreamNamedByNode`),
// those of the last call taken off first; `poller_update` calls it. A source on loopback or
// "any" is the TV node's own host.
void poller_attach_broadcasts(Poller& p);

// The node at `address`, or null.
const HubNode* poller_find(const Poller& p, const net::Address& address);
// Nodes of these kinds that have answered.
int poller_count(const Poller& p, uint8_t kinds);
// The forwarder a browser page joins the game server at `server` through: one that answered,
// takes pages (its record names a WebTransport port and a certificate, or a WebSocket port) and
// reaches `server`; the one with the fewest clients, or null when none does (docs/hub.md §
// Playing in a browser).
const HubNode* poller_page_forward(const Poller& p, const net::Address& server);
// Humans on a node's record: players who are neither bots, spectators nor viewers.
// A record's bytes hashed (FNV-1a over status_write): equal records hash equal.
uint64_t record_hash(const net::Status& s);
int humans(const net::Status& s);

}  // namespace ge::hub

// THE MASTER: the public list of every node playing or broadcasting a game, which a hub, a server
// browser and a player's CLI read (docs/hub.md § The parts). One master serves every kind of
// node: game servers, broadcast (TV) nodes and forwarders BEAT to it, told apart by their kind,
// and anyone asks for the list of one game's nodes, a page at a time. A node that misses its beats
// for `expire_us` is dropped; one that stops says BYE and is dropped at once. The master keeps
// only who and where: what each node is playing is asked of the node itself (status.hpp).
//
// The datagrams, recognised by their magic:
//   beat    "geMS" 1, protocol u32, kind u8, version u32, port u16, nonce u64, padding to 32
//   ack     "geMS" 2, nonce u64
//   bye     "geMS" 3, protocol u32, kind u8, version u32, port u16, nonce u64, padding to 32
//   ask     "geMS" 4, protocol u32, kinds u8 (a bit per NodeKind), page u16, nonce u64,
//           padding to kMasterAskSize
//   page    "geMS" 5, nonce u64, total u32, page u16, count u8, then `count` entries of an
//           address (19 bytes), a kind u8 and a version u32
// A node is listed at the address its beat came from, with the beat's port when it names one (a
// node that beats from another socket than the one it serves on). No answer outgrows its ask, so
// a master cannot be used to amplify traffic at a third party. A host (one IP) holds at most
// `max_per_host` entries, so one machine cannot fill the list.
//
// The list is in the order the nodes first beat, so its pages stay stable while a hub reads them.
//
//   master_init(m, config, transport)       every frame: master_update(m, now)
//
// A node's half (MasterBeat) runs on the node's own socket, beside its game:
//   master_beat_init(b, config)             every frame: master_beat_update(b, transport, now)
//   each datagram on that socket: master_beat_read(b, data, size, from)   (true: it was ours)
//   stopping: master_beat_stop(b, transport, now)
// A reader's half (MasterQuery) asks each master for every page, again until it answers:
//   master_query_start(q, config, now)      every frame: master_query_update(q, transport, now)
//   each datagram: master_query_read(q, data, size, from); `done` when every master answered
//
// The clock is the caller's: a master and its nodes on the simulated network replay to the bit
// (the `hub-master` lab).
//
// ge:summary The master: game servers, TV nodes and forwarders beat to it; the list by game, paged
// ge:keywords net master master-server heartbeat beat list server-list hub expire bye tv
// ge:keywords forward node-kind directory public-list
// ge:skill net
// ge:test tests/net_master_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/address.hpp"
#include "net/status.hpp"
#include "net/transport.hpp"

namespace ge::net {

constexpr uint16_t kMasterPort = 27900;  // a master's port when an address names none
constexpr size_t kMasterBeatSize = 32;
constexpr size_t kMasterAskSize = 512;
constexpr size_t kMasterEntrySize = 19 + 1 + 4;
constexpr size_t kMasterPageHead = 4 + 1 + 8 + 4 + 2 + 1;
constexpr int kMasterPage = static_cast<int>((kMasterAskSize - kMasterPageHead) / kMasterEntrySize);

// A bit per NodeKind for an ask's `kinds`.
constexpr uint8_t kind_bit(NodeKind k) { return static_cast<uint8_t>(1u << static_cast<int>(k)); }
constexpr uint8_t kAllKinds = 7;

struct MasterEntry {
  Address address;  // where the node serves (its status is asked there)
  uint32_t protocol = 0;
  NodeKind kind = NodeKind::game;
  uint32_t version = 0;
  int64_t first_us = 0, last_us = 0;
};

struct MasterConfig {
  int max_entries = 4096;
  int max_per_host = 64;          // entries one IP may hold
  int64_t expire_us = 100000000;  // a node silent this long is dropped (three missed beats)
};

struct MasterStats {
  uint64_t beats = 0, added = 0, byes = 0, expired = 0;
  uint64_t asks = 0, pages = 0;
  uint64_t full = 0;     // beats refused: the list or the host's share is full
  uint64_t refused = 0;  // anything else: short, unknown, or not padded
};

struct Master {
  MasterConfig config;
  Transport* transport = nullptr;
  std::vector<MasterEntry> entries;
  MasterStats stats;
  uint8_t buffer[kMaxDatagram];
};

void master_init(Master& m, const MasterConfig& config, Transport& transport);
// Answer everything waiting on the master's port, then drop the silent.
void master_update(Master& m, int64_t now_us);
// The entries of one game whose kind is in `kinds`, in list order.
int master_count(const Master& m, uint32_t protocol, uint8_t kinds);

// A NODE'S BEAT.
struct MasterBeatConfig {
  std::vector<Address> masters;
  uint32_t protocol = 0;
  NodeKind kind = NodeKind::game;
  uint32_t version = 0;
  uint16_t port = 0;           // where the node serves (0: the socket it beats from)
  int64_t beat_us = 30000000;  // a beat every this often
  int64_t ask_us = 2000000;    // a beat not acknowledged is sent again this often
  int bye_tries = 3;           // a stop's bye sent this many times
  uint64_t seed = 0;
};

struct MasterBeatState {
  int64_t next_us = 0;  // the next beat (0: at once)
  int64_t acked_us = -1;
  uint64_t nonce = 0;
};

struct MasterBeat {
  MasterBeatConfig config;
  std::vector<MasterBeatState> masters;
  uint64_t beats = 0, acks = 0;
  bool stopped = false;
};

void master_beat_init(MasterBeat& b, const MasterBeatConfig& config);
void master_beat_update(MasterBeat& b, Transport& t, int64_t now_us);
// An ack from one of the masters: true when the datagram was one (the caller drops it).
bool master_beat_read(MasterBeat& b, const uint8_t* data, size_t size, const Address& from,
                      int64_t now_us);
// Say bye to every master (sent `bye_tries` times at once: no answer is awaited).
void master_beat_stop(MasterBeat& b, Transport& t, int64_t now_us);
// How many masters acknowledged a beat within the last `within_us`.
int master_beat_listed(const MasterBeat& b, int64_t now_us, int64_t within_us);

// A READER'S QUERY.
struct MasterQueryConfig {
  std::vector<Address> masters;
  uint32_t protocol = 0;
  uint8_t kinds = kAllKinds;
  int64_t ask_us = 500000;  // a page not answered is asked again this often
  int tries = 4;            // a page asked this many times at most
  uint64_t seed = 0;
};

struct MasterQueryState {
  uint64_t nonce = 0;
  int total = -1;                 // entries the master holds (-1: not heard yet)
  std::vector<int64_t> asked_us;  // per page: when last asked (-1: answered)
  std::vector<int> tries;
  bool done = false;
};

struct MasterQuery {
  MasterQueryConfig config;
  std::vector<MasterQueryState> masters;
  std::vector<MasterEntry> found;  // every node heard of, once (first_us: when heard)
  bool done = false;
};

void master_query_start(MasterQuery& q, const MasterQueryConfig& config, int64_t now_us);
void master_query_update(MasterQuery& q, Transport& t, int64_t now_us);
bool master_query_read(MasterQuery& q, const uint8_t* data, size_t size, const Address& from,
                       int64_t now_us);

}  // namespace ge::net

// engine/hub/poller.hpp: the hub reads the master's list and every listed node's status through
// its cookie round, sees a record change, polls a node with humans on it more often, and drops a
// node the master no longer lists once it has been silent long enough.
//
// ge:summary Test: the hub's poller: list, statuses, changes, busy nodes, drops
// ge:keywords test hub poller master list status cookie change busy drop
// ge:skill net testing
#include "hub/poller.hpp"

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/packet.hpp"

using namespace ge::net;
using namespace ge::hub;

namespace {

const uint32_t kGame = protocol_id("hub_test", 1);
const Address kMasterAt = sim_address(1, 27900);

struct Node {
  Transport port;
  MasterBeat beat;
  Status status;
  bool answering = true;
};

struct World {
  SimNetwork net = make_sim_network(5, LinkConditions{});
  Transport master_port, hub_port;
  Master master;
  Poller poller;
  std::vector<std::unique_ptr<Node>> nodes;
  int64_t now = 0;

  explicit World(const PollerConfig& extra = {}) {
    open_sim_transport(master_port, net, kMasterAt);
    master_init(master, MasterConfig{}, master_port);
    open_sim_transport(hub_port, net, sim_address(2, 40000));
    PollerConfig c = extra;
    c.masters = {kMasterAt};
    c.protocol = kGame;
    poller_init(poller, c, now);
  }

  Node& add(uint32_t at, NodeKind kind, int humans_on, int bots_on) {
    nodes.push_back(std::make_unique<Node>());
    Node& n = *nodes.back();
    open_sim_transport(n.port, net, sim_address(at, 27500));
    MasterBeatConfig c;
    c.masters = {kMasterAt};
    c.protocol = kGame;
    c.kind = kind;
    c.seed = at;
    master_beat_init(n.beat, c);
    n.status.kind = kind;
    n.status.protocol = kGame;
    n.status.name = "node " + std::to_string(at);
    n.status.map = "start";
    n.status.max_players = 8;
    for (int i = 0; i < humans_on + bots_on; i++) {
      StatusPlayer p;
      p.name = "p" + std::to_string(i);
      p.flags = i < humans_on ? 0 : kPlayerBot;
      n.status.players.push_back(p);
    }
    return n;
  }

  void tick(int64_t dt = 50000) {
    for (auto& n : nodes) {
      master_beat_update(n->beat, n->port, now);
      uint8_t buf[kMaxDatagram], out[kMaxDatagram];
      Address from;
      int k = 0;
      while ((k = transport_receive(n->port, buf, sizeof buf, from, now)) >= 0) {
        if (master_beat_read(n->beat, buf, static_cast<size_t>(k), from, now)) continue;
        if (!n->answering) continue;
        const size_t a = status_answer(buf, static_cast<size_t>(k), from, kGame, 77, now, n->status,
                                       out, sizeof out);
        if (a) transport_send(n->port, from, out, a, now);
      }
    }
    master_update(master, now);
    poller_pump(poller, hub_port, now);
    now += dt;
    sim_advance(net, now);
  }

  void run_s(double s) {
    const int64_t until = now + static_cast<int64_t>(s * 1e6);
    while (now < until) tick();
  }
};

PollerConfig quick() {
  PollerConfig c;
  c.list_us = 1000000;
  c.status_us = 5000000;
  c.busy_us = 1000000;
  c.drop_us = 10000000;
  return c;
}

}  // namespace

TEST(poller_reads_the_list_and_every_status) {
  World w(quick());
  for (uint32_t i = 0; i < 3; i++) w.add(10 + i, NodeKind::game, 0, 2);
  w.add(20, NodeKind::tv, 0, 0);
  w.run_s(4);
  CHECK(w.poller.nodes.size() == 4);
  CHECK(poller_count(w.poller, kind_bit(NodeKind::game)) == 3);
  CHECK(poller_count(w.poller, kind_bit(NodeKind::tv)) == 1);
  CHECK(w.poller.stats.cookies >= 4 && w.poller.stats.records >= 4);
  const HubNode* n = poller_find(w.poller, sim_address(11, 27500));
  CHECK(n && n->have && n->status.name == "node 11" && n->listed);
  CHECK(n && n->status.players.size() == 2);
}

TEST(poller_sees_a_record_change) {
  World w(quick());
  Node& a = w.add(10, NodeKind::game, 0, 1);
  w.run_s(3);
  const uint64_t serial = w.poller.serial;
  const HubNode* n = poller_find(w.poller, sim_address(10, 27500));
  CHECK(n && n->changes == 1);
  a.status.map = "tower";
  w.run_s(6);
  n = poller_find(w.poller, sim_address(10, 27500));
  CHECK(n && n->status.map == "tower" && n->changes == 2);
  CHECK(w.poller.serial > serial);
}

TEST(poller_asks_a_busy_node_more_often) {
  World w(quick());
  w.add(10, NodeKind::game, 2, 0);
  w.add(11, NodeKind::game, 0, 2);
  w.run_s(20);
  // A record per busy_us (1 s) on the first, per status_us (5 s) on the second; one ask each
  // after the first goes with the cookie it already holds.
  const HubNode* busy = poller_find(w.poller, sim_address(10, 27500));
  const HubNode* idle = poller_find(w.poller, sim_address(11, 27500));
  CHECK(busy && idle && busy->heard_us > 0 && idle->heard_us > 0);
  CHECK(humans(busy->status) == 2 && humans(idle->status) == 0);
  CHECK(w.poller.stats.records >= 18 + 4);
}

TEST(poller_drops_a_node_gone_from_the_list) {
  World w(quick());
  w.add(10, NodeKind::game, 0, 1);
  Node& b = w.add(11, NodeKind::game, 0, 1);
  w.run_s(3);
  CHECK(w.poller.nodes.size() == 2);
  master_beat_stop(b.beat, b.port, w.now);
  b.answering = false;
  w.run_s(3);
  const HubNode* n = poller_find(w.poller, sim_address(11, 27500));
  CHECK(n && !n->listed);  // off the list at once, kept while its record is fresh
  w.run_s(12);
  CHECK(poller_find(w.poller, sim_address(11, 27500)) == nullptr);
  CHECK(w.poller.stats.dropped == 1 && w.poller.nodes.size() == 1);
}

TEST(the_engine_master_takes_a_browsed_node_over) {
  // A node the browse found first (browse.hpp) that the engine's master then lists: from that list
  // on it is the poller's, asked in the engine's protocol, and its own record replaces the
  // browse's.
  World w(quick());
  w.add(30, NodeKind::game, 1, 0);
  HubNode b;
  b.address = sim_address(30, 27500);
  b.browsed = true;
  b.have = true;
  b.status.name = "as browsed";
  b.heard_us = 0;
  w.poller.nodes.push_back(b);
  w.run_s(4);
  CHECK(w.poller.nodes.size() == 1);
  const HubNode* n = poller_find(w.poller, sim_address(30, 27500));
  CHECK(n && !n->browsed && n->listed && n->status.name == "node 30");
}

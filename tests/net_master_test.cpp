// engine/net/master.hpp: nodes of every kind beat and are listed by game and kind, a reader gets
// every page of a long list, a node that says bye goes at once and a silent one after the expiry,
// one host cannot hold more than its share, and no answer outweighs its ask.
//
// ge:summary Test: the master's list: beats, kinds, pages, bye, expiry, per-host share
// ge:keywords test net master beat list page bye expire per-host amplification
// ge:skill net testing
#include "net/master.hpp"

#include <memory>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

constexpr uint32_t kGame = 0x51, kOther = 0x52;
const Address kMasterAt = sim_address(1, 27000);

struct World {
  SimNetwork net = make_sim_network(3, LinkConditions{});
  Transport master_port;
  Master master;
  std::vector<std::unique_ptr<Transport>> ports;
  std::vector<MasterBeat> beats;
  int64_t now = 0;

  World() {
    open_sim_transport(master_port, net, kMasterAt);
    master_init(master, MasterConfig{}, master_port);
  }

  void add(uint32_t node, uint16_t port, uint32_t protocol, NodeKind kind) {
    ports.push_back(std::make_unique<Transport>());
    open_sim_transport(*ports.back(), net, sim_address(node, port));
    MasterBeatConfig c;
    c.masters = {kMasterAt};
    c.protocol = protocol;
    c.kind = kind;
    c.version = 3;
    c.seed = node * 100 + port;
    beats.emplace_back();
    master_beat_init(beats.back(), c);
  }

  // Everyone a tick: beats out, the master, acks in.
  void tick(int64_t dt = 50000) {
    for (size_t i = 0; i < beats.size(); i++) master_beat_update(beats[i], *ports[i], now);
    now += dt;
    sim_advance(net, now);
    master_update(master, now);
    sim_advance(net, now);
    for (size_t i = 0; i < beats.size(); i++) {
      uint8_t buf[kMaxDatagram];
      Address from;
      int n = 0;
      while ((n = transport_receive(*ports[i], buf, sizeof buf, from, now)) >= 0) {
        master_beat_read(beats[i], buf, static_cast<size_t>(n), from, now);
      }
    }
  }

  std::vector<MasterEntry> query(uint32_t protocol, uint8_t kinds) {
    Transport reader;
    open_sim_transport(reader, net, sim_address(2, 9000 + static_cast<uint16_t>(now / 1000 % 500)));
    MasterQuery q;
    MasterQueryConfig c;
    c.masters = {kMasterAt};
    c.protocol = protocol;
    c.kinds = kinds;
    master_query_start(q, c, now);
    for (int i = 0; i < 100 && !q.done; i++) {
      master_query_update(q, reader, now);
      now += 20000;
      sim_advance(net, now);
      master_update(master, now);
      sim_advance(net, now);
      uint8_t buf[kMaxDatagram];
      Address from;
      int n = 0;
      while ((n = transport_receive(reader, buf, sizeof buf, from, now)) >= 0) {
        master_query_read(q, buf, static_cast<size_t>(n), from, now);
      }
    }
    close_transport(reader);
    return q.found;
  }
};

}  // namespace

TEST(master_lists_every_kind_by_game) {
  World w;
  for (uint32_t i = 0; i < 5; i++) w.add(10 + i, 27500, kGame, NodeKind::game);
  w.add(20, 28000, kGame, NodeKind::tv);
  w.add(21, 30000, kGame, NodeKind::forward);
  w.add(22, 27500, kOther, NodeKind::game);
  for (int i = 0; i < 4; i++) w.tick();
  CHECK(master_count(w.master, kGame, kAllKinds) == 7);
  CHECK(master_count(w.master, kGame, kind_bit(NodeKind::tv)) == 1);
  CHECK(master_count(w.master, kOther, kAllKinds) == 1);
  CHECK(master_beat_listed(w.beats[0], w.now, 1000000) == 1);
  const auto games = w.query(kGame, kind_bit(NodeKind::game));
  CHECK(games.size() == 5);
  CHECK(games[0].address == sim_address(10, 27500) && games[0].version == 3);
  const auto all = w.query(kGame, kAllKinds);
  CHECK(all.size() == 7);
}

TEST(master_pages_a_long_list) {
  World w;
  for (uint32_t i = 0; i < 50; i++) w.add(100 + i, 27500, kGame, NodeKind::game);
  for (int i = 0; i < 4; i++) w.tick();
  CHECK(master_count(w.master, kGame, kAllKinds) == 50);
  const auto all = w.query(kGame, kAllKinds);
  CHECK(all.size() == 50);
  CHECK(w.master.stats.pages >= 3);  // kMasterPage (20) a page
}

TEST(master_drops_on_bye_and_after_silence) {
  World w;
  for (uint32_t i = 0; i < 3; i++) w.add(10 + i, 27500, kGame, NodeKind::game);
  for (int i = 0; i < 4; i++) w.tick();
  CHECK(master_count(w.master, kGame, kAllKinds) == 3);
  master_beat_stop(w.beats[0], *w.ports[0], w.now);
  w.tick();
  CHECK(master_count(w.master, kGame, kAllKinds) == 2);
  CHECK(w.master.stats.byes == 1);
  // The second node falls silent (its beat stops without a bye); the third keeps beating.
  w.beats[1].stopped = true;
  for (int i = 0; i < 2200; i++) w.tick();  // 110 s
  CHECK(master_count(w.master, kGame, kAllKinds) == 1);
  CHECK(w.master.stats.expired == 1);
}

TEST(master_holds_one_host_to_its_share) {
  World w;
  MasterConfig c;
  c.max_per_host = 4;
  master_init(w.master, c, w.master_port);
  for (uint16_t p = 0; p < 6; p++)
    w.add(30, static_cast<uint16_t>(27500 + p), kGame, NodeKind::game);
  for (int i = 0; i < 4; i++) w.tick();
  CHECK(master_count(w.master, kGame, kAllKinds) == 4);
  CHECK(w.master.stats.full > 0);
}

TEST(master_never_outweighs_an_ask) {
  World w;
  for (uint32_t i = 0; i < 40; i++) w.add(100 + i, 27500, kGame, NodeKind::game);
  for (int i = 0; i < 4; i++) w.tick();
  // An unpadded ask is refused; a padded one gets a page no bigger than itself.
  Transport probe;
  open_sim_transport(probe, w.net, sim_address(3, 9999));
  uint8_t ask[kMasterAskSize] = {'g', 'e', 'M', 'S', 4};
  ask[5] = kGame;
  ask[9] = kAllKinds;
  transport_send(probe, kMasterAt, ask, 40, w.now);
  transport_send(probe, kMasterAt, ask, sizeof ask, w.now);
  w.tick();
  uint8_t buf[kMaxDatagram];
  Address from;
  int n = 0, answers = 0;
  while ((n = transport_receive(probe, buf, sizeof buf, from, w.now)) >= 0) {
    CHECK(static_cast<size_t>(n) <= kMasterAskSize);
    answers++;
  }
  CHECK(answers == 1);
  CHECK(w.master.stats.refused >= 1);
}

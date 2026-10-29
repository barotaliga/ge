// engine/net/directory.hpp and the rendezvous's list over the simulated network: nine listed
// games read over two pages, each pinged; the view's filters and sorts; an invite-only game never
// listed; an advertise from anyone but the session's host refused; an unpadded list ask refused;
// the route to a LAN game and to a listed one.
//
// ge:summary Test: the server directory: the list's pages, pings, filters, sorts, refusals, routes
// ge:keywords test net directory server-browser listing page ping filter sort advertise refuse
// ge:skill net testing
#include "net/directory.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/hosting.hpp"
#include "net/reach_wire.hpp"
#include "net/rendezvous.hpp"

using namespace ge::net;

namespace {

const Address kServer = sim_address(100, 3478);
const uint32_t kGame = protocol_id("dir-test", 1);

struct Host {
  Transport t;
  Node n;
  SessionHost s;
  Hosting h;
};

std::unique_ptr<Host> host(SimNetwork& net, uint32_t node, const char* name, Visibility vis,
                           int max_players, bool locked) {
  auto e = std::make_unique<Host>();
  open_sim_transport(e->t, net, sim_address(node, 7000));
  NodeConfig nc;
  nc.protocol = kGame;
  nc.accept = true;
  nc.channels.delivery[0] = Delivery::reliable;
  nc.seed = node;
  node_init(e->n, nc, e->t);
  SessionConfig sc;
  sc.max_players = max_players;
  sc.host_plays = true;
  session_host_init(e->s, sc, e->n, 0);
  HostingConfig hc;
  hc.protocol = kGame;
  hc.name = name;
  hc.visibility = vis;
  hc.locked = locked;
  hc.rendezvous = kServer;
  hc.seed = node;
  hosting_start(e->h, hc, e->n, e->s, nullptr, nullptr, 0);
  return e;
}

struct World {
  SimNetwork net = make_sim_network(3, LinkConditions{});
  Transport st, bt;
  Rendezvous rv;
  std::vector<std::unique_ptr<Host>> hosts;
  Directory d;

  World() {
    open_sim_transport(st, net, kServer);
    rendezvous_init(rv, {}, st);
    // Nodes 10..18 a hop of 20 ms more each, so the pings sort them.
    for (uint32_t i = 0; i < 9; i++) {
      char name[16];
      std::snprintf(name, sizeof name, "%c game", static_cast<char>('i' - i));
      LinkConditions lc;
      lc.latency_ms = 20.0 * (i + 1);
      sim_set_path(net, 1, 10 + i, lc);
      hosts.push_back(host(net, 10 + i, name, Visibility::listed, i < 3 ? 1 : 8, i == 4));
    }
    hosts.push_back(host(net, 30, "secret", Visibility::invite, 8, false));
    open_sim_transport(bt, net, sim_address(1, 27600));
    DirectoryConfig dc;
    dc.protocol = kGame;
    dc.rendezvous = kServer;
    dc.seed = 5;
    directory_init(d, dc, bt);
  }

  void run(int64_t from, int64_t to) {
    for (int64_t now = from; now <= to; now += 10000) {
      sim_advance(net, now);
      rendezvous_update(rv, now);
      for (auto& h : hosts) {
        node_update(h->n, now);
        session_host_update(h->s, now);
        hosting_update(h->h, now);
        node_flush(h->n, now);
      }
      directory_update(d, now);
    }
  }
};

std::vector<std::string> names(const Directory& d, const std::vector<int>& rows) {
  std::vector<std::string> out;
  out.reserve(rows.size());
  for (const int i : rows) out.emplace_back(d.entries[static_cast<size_t>(i)].advert.name);
  return out;
}

}  // namespace

TEST(directory_reads_every_page_and_pings_each_game) {
  World w;
  w.run(10000, 2000000);
  directory_refresh(w.d, 2000000);
  w.run(2010000, 5000000);
  CHECK(directory_done(w.d, 5000000));
  CHECK(w.d.pages == 2);
  CHECK(w.d.entries.size() == 9);  // "secret" is invite-only
  CHECK(w.d.stats.pages == 2);
  for (const DirectoryEntry& e : w.d.entries) {
    CHECK(e.from == kFoundListed);
    CHECK(e.ping_ms > 0 && e.pings_answered == 3);
  }
  std::vector<int> rows;
  directory_view(w.d, {}, DirectorySort::ping, rows);
  const auto by_ping = names(w.d, rows);
  CHECK(by_ping.front() == "i game" && by_ping.back() == "a game");  // the nearest is node 10
  directory_view(w.d, {}, DirectorySort::name, rows);
  CHECK(names(w.d, rows).front() == "a game");
  DirectoryFilter f;
  f.hide_full = true;  // the first three hold one player, the host
  f.hide_locked = true;
  directory_view(w.d, f, DirectorySort::name, rows);
  CHECK(rows.size() == 5);
  f = DirectoryFilter{};
  f.max_ping_ms = 100;  // a round trip is twice the hop, and a tick or two
  directory_view(w.d, f, DirectorySort::ping, rows);
  CHECK(rows.size() == 2);
  f = DirectoryFilter{};
  f.name_has = "C GA";
  directory_view(w.d, f, DirectorySort::ping, rows);
  CHECK(rows.size() == 1 && names(w.d, rows)[0] == "c game");
  const JoinRoute r = directory_route(w.d.entries[0]);
  CHECK(!r.lan && r.session == w.d.entries[0].advert.session && r.session != 0);
}

TEST(directory_rendezvous_refuses_a_strangers_advertise_and_a_short_ask) {
  World w;
  w.run(10000, 1000000);
  const uint64_t refused = w.rv.stats.refused;
  // A stranger advertises the first game unlisted.
  Transport x;
  open_sim_transport(x, w.net, sim_address(66, 7000));
  uint8_t out[reach_wire::kListAsk];
  Writer a = make_writer(out, sizeof out);
  reach_wire::write_start(a, reach_wire::kAdvertise);
  write_u64(a, hosting_invite(w.hosts[0]->h));
  write_u32(a, 1);
  write_u32(a, kGame);
  write_u8(a, 0);
  write_advert_body(a, Advert{});
  write_pad_to(a, reach_wire::kMaxSize);
  transport_send(x, kServer, out, a.size, 1000000);
  // A list ask not padded to the biggest page.
  Writer l = make_writer(out, sizeof out);
  reach_wire::write_start(l, reach_wire::kList);
  write_u32(l, kGame);
  write_u16(l, 0);
  write_pad_to(l, reach_wire::kMaxSize);
  transport_send(x, kServer, out, l.size, 1000000);
  w.run(1010000, 1100000);
  CHECK(w.rv.stats.refused == refused + 2);
  int listed = 0;
  for (const RendezvousSession& s : w.rv.sessions) listed += s.listed;
  CHECK(listed == 9);
  uint8_t in[kMaxDatagram];
  Address from;
  CHECK(transport_receive(x, in, sizeof in, from, 1100000) < 0);  // nothing answered
}

TEST(directory_route_to_a_lan_game_is_its_address) {
  DirectoryEntry e;
  e.from = kFoundLan | kFoundListed;
  e.lan = sim_address(3, 7000);
  e.advert.session = 9;
  const JoinRoute r = directory_route(e);
  CHECK(r.lan && r.address == sim_address(3, 7000));
}

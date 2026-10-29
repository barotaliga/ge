// The hosting trial (net/hosting_trial.hpp): the evening's scene, its timeline and what it
// measured.
//
// ge:summary The hosting trial's scene: the hosts, the browsers, the joins, the refreshes, the hash
// ge:keywords net hosting trial scene browser join refresh hash
// ge:skill net
// ge:test tests/net_hosting_test.cpp
#include "net/hosting_trial.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

#include "net/rendezvous.hpp"
#include "net/sim_router.hpp"

namespace ge::net {

namespace {

constexpr int64_t kTick = 10000;  // every node updates at 100 Hz
constexpr int64_t kEnd = 28000000;
constexpr int64_t kRefresh[3] = {6000000, 15000000, 22000000};
constexpr int64_t kRefreshGiveUp = 5000000;  // a refresh not done by then is drawn as it stands
constexpr int64_t kHomeStart = 14000000, kHomeStop = 20000000;
constexpr uint16_t kGamePort = 7000, kDiscovery = 27500, kPortmapPort = 5350, kBrowserPort = 27600;
const Address kServer = sim_address(100, 3478);

uint32_t trial_protocol() { return protocol_id("net-host", 1); }

NodeConfig node_config(bool accept, uint64_t seed) {
  NodeConfig c;
  c.protocol = trial_protocol();
  c.accept = accept;
  c.channels.count = 1;
  c.channels.delivery[0] = Delivery::reliable;
  c.keep_foreign = true;
  c.seed = seed;
  return c;
}

struct HostEnd {
  std::string name;
  Transport t, disc, pm;
  Node n;
  SessionHost s;
  Hosting h;
  std::unique_ptr<SimRouter> router;
};

struct PlayerEnd {
  Transport t;
  Node n;
  SessionClient c;
};

struct JoinEnd {
  TrialJoin result;
  HostEnd* host = nullptr;
  Transport t;
  Node n;
  ReachClient reach;
  bool by_reach = false, attached = false, said = false;
  bool host_heard = false, echoed = false;
  int64_t at_us = 0;
  SessionClient c;
};

struct BrowserEnd {
  std::string name;
  uint32_t node = 0;
  Transport t;
  Directory d;
  TrialBrowser result;
  int refresh = -1;  // the refresh in progress, -1: none
  bool joined = false;
};

int add_nat(SimNetwork& net, int kind, uint32_t public_node, std::vector<uint32_t> inside,
            uint32_t gateway) {
  if (kind == kTrialOpen) return -1;
  NatConfig nc;
  nc.kind = static_cast<NatKind>(kind);
  nc.public_node = public_node;
  nc.inside = std::move(inside);
  nc.gateway = gateway;
  return sim_add_nat(net, nc);
}

std::unique_ptr<HostEnd> make_host(SimNetwork& net, uint32_t node, const char* name, Visibility vis,
                                   int max_players, bool host_plays, bool locked, uint32_t seed,
                                   int nat, uint8_t router_speaks, uint32_t gateway) {
  auto e = std::make_unique<HostEnd>();
  e->name = name;
  open_sim_transport(e->t, net, sim_address(node, kGamePort));
  open_sim_transport(e->disc, net, sim_address(node, kDiscovery));
  node_init(e->n, node_config(true, uint64_t{seed} * 1000 + node), e->t);
  SessionConfig sc;
  sc.max_players = max_players;
  sc.need_ready = false;
  sc.host_plays = host_plays;
  sc.host_name = "host";
  sc.seed = uint64_t{seed} * 77 + node;
  session_host_init(e->s, sc, e->n, 0);
  HostingConfig hc;
  hc.protocol = trial_protocol();
  hc.name = name;
  hc.visibility = vis;
  hc.locked = locked;
  hc.rendezvous = kServer;
  hc.seed = uint64_t{seed} * 13 + node;
  Transport* router = nullptr;
  if (router_speaks && nat >= 0 && gateway != kSimNoNode) {
    SimRouterConfig rc;
    rc.nat = nat;
    rc.protocols = router_speaks;
    e->router = std::make_unique<SimRouter>();
    sim_router_init(*e->router, net, rc, 0);
    open_sim_transport(e->pm, net, sim_address(node, kPortmapPort));
    hc.portmap.gateway = sim_address(gateway, 5351);
    hc.portmap.seed = seed;
    router = &e->pm;
  }
  hosting_start(e->h, hc, e->n, e->s, &e->disc, router, 0);
  return e;
}

uint64_t fold(uint64_t h, int64_t v) {
  const auto u = static_cast<uint64_t>(v);
  for (int i = 0; i < 8; i++) {
    h ^= (u >> (8 * i)) & 0xff;
    h *= 1099511628211ull;
  }
  return h;
}

uint64_t fold(uint64_t h, const std::string& s) {
  for (const char c : s) h = fold(h, c);
  return fold(h, static_cast<int64_t>(s.size()));
}

void snapshot(BrowserEnd& b, int64_t now_us, bool done) {
  TrialRefresh& r = b.result.refreshes.back();
  r.done_us = done ? now_us - r.at_us : -1;
  r.stats = b.d.stats;
  std::vector<int> rows;
  directory_view(b.d, DirectoryFilter{}, DirectorySort::ping, rows);
  for (const int i : rows) {
    const DirectoryEntry& e = b.d.entries[static_cast<size_t>(i)];
    TrialRow row;
    row.name = e.advert.name;
    row.seated = e.advert.seated;
    row.max_players = e.advert.max_players;
    row.phase = e.advert.phase;
    row.locked = (e.advert.flags & kAdvertLocked) != 0;
    row.from = e.from;
    row.ping_ms = e.ping_ms;
    r.rows.push_back(row);
  }
  b.refresh = -1;
}

int listed(const Rendezvous& rv) {
  int n = 0;
  for (const RendezvousSession& s : rv.sessions) n += s.listed ? 1 : 0;
  return n;
}

}  // namespace

HostTrial run_host_trial(const HostTrialConfig& cfg) {
  SimNetwork net = make_sim_network(cfg.seed, cfg.link);
  Transport st;
  open_sim_transport(st, net, kServer);
  Rendezvous rv;
  RendezvousConfig rc;
  rc.seed = cfg.seed;
  rendezvous_init(rv, rc, st);

  // The NATs first: a host's router binds on its NAT's gateway.
  const bool home_router = cfg.home_router && cfg.home_nat != kTrialOpen;
  const int home_nat = add_nat(net, cfg.home_nat, 11, {1, 3}, home_router ? 21 : kSimNoNode);
  add_nat(net, cfg.away_nat, 12, {2}, kSimNoNode);
  add_nat(net, static_cast<int>(NatKind::port_restricted), 15, {5}, kSimNoNode);
  add_nat(net, static_cast<int>(NatKind::symmetric), 16, {6}, kSimNoNode);

  std::vector<std::unique_ptr<HostEnd>> hosts;
  hosts.push_back(make_host(net, 1, "home", Visibility::listed, 8, true, false, cfg.seed, home_nat,
                            home_router ? cfg.home_router : 0, 21));
  HostEnd& home = *hosts.back();
  for (uint32_t i = 0; i < 10; i++) {
    char name[24];
    std::snprintf(name, sizeof name, "arena %02u", i + 1);
    hosts.push_back(make_host(net, 40 + i, name, Visibility::listed, i % 2 ? 16 : 8, false, i == 2,
                              cfg.seed, -1, 0, kSimNoNode));
  }
  hosts.push_back(make_host(net, 50, "friends only", Visibility::invite, 8, false, false, cfg.seed,
                            -1, 0, kSimNoNode));
  HostEnd& friends_only = *hosts.back();
  hosts.push_back(
      make_host(net, 6, "attic", Visibility::listed, 4, true, false, cfg.seed, -1, 0, kSimNoNode));
  HostEnd& attic = *hosts.back();
  hosts.push_back(
      make_host(net, 5, "basement", Visibility::lan, 8, true, false, cfg.seed, -1, 0, kSimNoNode));

  // The arenas' players: arena i seats i % 4 (nobody on the first).
  std::vector<std::unique_ptr<PlayerEnd>> players;
  for (uint32_t i = 0; i < 10; i++) {
    for (uint32_t j = 0; j < i % 4; j++) {
      auto p = std::make_unique<PlayerEnd>();
      const uint32_t node = 200 + i * 4 + j;
      open_sim_transport(p->t, net, sim_address(node, kGamePort));
      node_init(p->n, node_config(false, uint64_t{cfg.seed} * 1000 + node), p->t);
      session_client_init(p->c, p->n, 0);
      session_client_join(p->c, sim_address(40 + i, kGamePort), "player", 0, 100000);
      players.push_back(std::move(p));
    }
  }

  std::vector<std::unique_ptr<BrowserEnd>> browsers;
  for (const auto& [name, node] : {std::pair{"friend", 3u}, std::pair{"away", 2u}}) {
    auto b = std::make_unique<BrowserEnd>();
    b->name = name;
    b->node = node;
    b->result.name = name;
    open_sim_transport(b->t, net, sim_address(node, kBrowserPort));
    DirectoryConfig dc;
    dc.protocol = trial_protocol();
    dc.lan = sim_broadcast(kDiscovery);
    dc.rendezvous = kServer;
    dc.seed = uint64_t{cfg.seed} * 31 + node;
    directory_init(b->d, dc, b->t);
    browsers.push_back(std::move(b));
  }

  std::vector<std::unique_ptr<JoinEnd>> joins;
  const auto join = [&](BrowserEnd& b, HostEnd& host, const JoinRoute& route, int64_t now) {
    auto j = std::make_unique<JoinEnd>();
    j->result.browser = b.name;
    j->result.host = host.name;
    j->result.lan = route.lan;
    j->host = &host;
    j->at_us = now;
    const auto port = static_cast<uint16_t>(7100 + joins.size());
    open_sim_transport(j->t, net, sim_address(b.node, port));
    node_init(j->n,
              node_config(false, uint64_t{cfg.seed} * 1000 + uint64_t{b.node} * 10 + joins.size()),
              j->t);
    session_client_init(j->c, j->n, 0);
    if (route.lan) {
      session_client_join(j->c, route.address, b.name.c_str(), 0, now);
    } else {
      ReachConfig rcfg;
      rcfg.rendezvous = kServer;
      rcfg.session = route.session;
      rcfg.seed = uint64_t{cfg.seed} * 7 + port;
      reach_client_init(j->reach, rcfg, j->n);
      reach_client_start(j->reach, now);
      j->by_reach = true;
    }
    joins.push_back(std::move(j));
  };
  const auto route_to = [](const BrowserEnd& b, const std::string& name, JoinRoute& out) {
    for (const DirectoryEntry& e : b.d.entries) {
      if (name == e.advert.name) {
        out = directory_route(e);
        return true;
      }
    }
    return false;
  };

  HostTrial out;
  for (int64_t now = kTick; now <= kEnd; now += kTick) {
    sim_advance(net, now);
    if (home.router) sim_router_update(*home.router, now);
    rendezvous_update(rv, now);
    for (auto& h : hosts) {
      node_update(h->n, now);
      session_host_update(h->s, now);
      for (const SessionEvent& e : h->s.events) {
        if (e.type != SessionEventType::chat) continue;
        for (auto& j : joins) {
          if (j->host == h.get() && e.player == j->c.me) j->host_heard = true;
        }
      }
      hosting_update(h->h, now);
    }
    for (auto& p : players) {
      node_update(p->n, now);
      session_client_update(p->c, now);
    }
    for (auto& j : joins) {
      node_update(j->n, now);
      if (j->by_reach) {
        reach_client_update(j->reach, now);
        if (!j->attached && j->reach.state == ReachState::connected) {
          j->attached =
              session_client_attach(j->c, j->reach.peer, j->result.browser.c_str(), 0, now);
          j->result.path = j->reach.path;
        }
      }
      session_client_update(j->c, now);
      for (const SessionEvent& e : j->c.events) {
        if (e.type == SessionEventType::welcome) j->result.seated_ms = (now - j->at_us) / 1000;
        if (e.type == SessionEventType::chat && e.player == j->c.me) j->echoed = true;
      }
      if (j->result.seated_ms >= 0 && !j->said) {
        const std::string line = "hello from " + j->result.browser;
        j->said = session_client_chat(j->c, ChatScope::all, kNoPlayer, line.c_str(), line.size());
      }
    }
    for (auto& b : browsers) {
      for (int k = 0; k < 3; k++) {
        if (now == kRefresh[k]) {
          directory_refresh(b->d, now);
          b->refresh = k;
          b->result.refreshes.push_back({});
          b->result.refreshes.back().at_us = now;
        }
      }
      directory_update(b->d, now);
      if (b->refresh >= 0) {
        const bool done = directory_done(b->d, now);
        if (done || now - kRefresh[b->refresh] >= kRefreshGiveUp) {
          const int k = b->refresh;
          snapshot(*b, now, done);
          if (k == 0 && !b->joined) {
            b->joined = true;
            JoinRoute route;
            if (route_to(*b, "home", route)) join(*b, home, route, now);
            if (b->name == "away") {
              if (route_to(*b, "attic", route)) join(*b, attic, route, now);
              JoinRoute invite;
              invite.session = hosting_invite(friends_only.h);  // handed over out of band
              join(*b, friends_only, invite, now);
            }
          }
        }
      }
    }
    // The arena that is playing, once its players are in; HOME starts and stops.
    if (now == 1000000) session_host_start(hosts[8]->s, session_leader(hosts[8]->s));
    if (now == kHomeStart) session_host_start(home.s, 0);
    if (now == kHomeStop - kTick) out.listed_at_server = listed(rv);
    if (now == kHomeStop) hosting_stop(home.h, now);
    if (now == kHomeStop + 1500000) {
      out.listed_after_stop = listed(rv);
      for (const RendezvousSession& s : rv.sessions)
        out.home_listed_after_stop |= s.listed && s.id == hosting_invite(home.h);
    }
    if (home.h.forwarded && out.home_forward_ms < 0) out.home_forward_ms = now / 1000;
    for (auto& h : hosts) node_flush(h->n, now);
    for (auto& p : players) node_flush(p->n, now);
    for (auto& j : joins) node_flush(j->n, now);
  }
  out.home_portmap = home.h.mapping ? home.h.portmap.state : PortmapState::idle;
  out.home_proto = home.h.portmap.proto;
  out.home_forward_closed = !home.router || sim_router_forwards(*home.router, kEnd) == 0;
  out.home = home.h.stats;
  out.server = rv.stats;
  for (auto& b : browsers) out.browsers.push_back(b->result);
  // In the scene's order, whichever browser finished its refresh first.
  for (const auto& [browser, host] :
       {std::pair{"friend", "home"}, std::pair{"away", "home"}, std::pair{"away", "attic"},
        std::pair{"away", "friends only"}}) {
    for (auto& j : joins) {
      if (j->result.browser != browser || j->result.host != host) continue;
      j->result.chat_heard = j->host_heard && j->echoed;
      out.joins.push_back(j->result);
    }
  }
  rendezvous_close(rv);

  uint64_t h = 1469598103934665603ull;
  for (const TrialBrowser& b : out.browsers) {
    h = fold(h, b.name);
    for (const TrialRefresh& r : b.refreshes) {
      h = fold(h, r.done_us);
      for (const TrialRow& row : r.rows) {
        h = fold(fold(fold(h, row.name), row.seated), row.ping_ms);
        h = fold(fold(h, static_cast<int64_t>(row.phase)), row.from);
      }
    }
  }
  for (const TrialJoin& j : out.joins) {
    h = fold(fold(h, j.host), j.seated_ms);
    h = fold(fold(h, static_cast<int64_t>(j.path)), j.chat_heard);
  }
  h = fold(fold(h, out.home_forward_ms), out.listed_at_server);
  h = fold(fold(h, out.listed_after_stop), static_cast<int64_t>(out.server.lists));
  h = fold(h, out.home_listed_after_stop);
  out.hash = h;
  return out;
}

}  // namespace ge::net

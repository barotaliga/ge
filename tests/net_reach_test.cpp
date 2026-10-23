// engine/net/rendezvous.hpp and reach.hpp over the simulated network: a client reaches a host
// behind every pair of NAT kinds, straight through a punched hole where the kinds allow one and
// through the server's relay where they do not; messages flow both ways on the path found; an
// unknown session and a missing relay fail with their reason; STUN answers on the server's port.
//
// ge:summary Test: rendezvous, hole punching and the relay across every pair of NAT kinds
// ge:keywords test net reach rendezvous nat hole-punching relay stun direct path
// ge:skill net testing
#include "net/reach.hpp"

#include <cstring>
#include <memory>

#include "check.hpp"
#include "net/rendezvous.hpp"
#include "net/stun.hpp"

using namespace ge::net;

namespace {

constexpr int kOpen = -1;  // no NAT
const Address kServer = sim_address(100, 3478);

struct End {
  Transport t;
  Node n;
};

std::unique_ptr<End> make_end(SimNetwork& net, uint32_t node, int nat_kind, bool host) {
  if (nat_kind != kOpen) {
    NatConfig nc;
    nc.kind = static_cast<NatKind>(nat_kind);
    nc.public_node = node + 10;
    nc.inside = {node};
    sim_add_nat(net, nc);
  }
  auto e = std::make_unique<End>();
  open_sim_transport(e->t, net, sim_address(node, 7000));
  NodeConfig c;
  c.protocol = protocol_id("reach-test", 1);
  c.accept = host;
  c.channels.count = 1;
  c.channels.delivery[0] = Delivery::reliable;
  c.keep_foreign = true;
  c.seed = node + 1;
  node_init(e->n, c, e->t);
  return e;
}

struct Outcome {
  ReachState state = ReachState::idle;
  ReachWhy why = ReachWhy::none;
  ReachPath path = ReachPath::none;
  int64_t connect_us = 0;
  bool host_heard = false, client_heard = false;  // a message each way on the path found
  RendezvousStats server;
};

Outcome reach(int host_kind, int client_kind, uint64_t session_asked = 42, int max_relays = 8) {
  LinkConditions lc;
  lc.latency_ms = 20;
  SimNetwork net = make_sim_network(3, lc);
  Transport st;
  open_sim_transport(st, net, kServer);
  Rendezvous rv;
  RendezvousConfig rc;
  rc.max_relays = max_relays;
  rc.seed = 9;
  rendezvous_init(rv, rc, st);
  auto host = make_end(net, 1, host_kind, true);
  auto client = make_end(net, 2, client_kind, false);
  ReachConfig cfg;
  cfg.rendezvous = kServer;
  cfg.session = 42;
  ReachHost h;
  reach_host_init(h, cfg, host->n);
  cfg.session = session_asked;
  cfg.seed = 5;
  ReachClient c;
  reach_client_init(c, cfg, client->n);
  Outcome out;
  bool said = false;
  for (int64_t now = 10000; now <= 20000000; now += 10000) {
    sim_advance(net, now);
    rendezvous_update(rv, now);
    node_update(host->n, now);
    node_update(client->n, now);
    reach_host_update(h, now);
    if (now == 500000) reach_client_start(c, now);
    reach_client_update(c, now);
    for (const Event& e : host->n.events) out.host_heard |= e.type == EventType::message;
    for (const Event& e : client->n.events) out.client_heard |= e.type == EventType::message;
    if (c.state == ReachState::connected && !said) {
      const uint8_t m[4] = {'p', 'i', 'n', 'g'};
      node_send(client->n, c.peer, 0, m, sizeof m);
      for (int p = 0; p < kMaxPeers && p < static_cast<int>(host->n.peers.size()); p++) {
        if (peer_connected(host->n, p)) node_send(host->n, p, 0, m, sizeof m);
      }
      said = true;
    }
    node_flush(host->n, now);
    node_flush(client->n, now);
    if (c.state == ReachState::failed || (out.host_heard && out.client_heard)) break;
  }
  out.state = c.state;
  out.why = c.why;
  out.path = c.path;
  out.connect_us = c.connected_us;
  out.server = rv.stats;
  rendezvous_close(rv);
  return out;
}

// Whether a pair of kinds can punch a hole, by RFC 4787's behaviours: a symmetric NAT gives the
// pair a port the other side has never sent to, which a port-restricted or symmetric NAT on the
// other side filters out.
bool punchable(int a, int b) {
  const auto hard = [](int k) {
    return k == static_cast<int>(NatKind::port_restricted) ||
           k == static_cast<int>(NatKind::symmetric);
  };
  const int sym = static_cast<int>(NatKind::symmetric);
  return !((a == sym && hard(b)) || (b == sym && hard(a)));
}

}  // namespace

TEST(reach_finds_a_path_through_every_pair_of_nats) {
  for (int hk = kOpen; hk <= static_cast<int>(NatKind::symmetric); hk++) {
    for (int ck = kOpen; ck <= static_cast<int>(NatKind::symmetric); ck++) {
      const Outcome o = reach(hk, ck);
      const bool ok = o.state == ReachState::connected && o.host_heard && o.client_heard;
      CHECK_MSG(ok,
                "a client failed to reach its host and trade a message: the pair of NAT kinds "
                "is in the loop's indices; run the net-reach lab for the matrix; skill: net");
      const ReachPath want = punchable(hk, ck) ? ReachPath::direct : ReachPath::relay;
      CHECK_MSG(o.path == want,
                "a pair took the wrong path (direct where a hole punches, relay where none does): "
                "run the net-reach lab's matrix; skill: net");
      if (want == ReachPath::relay) CHECK(o.server.relayed > 0);
    }
  }
}

TEST(reach_fails_an_unknown_session_and_a_missing_relay) {
  const Outcome unknown = reach(kOpen, kOpen, 7);
  CHECK(unknown.state == ReachState::failed && unknown.why == ReachWhy::unknown_session);
  CHECK(unknown.server.unknown > 0);
  const int sym = static_cast<int>(NatKind::symmetric);
  const Outcome no_relay = reach(sym, sym, 42, 0);
  CHECK(no_relay.state == ReachState::failed && no_relay.why == ReachWhy::no_relay);
  CHECK(no_relay.server.no_relay > 0);
}

TEST(rendezvous_answers_stun_with_the_nat_s_public_face) {
  SimNetwork net = make_sim_network(1, {});
  Transport st;
  open_sim_transport(st, net, kServer);
  Rendezvous rv;
  rendezvous_init(rv, {}, st);
  NatConfig nc;
  nc.kind = NatKind::port_restricted;
  nc.public_node = 11;
  nc.inside = {1};
  sim_add_nat(net, nc);
  Transport t;
  open_sim_transport(t, net, sim_address(1, 7000));
  const uint8_t id[12] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8};
  uint8_t req[kStunRequestSize];
  stun_request(req, id);
  transport_send(t, kServer, req, sizeof req, 0);
  sim_advance(net, 1000);
  rendezvous_update(rv, 1000);
  sim_advance(net, 2000);
  uint8_t buf[kMaxDatagram];
  Address from, mapped;
  const int n = transport_receive(t, buf, sizeof buf, from, 2000);
  CHECK(n > 0 && from == kServer);
  CHECK(stun_read_answer(buf, static_cast<size_t>(n), id, mapped));
  CHECK(mapped == sim_address(11, 40000) && rv.stats.stun == 1);
}

TEST(rendezvous_never_answers_bigger_than_it_was_asked) {
  SimNetwork net = make_sim_network(1, {});
  Transport st;
  open_sim_transport(st, net, kServer);
  Rendezvous rv;
  rendezvous_init(rv, {}, st);
  Transport t;
  open_sim_transport(t, net, sim_address(1, 7000));
  // An unpadded register (magic, kind, session): refused, nothing comes back.
  const uint8_t small[13] = {'g', 'e', 'R', 'V', 1, 42};
  transport_send(t, kServer, small, sizeof small, 0);
  sim_advance(net, 1000);
  rendezvous_update(rv, 1000);
  sim_advance(net, 2000);
  uint8_t buf[kMaxDatagram];
  Address from;
  CHECK(transport_receive(t, buf, sizeof buf, from, 2000) < 0);
  CHECK(rv.stats.refused == 1 && rv.stats.registered == 0);
}

// Tests: the tunnel (engine/net/tunnel.hpp). An echo game serves on one port in the simulated
// network: a client through a node and a plain client are both answered, each by the road it
// came; a datagram past the node's message size goes in pieces and comes back whole; what a client
// sends before its node is accepted is held and goes once it is; a host with no node fails the
// client's connect, and the same inner transport carries the client's game plainly from then on; a
// lost piece loses its datagram and is counted.
//
// ge:summary Tests: the tunnel carries a game's datagrams inside a node beside plain ones
// ge:keywords test tunnel native-protocol pieces keep_foreign one-port fallback
// ge:skill net testing
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "net/tunnel.hpp"

using namespace ge::net;

namespace {

TunnelConfig config(uint64_t seed) {
  TunnelConfig c;
  c.protocol = protocol_id("tunnel-test", 1);
  c.seed = seed;
  return c;
}

// The game behind the host: every datagram it reads is sent back to whoever sent it.
struct Echo {
  Transport outer;
  Tunnel tunnel;
  bool tunneled = true;
  Transport& game() { return tunneled ? tunnel.inner : outer; }
  int answered = 0;
  void frame(int64_t now) {
    if (tunneled) tunnel_update(tunnel, now);
    uint8_t buf[kMaxDatagram];
    Address from;
    for (int n; (n = transport_receive(game(), buf, sizeof buf, from, now)) >= 0;) {
      transport_send(game(), from, buf, static_cast<size_t>(n), now);
      answered++;
    }
    if (tunneled) tunnel_flush(tunnel, now);
  }
};

struct Client {
  Transport outer;
  Tunnel tunnel;
  bool tunneled = false;
  std::vector<std::vector<uint8_t>> heard;
  Transport& game() { return tunneled ? tunnel.inner : outer; }
  void frame(int64_t now) {
    if (tunneled) tunnel_update(tunnel, now);
    uint8_t buf[kMaxDatagram];
    Address from;
    for (int n; (n = transport_receive(game(), buf, sizeof buf, from, now)) >= 0;)
      heard.emplace_back(buf, buf + n);
    if (tunneled) tunnel_flush(tunnel, now);
  }
  void say(const std::vector<uint8_t>& d, int64_t now) {
    transport_send(game(), sim_address(0, 1), d.data(), d.size(), now);
  }
};

std::vector<uint8_t> bytes(size_t n, uint8_t seed) {
  std::vector<uint8_t> v(n);
  for (size_t i = 0; i < n; i++) v[i] = static_cast<uint8_t>(seed + i * 7);
  return v;
}

int64_t run(SimNetwork& net, Echo& host, const std::vector<Client*>& clients, int64_t now, int ms) {
  for (int i = 0; i < ms; i += 10) {
    now += 10000;
    sim_advance(net, now);
    host.frame(now);
    for (Client* c : clients) c->frame(now);
  }
  return now;
}

bool heard_back(const Client& c, const std::vector<uint8_t>& d) {
  for (const std::vector<uint8_t>& h : c.heard)
    if (h == d) return true;
  return false;
}

}  // namespace

TEST(a_tunneled_and_a_plain_client_are_answered_on_one_port) {
  LinkConditions lan;
  lan.latency_ms = 10;
  SimNetwork net = make_sim_network(1, lan);
  Echo host;
  CHECK(open_sim_transport(host.outer, net, sim_address(0, 1)));
  CHECK(tunnel_host(host.tunnel, host.outer, config(1)));
  Client native, plain;
  CHECK(open_sim_transport(native.outer, net, sim_address(1, 1)));
  CHECK(open_sim_transport(plain.outer, net, sim_address(2, 1)));
  native.tunneled = true;
  int64_t now = 0;
  CHECK(tunnel_connect(native.tunnel, native.outer, sim_address(0, 1), config(2), now));
  // Said before the node is accepted: held, then carried.
  const std::vector<uint8_t> early = bytes(40, 1);
  native.say(early, now);
  plain.say(bytes(30, 2), now);
  now = run(net, host, {&native, &plain}, now, 300);
  CHECK(tunnel_joined(native.tunnel));
  CHECK(tunnel_carried(host.tunnel, sim_address(1, 1)));
  CHECK(!tunnel_carried(host.tunnel, sim_address(2, 1)));
  CHECK_MSG(heard_back(native, early), "the datagram held for the accept never came back");
  CHECK(heard_back(plain, bytes(30, 2)));
  // A datagram larger than one of the node's messages goes in pieces and comes back whole.
  const std::vector<uint8_t> big = bytes(1450, 3);
  native.say(big, now);
  run(net, host, {&native, &plain}, now, 200);
  CHECK_MSG(heard_back(native, big), "the datagram in pieces did not come back whole");
  CHECK(host.tunnel.stats.carried_in == 2 && host.tunnel.stats.carried_out == 2);
  CHECK(host.tunnel.stats.plain_in == 1 && host.tunnel.stats.plain_out == 1);
  // Only the plain client's datagrams crossed the network as they were sent.
  CHECK(native.tunnel.stats.plain_out == 0);
}

TEST(a_host_with_no_node_fails_the_connect_and_the_client_goes_plain) {
  LinkConditions lan;
  lan.latency_ms = 10;
  SimNetwork net = make_sim_network(1, lan);
  Echo host;
  host.tunneled = false;  // a server of the older protocol alone
  CHECK(open_sim_transport(host.outer, net, sim_address(0, 1)));
  Client c;
  CHECK(open_sim_transport(c.outer, net, sim_address(1, 1)));
  c.tunneled = true;
  TunnelConfig cfg = config(3);
  cfg.connect_timeout_us = 500000;
  int64_t now = 0;
  CHECK(tunnel_connect(c.tunnel, c.outer, sim_address(0, 1), cfg, now));
  now = run(net, host, {&c}, now, 700);
  CHECK(c.tunnel.failed);
  // The echo answered the node's requests as datagrams of its own protocol; none reached the game.
  CHECK(c.heard.empty());
  c.heard.clear();
  c.say(bytes(20, 4), now);  // the same inner transport, now plain
  run(net, host, {&c}, now, 100);
  CHECK(heard_back(c, bytes(20, 4)));
}

TEST(a_lost_piece_loses_its_datagram_and_is_counted) {
  LinkConditions lan;
  lan.latency_ms = 10;
  SimNetwork net = make_sim_network(1, lan);
  Echo host;
  CHECK(open_sim_transport(host.outer, net, sim_address(0, 1)));
  CHECK(tunnel_host(host.tunnel, host.outer, config(5)));
  Client c;
  CHECK(open_sim_transport(c.outer, net, sim_address(1, 1)));
  c.tunneled = true;
  int64_t now = 0;
  CHECK(tunnel_connect(c.tunnel, c.outer, sim_address(0, 1), config(6), now));
  now = run(net, host, {&c}, now, 300);
  CHECK(tunnel_joined(c.tunnel));
  // A piece made by hand that never gets its partner, then a whole datagram after it.
  uint8_t half[kTunnelHeader + 8] = {9, 0, 0, 2, 1, 2, 3, 4, 5, 6, 7, 8};
  CHECK(node_send(*c.tunnel.node, c.tunnel.server_peer, 0, half, sizeof half));
  c.say(bytes(1450, 7), now);
  run(net, host, {&c}, now, 200);
  CHECK(host.tunnel.stats.lost == 1);
  CHECK(heard_back(c, bytes(1450, 7)));
  CHECK(host.answered == 1);
}

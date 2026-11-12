// Forward (engine/forward/forward.hpp) on the simulated network: a route parses or says why not; a
// game's client and server talk through a chain of two forwarders, the game's own handshake and
// messages untouched; a destination off the allow list is refused with the reason; a client whose
// NAT gives it a new port is followed by its alive; a silent client is dropped; and no answer a
// forwarder gives is bigger than the datagram that asked for it.
//
// ge:summary Tests forward: routes, a two-hop chain with a real game, allow list, rebinding, idle
// ge:keywords test forward route chain hop allow-list nat-rebinding idle amplification status stun
// ge:skill net testing
#include <string>
#include <vector>

#include "check.hpp"
#include "forward/forward.hpp"
#include "net/node.hpp"
#include "net/packet.hpp"
#include "net/sim_network.hpp"
#include "net/status.hpp"
#include "net/stun.hpp"

namespace fw = ge::forward;
namespace net = ge::net;

namespace {

// Clients on node 0, forwarders on 1 and 2 (port 30000, their upstream sockets from 40000), the
// game server on node 3.
struct World {
  net::SimNetwork sim;
  int64_t now = 0;
  net::Transport f1_port, f2_port;
  fw::Forward f1, f2;
  uint16_t next_up = 40000;
  const net::Address f1_at = net::sim_address(1, 30000), f2_at = net::sim_address(2, 30000);
  const net::Address server_at = net::sim_address(3, 27500);

  explicit World(const net::LinkConditions& link = {}) : sim(net::make_sim_network(9, link)) {
    net::open_sim_transport(f1_port, sim, f1_at);
    net::open_sim_transport(f2_port, sim, f2_at);
    fw::ForwardConfig c;
    c.name = "f1";
    c.allow = {f2_at, net::sim_address(3, 0)};
    c.seed = 1;
    fw::forward_init(f1, c, f1_port, opener(1));
    c.name = "f2";
    c.allow = {server_at};
    c.seed = 2;
    fw::forward_init(f2, c, f2_port, opener(2));
  }
  ~World() {
    fw::forward_close(f1);
    fw::forward_close(f2);
  }
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  fw::OpenUpstream opener(uint32_t node) {
    return [this, node](net::Transport& t) {
      return net::open_sim_transport(t, sim, net::sim_address(node, next_up++));
    };
  }

  void forwarders() {
    fw::forward_update(f1, now);
    fw::forward_update(f2, now);
  }
  void advance(int64_t dt) {
    now += dt;
    net::sim_advance(sim, now);
  }
};

// A dial on `t` until it settles; the datagrams that are not the dial's are kept in `other`.
void dial(World& w, fw::Dial& d, net::Transport& t, std::vector<std::vector<uint8_t>>* other) {
  for (int i = 0; i < 400 && d.state == fw::DialState::asking; i++) {
    fw::dial_update(d, t, w.now);
    w.forwarders();
    w.advance(5000);
    uint8_t buf[net::kMaxDatagram];
    net::Address from;
    int n = 0;
    while ((n = net::transport_receive(t, buf, sizeof buf, from, w.now)) >= 0)
      if (!fw::dial_read(d, buf, static_cast<size_t>(n), from, w.now) && other != nullptr)
        other->emplace_back(buf, buf + n);
  }
}

}  // namespace

TEST(a_route_parses_or_says_why_not) {
  std::vector<net::Address> hops;
  std::string error;
  CHECK(fw::parse_route("sim:2:30000@sim:3:27500", hops, error));
  CHECK(hops.size() == 2);
  CHECK(hops[1] == net::sim_address(3, 27500));
  CHECK(fw::route_text(hops) == "sim:2:30000@sim:3:27500");
  CHECK(fw::parse_route("10.0.0.1:27500", hops, error));
  CHECK(!fw::parse_route("10.0.0.1", hops, error));  // no port
  CHECK(error.find("not an address with a port") != std::string::npos);
  CHECK(!fw::parse_route("", hops, error));
  CHECK(!fw::parse_route(std::string(300, 'a'), hops, error));
  std::string many;
  for (int i = 0; i < 9; i++) many += (many.empty() ? "" : "@") + std::string("10.0.0.1:1");
  CHECK(!fw::parse_route(many, hops, error));
}

TEST(a_game_plays_through_a_chain_of_two_forwarders) {
  World w(net::LinkConditions{20, 2, 0.02});
  net::Transport client_port, server_port;
  net::open_sim_transport(client_port, w.sim, net::sim_address(0, 5000));
  net::open_sim_transport(server_port, w.sim, w.server_at);
  // The route first: the client's half, through f1 and f2 to the server.
  fw::Dial d;
  const std::vector<net::Address> chain = {w.f1_at, w.f2_at, w.server_at};
  fw::dial_start(d, chain, 77, w.now);
  CHECK(d.rest == "sim:2:30000@sim:3:27500");
  dial(w, d, client_port, nullptr);
  CHECK(d.state == fw::DialState::routed);
  CHECK(w.f1.clients.size() == 1 && w.f2.clients.size() == 1);
  CHECK(w.f1.clients[0]->connected && w.f1.clients[0]->chained);

  // Then the game's own nodes, the client connecting to f1 as if it were the server.
  net::NodeConfig hc;
  hc.protocol = 0x77;
  hc.accept = true;
  hc.max_peers = 4;
  hc.channels.count = 1;
  hc.channels.delivery[0] = net::Delivery::reliable;
  hc.seed = 3;
  net::NodeConfig cc = hc;
  cc.accept = false;
  cc.keep_foreign = true;  // the forwarder's alive answers arrive on the game's socket
  cc.seed = 4;
  net::Node host, client;
  net::node_init(host, hc, server_port);
  net::node_init(client, cc, client_port);
  const int peer = net::node_connect(client, w.f1_at, w.now);
  CHECK(peer >= 0);
  int got_up = 0, got_down = 0, host_peer = -1;
  bool connected = false;
  for (int i = 0; i < 2000 && (got_up < 20 || got_down < 20); i++) {
    net::node_update(client, w.now);
    net::node_update(host, w.now);
    for (const net::Foreign& f : client.foreign)
      fw::dial_read(d, client.foreign_bytes.data() + f.offset, f.size, f.from, w.now);
    for (const net::Event& e : client.events) {
      if (e.type == net::EventType::connected) connected = true;
      if (e.type == net::EventType::message) got_down++;
    }
    for (const net::Event& e : host.events) {
      if (e.type == net::EventType::connected) host_peer = e.peer;
      if (e.type == net::EventType::message) got_up++;
    }
    if (connected && i % 5 == 0 && got_up < 20) {
      const uint8_t m[3] = {1, 2, 3};
      net::node_send(client, peer, 0, m, sizeof m);
    }
    if (host_peer >= 0 && i % 5 == 0 && got_down < 20) {
      const uint8_t m[4] = {4, 5, 6, 7};
      net::node_send(host, host_peer, 0, m, sizeof m);
    }
    fw::dial_update(d, client_port, w.now);
    net::node_flush(client, w.now);
    net::node_flush(host, w.now);
    w.forwarders();
    w.advance(10000);
  }
  CHECK(connected);
  CHECK(got_up >= 20 && got_down >= 20);
  // The server sees f2's socket for this client, not the client.
  CHECK(host.peers[static_cast<size_t>(host_peer)].address.port >= 40000);
  CHECK(d.rtt_us > 0);  // the first hop answered the client's alives
  CHECK(w.f1.clients[0]->packets_up > 20 && w.f2.clients[0]->packets_down > 20);
  net::close_transport(client_port);
  net::close_transport(server_port);
}

TEST(a_destination_off_the_allow_list_is_refused_with_the_reason) {
  World w;
  net::Transport client_port;
  net::open_sim_transport(client_port, w.sim, net::sim_address(0, 5000));
  fw::Dial d;
  const std::vector<net::Address> chain = {w.f2_at, net::sim_address(3, 9999)};
  fw::dial_start(d, chain, 5, w.now);
  dial(w, d, client_port, nullptr);
  CHECK(d.state == fw::DialState::refused);
  CHECK(d.answer == fw::Answer::not_allowed);
  CHECK(d.why == "f2 does not forward to sim:3:9999");
  CHECK(w.f2.clients.empty());
  // Through f1 to f2 to the same place: f2's refusal comes back to the client through f1.
  fw::Dial d2;
  const std::vector<net::Address> chain2 = {w.f1_at, w.f2_at, net::sim_address(3, 9999)};
  fw::dial_start(d2, chain2, 6, w.now);
  dial(w, d2, client_port, nullptr);
  CHECK(d2.state == fw::DialState::refused);
  CHECK(d2.why == "f2 does not forward to sim:3:9999");
  CHECK(w.f1.clients.empty());
  net::close_transport(client_port);
}

TEST(a_rebound_client_is_followed_and_a_silent_one_dropped) {
  World w;
  net::Transport client_port, moved_port, server_port;
  net::open_sim_transport(client_port, w.sim, net::sim_address(0, 5000));
  net::open_sim_transport(moved_port, w.sim, net::sim_address(0, 5001));
  net::open_sim_transport(server_port, w.sim, w.server_at);
  fw::Dial d;
  const std::vector<net::Address> chain = {w.f2_at, w.server_at};
  fw::dial_start(d, chain, 8, w.now);
  dial(w, d, client_port, nullptr);
  CHECK(d.state == fw::DialState::routed);
  // The NAT moves the client: its next alive comes from the new port, and the server's answers
  // follow it there.
  d.next_us = w.now;
  fw::dial_update(d, moved_port, w.now);
  w.advance(1000);
  w.forwarders();
  CHECK(w.f2.stats.rebinds == 1);
  const uint8_t game[5] = {9, 8, 7, 6, 5};
  net::transport_send(moved_port, w.f2_at, game, sizeof game, w.now);
  w.advance(1000);
  w.forwarders();
  w.advance(1000);
  uint8_t buf[net::kMaxDatagram];
  net::Address from;
  int n = net::transport_receive(server_port, buf, sizeof buf, from, w.now);
  CHECK(n == 5);
  net::transport_send(server_port, from, buf, 5, w.now);
  w.advance(1000);
  w.forwarders();
  w.advance(1000);
  std::vector<std::vector<uint8_t>> other;
  while ((n = net::transport_receive(moved_port, buf, sizeof buf, from, w.now)) >= 0)
    if (!fw::dial_read(d, buf, static_cast<size_t>(n), from, w.now))
      other.emplace_back(buf, buf + n);
  CHECK(other.size() == 1 && other[0].size() == 5);
  // Silent both ways past the idle time: dropped.
  for (int i = 0; i < 20; i++) {
    w.advance(1000000);
    w.forwarders();
  }
  CHECK(w.f2.clients.empty());
  CHECK(w.f2.stats.idle_drops == 1);
  net::close_transport(client_port);
  net::close_transport(moved_port);
  net::close_transport(server_port);
}

TEST(no_answer_is_bigger_than_the_datagram_that_asked) {
  World w;
  net::Transport asker;
  net::open_sim_transport(asker, w.sim, net::sim_address(0, 6000));
  std::vector<std::vector<uint8_t>> asks;
  // A route request (padded), a short one, an alive for nobody, a status ask, STUN, junk.
  uint8_t route[fw::kForwardRouteSize] = {'g', 'e', 'F', 'W', 1};
  asks.emplace_back(route, route + sizeof route);
  asks.emplace_back(route, route + 30);  // short: not padded, no answer
  uint8_t alive[29] = {'g', 'e', 'F', 'W', 4, 1};
  asks.emplace_back(alive, alive + sizeof alive);
  uint8_t status[net::kStatusAskSize];
  net::status_ask(fw::forward_protocol(), 3, 0, status);
  asks.emplace_back(status, status + sizeof status);
  uint8_t stun[net::kStunRequestSize];
  const uint8_t id[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  net::stun_request(stun, id);
  asks.emplace_back(stun, stun + sizeof stun);  // 20 bytes: its answer would be 32, so none
  std::vector<uint8_t> big_stun(stun, stun + sizeof stun);
  big_stun.resize(40);
  big_stun[3] = 20;  // the length field: a 20-byte attribute (comprehension-optional)
  big_stun[20] = 0x80;
  big_stun[21] = 0x22;
  big_stun[23] = 16;
  asks.push_back(big_stun);
  const std::vector<uint8_t> junk = {1, 2, 3};
  asks.push_back(junk);
  int answered = 0;
  for (const auto& a : asks) {
    net::transport_send(asker, w.f1_at, a.data(), a.size(), w.now);
    w.advance(1000);
    w.forwarders();
    w.advance(1000);
    uint8_t buf[net::kMaxDatagram];
    net::Address from;
    int n = 0;
    while ((n = net::transport_receive(asker, buf, sizeof buf, from, w.now)) >= 0) {
      CHECK(static_cast<size_t>(n) <= a.size());
      answered++;
    }
  }
  CHECK(answered == 3);  // the route's cookie, the status ask's cookie, the padded STUN
  CHECK(w.f1.stats.stun_answers == 1);
  net::close_transport(asker);
}

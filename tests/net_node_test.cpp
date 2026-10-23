// engine/net/node.hpp over the simulated network: the handshake, a host's maximum (32 connect, the
// 33rd is denied), a host that takes no one, a strange protocol ignored, timeouts, disconnects and
// a connect to nobody failing.
//
// ge:summary Test: nodes' handshake, the 32-peer limit, denial, timeouts and disconnects
// ge:keywords test net node handshake max-peers deny timeout disconnect protocol
// ge:skill net testing
#include "net/node.hpp"

#include <memory>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

struct End {
  Transport t;
  Node n;
};

NodeConfig config(bool host, int max_peers, uint64_t seed) {
  NodeConfig c;
  c.protocol = protocol_id("node-test", 1);
  c.accept = host;
  c.max_peers = max_peers;
  c.channels.count = 1;
  c.channels.delivery[0] = Delivery::reliable;
  c.seed = seed;
  return c;
}

std::unique_ptr<End> make_end(SimNetwork& net, uint32_t node, bool host, int max_peers) {
  auto e = std::make_unique<End>();
  open_sim_transport(e->t, net, sim_address(node, 1));
  node_init(e->n, config(host, max_peers, node + 100), e->t);
  return e;
}

// Step every end `ms` milliseconds from `now`; returns the new time.
int64_t step(SimNetwork& net, std::vector<End*> ends, int64_t now, int ms,
             std::vector<std::vector<Event>>* seen = nullptr) {
  for (int i = 0; i < ms; i += 10) {
    now += 10000;
    sim_advance(net, now);
    for (size_t k = 0; k < ends.size(); k++) {
      node_update(ends[k]->n, now);
      if (seen) {
        for (const Event& e : ends[k]->n.events) (*seen)[k].push_back(e);
      }
    }
    for (End* e : ends) node_flush(e->n, now);
  }
  return now;
}

}  // namespace

TEST(node_host_takes_32_and_denies_the_33rd) {
  LinkConditions c;
  c.latency_ms = 20;
  SimNetwork net = make_sim_network(1, c);
  auto host = make_end(net, 0, true, 32);
  std::vector<std::unique_ptr<End>> clients;
  std::vector<End*> all{host.get()};
  for (int i = 0; i < 33; i++) {
    clients.push_back(make_end(net, static_cast<uint32_t>(i + 1), false, 1));
    all.push_back(clients.back().get());
  }
  std::vector<std::vector<Event>> seen(all.size());
  int64_t now = 0;
  for (int i = 0; i < 33; i++) {
    node_connect(clients[i]->n, sim_address(0, 1), now);
    now = step(net, all, now, 10, &seen);
  }
  now = step(net, all, now, 500, &seen);
  CHECK(node_connected_count(host->n) == 32);
  int connected = 0, denied = 0;
  for (size_t k = 1; k < all.size(); k++) {
    for (const Event& e : seen[k]) {
      connected += e.type == EventType::connected;
      denied += e.type == EventType::connect_failed && e.reason == Reason::denied_full;
    }
  }
  CHECK(connected == 32 && denied == 1);
  CHECK(host->n.stats.denied == 1);
  // A message from each client reaches the host from the right peer.
  for (int i = 0; i < 32; i++) {
    const uint8_t m = static_cast<uint8_t>(i);
    CHECK(node_send(clients[i]->n, 0, 0, &m, 1));
  }
  seen.assign(all.size(), {});
  step(net, all, now, 200, &seen);
  int got = 0;
  for (const Event& e : seen[0]) {
    if (e.type != EventType::message) continue;
    got++;
    // The client in slot e.peer is the one whose address the host holds.
    CHECK(host->n.peers[e.peer].address.family == Family::sim);
  }
  CHECK(got == 32);
}

TEST(node_closed_host_and_strange_protocols) {
  SimNetwork net = make_sim_network(1, {});
  auto host = make_end(net, 0, true, 4);
  auto client = make_end(net, 1, false, 1);
  host->n.closed = true;
  std::vector<std::vector<Event>> seen(2);
  node_connect(client->n, sim_address(0, 1), 0);
  step(net, {host.get(), client.get()}, 0, 100, &seen);
  CHECK(seen[1].size() == 1 && seen[1][0].reason == Reason::denied_closed);

  // A node of another protocol is never heard.
  auto other = std::make_unique<End>();
  open_sim_transport(other->t, net, sim_address(2, 1));
  NodeConfig oc = config(false, 1, 7);
  oc.protocol = protocol_id("node-test", 2);
  node_init(other->n, oc, other->t);
  host->n.closed = false;
  node_connect(other->n, sim_address(0, 1), 1000000);
  seen.assign(3, {});
  step(net, {host.get(), client.get(), other.get()}, 1000000, 300, &seen);
  CHECK(host->n.stats.foreign > 0);
  CHECK(node_connected_count(host->n) == 0);
}

TEST(node_times_out_disconnects_and_fails_to_reach_nobody) {
  SimNetwork net = make_sim_network(1, {});
  auto host = make_end(net, 0, true, 4);
  auto a = make_end(net, 1, false, 1);
  auto b = make_end(net, 2, false, 1);
  node_connect(a->n, sim_address(0, 1), 0);
  node_connect(b->n, sim_address(0, 1), 0);
  std::vector<std::vector<Event>> seen(3);
  int64_t now = step(net, {host.get(), a.get(), b.get()}, 0, 100, &seen);
  CHECK(node_connected_count(host->n) == 2);

  // a says goodbye: the host hears it at once.
  node_disconnect(a->n, 0, now);
  seen.assign(3, {});
  now = step(net, {host.get(), a.get(), b.get()}, now, 50, &seen);
  CHECK(seen[0].size() == 1 && seen[0][0].type == EventType::disconnected &&
        seen[0][0].reason == Reason::remote_closed);

  // b goes silent (its link cut): the host times it out after five seconds.
  LinkConditions cut;
  cut.loss = 1;
  sim_set_path(net, 0, 2, cut);
  seen.assign(3, {});
  now = step(net, {host.get(), b.get()}, now, 5200, &seen);
  CHECK(seen[0].size() == 1 && seen[0][0].reason == Reason::timed_out);
  CHECK(node_connected_count(host->n) == 0);

  // A connect to an address nobody holds fails after its timeout.
  auto lonely = make_end(net, 9, false, 1);
  node_connect(lonely->n, sim_address(42, 1), now);
  std::vector<std::vector<Event>> s1(1);
  step(net, {lonely.get()}, now, 5100, &s1);
  CHECK(s1[0].size() == 1 && s1[0][0].reason == Reason::connect_timed_out);
}

TEST(node_fake_conditions_on_a_transport_delay_and_drop_its_sends) {
  SimNetwork net = make_sim_network(3, LinkConditions{});
  auto host = make_end(net, 0, true, 4);
  auto client = make_end(net, 1, false, 1);
  FakeLink fake;
  LinkConditions c;
  c.latency_ms = 80;  // one way, on the client's sends alone
  c.drop_every = 4;   // and one of every four lost
  transport_fake(client->t, &fake, c, 9);
  int64_t now = 0;
  const int peer = node_connect(client->n, sim_address(0, 1), now);
  now = step(net, {host.get(), client.get()}, now, 3000);
  CHECK(peer_connected(client->n, peer));
  // The round trip the client measures carries the fake 80 ms its sends wait (and the ticks).
  const double rtt = client->n.peers[static_cast<size_t>(peer)].conn.stats.rtt_ms;
  CHECK(rtt > 80 && rtt < 130);
  CHECK(fake.lost > 0 && fake.delayed > 0 && fake.overflow == 0);
  // The client's own loss meter (smoothed) sees its sends go missing.
  const double loss = client->n.peers[static_cast<size_t>(peer)].conn.stats.loss;
  CHECK_MSG(loss > 0.05, "loss " + std::to_string(loss));
  transport_fake(client->t, nullptr, c, 0);
  CHECK(client->t.fake == nullptr);
}

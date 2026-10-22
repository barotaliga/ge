// engine/net/sim_network.hpp: delivery in (arrival, send order), unbound addresses counted, the
// clock never goes back, and the same seed is the same deliveries.
//
// ge:summary Test: the simulated network's delivery order, binding and determinism
// ge:keywords test net sim-network deliver order unbound determinism
// ge:skill net testing
#include "net/sim_network.hpp"

#include "check.hpp"

using namespace ge::net;

TEST(sim_network_delivers_in_arrival_order) {
  LinkConditions c;
  c.latency_ms = 10;
  SimNetwork net = make_sim_network(1, c);
  const Address a = sim_address(1, 1), b = sim_address(2, 1);
  CHECK(sim_bind(net, a) && sim_bind(net, b));
  CHECK(!sim_bind(net, a));
  LinkConditions slow;
  slow.latency_ms = 50;
  sim_set_link(net, 2, 1, slow);  // b to a is slower; a to b keeps the defaults
  CHECK(sim_link(net, 1, 2).latency_ms == 10 && sim_link(net, 2, 1).latency_ms == 50);
  const uint8_t m[3] = {7, 8, 9};
  sim_send(net, a, b, m, 1);
  sim_send(net, a, b, m + 1, 1);
  sim_send(net, b, a, m + 2, 1);
  sim_advance(net, 9999);
  Datagram d;
  CHECK(!sim_receive(net, b, d));
  sim_advance(net, 10000);
  CHECK(sim_receive(net, b, d) && d.data[0] == 7 && d.at_us == 10000 && d.from == a);
  CHECK(sim_receive(net, b, d) && d.data[0] == 8);
  CHECK(!sim_receive(net, b, d));
  sim_advance(net, 5);  // never back
  CHECK(net.now_us == 10000);
  sim_advance(net, 50000);
  uint8_t buf[4];
  Address from;
  CHECK(sim_receive_bytes(net, a, buf, sizeof buf, from) == 1 && buf[0] == 9 && from == b);
  CHECK(net.stats.delivered == 3 && net.stats.sent == 3);
}

TEST(sim_network_counts_the_unbound_and_the_oversized) {
  SimNetwork net = make_sim_network(1, {});
  const Address a = sim_address(1, 1);
  sim_bind(net, a);
  uint8_t big[kMaxDatagram + 1] = {};
  CHECK(!sim_send(net, a, sim_address(9, 9), big, sizeof big));
  CHECK(sim_send(net, a, sim_address(9, 9), big, 10));
  sim_advance(net, 1);
  CHECK(net.stats.unbound == 1);
  sim_unbind(net, a);
  CHECK(sim_bind(net, a));
}

TEST(sim_network_is_the_seed_s) {
  const auto run = [](uint32_t seed) {
    LinkConditions c;
    conditions_preset("awful", c);
    SimNetwork net = make_sim_network(seed, c);
    const Address a = sim_address(1, 1), b = sim_address(2, 1);
    sim_bind(net, a);
    sim_bind(net, b);
    uint64_t h = 0xcbf29ce484222325ull;
    Datagram d;
    for (int t = 0; t < 2000; t++) {
      uint8_t m[2] = {static_cast<uint8_t>(t), static_cast<uint8_t>(t >> 8)};
      sim_send(net, a, b, m, sizeof m);
      sim_advance(net, int64_t{t} * 5000);
      while (sim_receive(net, b, d)) {
        h = (h ^ (static_cast<uint64_t>(d.at_us) << 16 | d.data[0] | d.data[1] << 8)) *
            0x100000001b3ull;
      }
    }
    return h;
  };
  CHECK(run(7) == run(7));
  CHECK(run(7) != run(8));
}

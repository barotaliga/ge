// engine/net/sim_network.hpp: delivery in (arrival, send order), unbound addresses counted, the
// clock never goes back, and the same seed is the same deliveries; each NAT kind's mapping and
// filter, a symmetric NAT's port per destination, a mapping's expiry and a port forward.
//
// ge:summary Test: the simulated network's delivery order, binding, determinism and NATs
// ge:keywords test net sim-network deliver order unbound determinism nat full-cone symmetric
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

namespace {

// A behind NAT (public node 10), B and C outside. A sends to B; then B from its own port, B from
// another port and C each send to A's public port: which of the three gets through.
struct NatSeen {
  Address b_saw;  // where A's datagram reached B from
  bool from_b = false, from_b_other_port = false, from_c = false;
  uint64_t dropped = 0;
};

NatSeen nat_seen(NatKind kind) {
  SimNetwork net = make_sim_network(1, {});
  NatConfig nc;
  nc.kind = kind;
  nc.public_node = 10;
  nc.inside = {1};
  sim_add_nat(net, nc);
  const Address a = sim_address(1, 5), b = sim_address(2, 5), b2 = sim_address(2, 6),
                c = sim_address(3, 5);
  sim_bind(net, a);
  sim_bind(net, b);
  sim_bind(net, b2);
  sim_bind(net, c);
  const uint8_t m[1] = {1};
  sim_send(net, a, b, m, 1);
  sim_advance(net, 1000);
  NatSeen seen;
  Datagram d;
  if (sim_receive(net, b, d)) seen.b_saw = d.from;
  const auto reaches = [&](const Address& from, uint8_t tag) {
    const uint8_t x[1] = {tag};
    sim_send(net, from, seen.b_saw, x, 1);
    sim_advance(net, net.now_us + 1000);
    bool got = false;
    while (sim_receive(net, a, d)) got |= d.data[0] == tag;
    return got;
  };
  seen.from_b = reaches(b, 2);
  seen.from_b_other_port = reaches(b2, 3);
  seen.from_c = reaches(c, 4);
  seen.dropped = net.stats.nat_dropped;
  return seen;
}

}  // namespace

TEST(sim_network_nats_map_and_filter_by_kind) {
  const NatSeen full = nat_seen(NatKind::full_cone);
  CHECK(full.b_saw == sim_address(10, 40000));
  CHECK(full.from_b && full.from_b_other_port && full.from_c && full.dropped == 0);
  const NatSeen restricted = nat_seen(NatKind::restricted);
  CHECK(restricted.from_b && restricted.from_b_other_port && !restricted.from_c);
  CHECK(restricted.dropped == 1);
  const NatSeen port = nat_seen(NatKind::port_restricted);
  CHECK(port.from_b && !port.from_b_other_port && !port.from_c && port.dropped == 2);
  const NatSeen symmetric = nat_seen(NatKind::symmetric);
  CHECK(symmetric.from_b && !symmetric.from_b_other_port && !symmetric.from_c);
  NatKind k{};
  CHECK(nat_kind("port-restricted", k) && k == NatKind::port_restricted);
  CHECK(!nat_kind("open", k));
}

TEST(sim_network_symmetric_nat_maps_each_destination_apart) {
  SimNetwork net = make_sim_network(1, {});
  NatConfig nc;
  nc.kind = NatKind::symmetric;
  nc.public_node = 10;
  nc.inside = {1};
  sim_add_nat(net, nc);
  const Address a = sim_address(1, 5), b = sim_address(2, 5), c = sim_address(3, 5);
  sim_bind(net, a);
  sim_bind(net, b);
  sim_bind(net, c);
  const uint8_t m[1] = {1};
  sim_send(net, a, b, m, 1);
  sim_send(net, a, c, m, 1);
  sim_advance(net, 1000);
  Datagram db, dc;
  CHECK(sim_receive(net, b, db) && sim_receive(net, c, dc));
  CHECK(sim_node(db.from) == 10 && sim_node(dc.from) == 10 && db.from.port != dc.from.port);
  CHECK(sim_nat_of(net, 1) == 0 && sim_nat_of(net, 2) == -1);
}

TEST(sim_network_nat_mappings_expire_and_forwards_let_anyone_in) {
  SimNetwork net = make_sim_network(1, {});
  NatConfig nc;
  nc.kind = NatKind::full_cone;
  nc.public_node = 10;
  nc.inside = {1};
  nc.mapping_us = 1000000;
  const int nat = sim_add_nat(net, nc);
  const Address a = sim_address(1, 5), b = sim_address(2, 5);
  sim_bind(net, a);
  sim_bind(net, b);
  const uint8_t m[1] = {1};
  sim_send(net, a, b, m, 1);
  sim_advance(net, 1000);
  Datagram d;
  CHECK(sim_receive(net, b, d));
  const Address mapped = d.from;
  sim_advance(net, 2000000);  // the mapping is idle past its life
  sim_send(net, b, mapped, m, 1);
  sim_advance(net, 2001000);
  CHECK(!sim_receive(net, a, d) && net.stats.nat_dropped == 1);
  // A port forward (what UPnP asks a router for) lets anything in to one inside address.
  CHECK(sim_nat_forward(net, nat, a, 7777, 9000000));
  sim_send(net, b, sim_address(10, 7777), m, 1);
  sim_advance(net, 2002000);
  CHECK(sim_receive(net, a, d) && d.from == b);
}

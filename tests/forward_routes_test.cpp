// The route finder (engine/forward/routes.hpp): the lowest summed ping to a server, straight when
// that is best, through one forwarder or a chain of two, a forwarder beside the server counted as
// that host, hosts matched without their ports, no route when nothing reaches it; and the finder
// asking every forwarder a server list holds over the simulated network, again when one does not
// answer, and building the table from the answers.
//
// ge:summary Tests: the route finder's shortest summed ping, and its asks on a simulated network
// ge:keywords test forward route-finder best_route dijkstra pingstatus chain
// ge:skill net testing
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

#include "check.hpp"
#include "forward/routes.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"

namespace fw = ge::forward;
namespace net = ge::net;

namespace {

net::Address at(uint32_t node, uint16_t port = 27500) { return net::sim_address(node, port); }

}  // namespace

TEST(the_best_route_is_the_lowest_summed_ping) {
  const net::Address server = at(1), f1 = at(2, 30000), f2 = at(3, 30000);
  fw::RouteTable t;
  t.direct = {{server, 180}, {f1, 20}, {f2, 90}};
  t.hops = {{f1, {{server, 140}, {f2, 10}}}, {f2, {{server, 15}}}};
  const fw::Route r = fw::best_route(t, server);
  CHECK(r.found);
  CHECK(r.ping_ms == 45);  // 20 to f1, 10 to f2, 15 to the server
  CHECK(r.direct_ms == 180);
  CHECK(r.forwarders.size() == 2 && r.forwarders[0] == f1 && r.forwarders[1] == f2);
  CHECK(fw::route_chain(r, server) == "sim:2:30000@sim:3:30000@sim:1:27500");
}

TEST(straight_is_kept_when_it_is_best_and_ties_take_fewer_hops) {
  const net::Address server = at(1), f1 = at(2, 30000);
  fw::RouteTable t;
  t.direct = {{server, 40}, {f1, 20}};
  t.hops = {{f1, {{server, 20}}}};
  const fw::Route r = fw::best_route(t, server);
  CHECK(r.found && r.ping_ms == 40 && r.forwarders.empty());
}

TEST(hosts_match_without_ports_and_a_forwarder_beside_the_server_is_that_host) {
  const net::Address server = at(1), beside = at(1, 30000), far = at(4, 30000);
  fw::RouteTable t;
  t.direct = {{far, 30}};
  // The far forwarder names the server on another port, and its neighbour on the server's host.
  t.hops = {{far, {{at(1, 27501), 25}}}, {beside, {{server, 0}}}};
  const fw::Route r = fw::best_route(t, server);
  CHECK(r.found && r.ping_ms == 55);
  CHECK(r.forwarders.size() == 1 && r.forwarders[0] == far);
}

TEST(nothing_reaches_it_no_route) {
  fw::RouteTable t;
  t.direct = {{at(2, 30000), 20}};
  t.hops = {{at(2, 30000), {{at(5), 65535}}}};  // known, but no answer yet
  const fw::Route r = fw::best_route(t, at(5));
  CHECK(!r.found && r.ping_ms == -1 && r.direct_ms == -1);
}

namespace {

// A forwarder's list as these tests write it: "R", then 4 bytes node, 2 port, 2 ping a server.
bool test_is_forwarder(const net::ServerInfo& info) { return info.mode == "forward"; }
size_t test_ask(uint8_t* out, size_t cap) {
  if (cap < 3) return 0;
  std::memcpy(out, "ASK", 3);
  return 3;
}
bool test_read(const uint8_t* data, size_t size, fw::Reach& out) {
  if (size < 1 || data[0] != 'R' || (size - 1) % 8 != 0) return false;
  for (size_t i = 1; i + 8 <= size; i += 8) {
    uint32_t node = 0;
    std::memcpy(&node, data + i, 4);
    const auto port = static_cast<uint16_t>(data[i + 4] | (data[i + 5] << 8));
    out.emplace_back(net::sim_address(node, port), data[i + 6] | (data[i + 7] << 8));
  }
  return true;
}

}  // namespace

TEST(the_finder_asks_every_forwarder_and_builds_the_table) {
  net::LinkConditions lan;
  net::conditions_preset("lan", lan);
  net::SimNetwork sn = net::make_sim_network(3, lan);
  net::Transport browser, hop, deaf;
  net::open_sim_transport(browser, sn, at(9, 27001));
  net::open_sim_transport(hop, sn, at(2, 30000));
  net::open_sim_transport(deaf, sn, at(3, 30000));
  net::ServerList list;
  for (const auto& [a, mode, ping] : std::vector<std::tuple<net::Address, std::string, int>>{
           {at(1), "ffa", 120}, {at(2, 30000), "forward", 10}, {at(3, 30000), "forward", 12}}) {
    net::ServerEntry e;
    e.address = a;
    e.answered = true;
    e.info.mode = mode;
    e.ping_ms = ping;
    list.entries.push_back(e);
  }
  fw::RouteFinder rf;
  rf.timeout_us = 200000;
  rf.gap_us = 10000;
  fw::route_finder_start(rf, {test_is_forwarder, test_ask, test_read}, browser, list, 0);
  CHECK(rf.asked.size() == 2);
  int64_t now = 0;
  for (int i = 0; i < 200 && !fw::route_finder_done(rf, now); i++) {
    now += 10000;
    net::sim_advance(sn, now);
    fw::route_finder_update(rf, now);
    uint8_t buf[64];
    net::Address from;
    int n;
    while ((n = net::transport_receive(hop, buf, sizeof buf, from, now)) >= 0) {
      const uint8_t reply[9] = {'R', 1, 0, 0, 0, 0x6c, 0x6b, 15, 0};  // node 1:27500 at 15 ms
      net::transport_send(hop, from, reply, sizeof reply, now);
    }
    while (net::transport_receive(deaf, buf, sizeof buf, from, now) >= 0) {
    }
  }
  CHECK(fw::route_finder_done(rf, now));
  CHECK(rf.replies == 1);
  CHECK(rf.asks == 4);  // one to the hop, three to the deaf one
  const fw::Route r = fw::best_route(fw::route_finder_table(rf, list), at(1));
  CHECK(r.found && r.ping_ms == 25 && r.direct_ms == 120);
  CHECK(r.forwarders.size() == 1 && r.forwarders[0] == at(2, 30000));
}

// engine/net/udp_socket.hpp and a node over real sockets: two nodes on loopback UDP connect and
// trade a reliable message, the same code the simulated network runs. A machine with no loopback
// socket (a sandbox) skips.
//
// ge:summary Test: UDP sockets on loopback, and two nodes over them
// ge:keywords test net udp socket loopback node real-network
// ge:skill net testing
#include "net/udp_socket.hpp"

#include <string>

#include "check.hpp"
#include "net/node.hpp"

using namespace ge::net;

TEST(udp_loopback_round_trip) {
  UdpSocket a, b;
  std::string err;
  if (!udp_open(a, loopback_v4(0), &err)) {
    ge::test::skip("no loopback UDP socket here: " + err);
    return;
  }
  CHECK_MSG(udp_open(b, loopback_v4(0), &err), err);
  CHECK(a.local.port != 0 && b.local.port != 0 && a.local.port != b.local.port);
  uint8_t buf[64];
  Address from;
  CHECK(udp_receive(a, buf, sizeof buf, from) == -1);  // nothing waiting: at once
  const uint8_t m[3] = {1, 2, 3};
  CHECK(udp_send(a, b.local, m, sizeof m, &err));
  int got = -1;
  for (int i = 0; i < 100000 && got < 0; i++) got = udp_receive(b, buf, sizeof buf, from);
  CHECK(got == 3 && buf[2] == 3 && from == a.local);
  udp_close(a);
  udp_close(b);
  CHECK(a.handle == -1);
  CHECK(os_entropy() != os_entropy());
}

TEST(udp_two_nodes_talk) {
  Transport ht, ct;
  std::string err;
  if (!open_udp_transport(ht, loopback_v4(0), &err) ||
      !open_udp_transport(ct, loopback_v4(0), &err)) {
    ge::test::skip("no loopback UDP socket here: " + err);
    return;
  }
  NodeConfig hc;
  hc.protocol = protocol_id("udp-test", 1);
  hc.accept = true;
  hc.max_peers = 2;
  hc.channels.count = 1;
  hc.channels.delivery[0] = Delivery::reliable;
  hc.seed = os_entropy();
  NodeConfig cc = hc;
  cc.accept = false;
  cc.max_peers = 1;
  cc.seed = os_entropy();
  Node host, client;
  node_init(host, hc, ht);
  node_init(client, cc, ct);
  node_connect(client, ht.local, 0);
  bool sent = false, heard = false;
  // The clock here is a counter, not the wall: each loop is a millisecond to the nodes.
  for (int64_t t = 0; t < 3000000 && !heard; t += 1000) {
    node_update(host, t);
    node_update(client, t);
    for (const Event& e : host.events) {
      if (e.type == EventType::message && e.size == 5)
        heard = std::string(reinterpret_cast<const char*>(event_data(host, e)), 5) == "hello";
    }
    if (!sent && peer_connected(client, 0))
      sent = node_send(client, 0, 0, reinterpret_cast<const uint8_t*>("hello"), 5);
    node_flush(host, t);
    node_flush(client, t);
  }
  CHECK(heard);
  close_transport(ht);
  close_transport(ct);
}

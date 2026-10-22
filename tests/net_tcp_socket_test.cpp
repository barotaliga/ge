// engine/net/tcp_socket.hpp and the TCP transport: frames over a loopback stream come back as
// they went in, whole and in order however the OS cuts the bytes, and two nodes over TCP connect
// and trade a 64 KB message in pieces, the same node code the simulated network runs. A machine
// with no loopback socket (a sandbox) skips.
//
// ge:summary Test: TCP frames on loopback, and two nodes over the TCP transport
// ge:keywords test net tcp socket stream frame loopback node big-message
// ge:skill net testing
#include "net/tcp_socket.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "net/node.hpp"

using namespace ge::net;

TEST(tcp_frames_round_trip_on_loopback) {
  TcpListener l;
  std::string err;
  if (!tcp_listen(l, loopback_v4(0), &err)) {
    ge::test::skip("no loopback TCP socket here: " + err);
    return;
  }
  CHECK(l.local.port != 0);
  TcpStream c, s;
  CHECK_MSG(tcp_connect(c, l.local, &err), err);
  bool accepted = false;
  for (int i = 0; i < 100000 && !accepted; i++) {
    accepted = tcp_accept(l, s);
    tcp_pump(c);
  }
  CHECK(accepted && s.peer.port == c.local.port);
  // Frames of every size up to the largest, the empty one included.
  std::vector<std::vector<uint8_t>> sent;
  for (size_t n : {size_t{0}, size_t{1}, size_t{1200}, size_t{7}, kMaxFrame}) {
    std::vector<uint8_t> f(n);
    for (size_t i = 0; i < n; i++) f[i] = static_cast<uint8_t>(i * 7 + n);
    CHECK(tcp_send_frame(c, f.data(), f.size()));
    sent.push_back(f);
  }
  CHECK(!tcp_send_frame(c, sent[0].data(), kMaxFrame + 1));
  std::vector<uint8_t> buf(kMaxFrame);
  size_t got = 0;
  for (int i = 0; i < 1000000 && got < sent.size(); i++) {
    tcp_pump(c);
    tcp_pump(s);
    const int n = tcp_next_frame(s, buf.data(), buf.size());
    if (n < 0) continue;
    CHECK(static_cast<size_t>(n) == sent[got].size() &&
          std::vector<uint8_t>(buf.begin(), buf.begin() + n) == sent[got]);
    got++;
  }
  CHECK(got == sent.size());
  // The client closes: the server's stream sees it.
  tcp_close(c);
  for (int i = 0; i < 100000 && !s.closed; i++) tcp_pump(s);
  CHECK(s.closed);
  tcp_close(s);
  tcp_close_listener(l);
}

TEST(tcp_two_nodes_trade_a_big_message) {
  Transport ht, ct;
  std::string err;
  if (!open_tcp_host_transport(ht, loopback_v4(0), &err)) {
    ge::test::skip("no loopback TCP socket here: " + err);
    return;
  }
  CHECK_MSG(open_tcp_client_transport(ct, ht.local, &err), err);
  NodeConfig hc;
  hc.protocol = protocol_id("tcp-test", 1);
  hc.accept = true;
  hc.max_peers = 2;
  hc.channels.count = 1;
  hc.channels.delivery[0] = Delivery::reliable;
  hc.channels.largest[0] = kMaxBigMessage;
  hc.seed = os_entropy();
  NodeConfig cc = hc;
  cc.accept = false;
  cc.max_peers = 1;
  cc.seed = os_entropy();
  Node host, client;
  node_init(host, hc, ht);
  node_init(client, cc, ct);
  node_connect(client, ht.local, 0);
  std::vector<uint8_t> big(kMaxBigMessage);
  for (size_t i = 0; i < big.size(); i++) big[i] = static_cast<uint8_t>(i * 13 + 5);
  bool sent = false, heard = false;
  // The clock here is a counter, not the wall: each loop is a millisecond to the nodes.
  for (int64_t t = 0; t < 5000000 && !heard; t += 1000) {
    node_update(host, t);
    node_update(client, t);
    for (const Event& e : host.events) {
      if (e.type == EventType::message && e.size == big.size()) {
        const uint8_t* d = event_data(host, e);
        heard = std::vector<uint8_t>(d, d + e.size) == big;
      }
    }
    if (!sent && peer_connected(client, 0)) sent = node_send(client, 0, 0, big.data(), big.size());
    node_flush(host, t);
    node_flush(client, t);
  }
  CHECK(sent && heard);
  close_transport(ht);
  close_transport(ct);
}

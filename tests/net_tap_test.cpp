// engine/net/tap.hpp: a tap frame reads back to the datagram it mirrors, and a real host's
// transport tapped over loopback mirrors every datagram its handshake and messages cross, in and
// out, to a listening socket.
//
// ge:summary Test: the live tap's frames, and a real UDP host mirrored over loopback
// ge:keywords test net tap live mirror frame loopback udp
// ge:skill net testing
#include "net/tap.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"
#include "net/node.hpp"
#include "net/transport.hpp"

using namespace ge::net;

TEST(tap_frame_reads_back_to_its_datagram) {
  Tap tap;
  std::string error;
  CHECK_MSG(open_tap(tap, 9, &error), error.c_str());  // port 9: nobody listens, sends still go
  const uint8_t data[5] = {1, 2, 3, 4, 5};
  tap_datagram(tap, 1234567, sim_address(3, 7), sim_address(0, 1), data, sizeof data);
  CapturedDatagram d;
  CHECK(read_tap_frame(tap.frame.data(), tap.frame.size(), d));
  CHECK(d.at_us == 1234567);
  CHECK(d.from == sim_address(3, 7) && d.to == sim_address(0, 1));
  CHECK(d.data.size() == 5 && d.data[4] == 5);
  // Not a frame: no mark, or a truncated record.
  CHECK(!read_tap_frame(data, sizeof data, d));
  CHECK(!read_tap_frame(tap.frame.data(), tap.frame.size() - 2, d));
  close_tap(tap);
}

TEST(tap_mirrors_a_real_host_over_loopback) {
  std::string error;
  UdpSocket watcher;
  if (!udp_open(watcher, loopback_v4(0), &error)) {
    ge::test::skip("no loopback UDP here: " + error);
    return;
  }
  Tap tap;
  CHECK_MSG(open_tap(tap, watcher.local.port, &error), error.c_str());
  NodeConfig hc;
  hc.protocol = protocol_id("tap-test", 1);
  hc.accept = true;
  hc.channels.count = 1;
  hc.channels.delivery[0] = Delivery::reliable;
  NodeConfig cc = hc;
  cc.accept = false;
  Transport ht, ct;
  CHECK_MSG(open_udp_transport(ht, loopback_v4(0), &error), error.c_str());
  CHECK_MSG(open_udp_transport(ct, loopback_v4(0), &error), error.c_str());
  ht.tap = &tap;
  Node host, client;
  node_init(host, hc, ht);
  node_init(client, cc, ct);
  int64_t now = 0;
  const int server = node_connect(client, ht.local, now);
  bool sent = false;
  for (int i = 0; i < 200; i++) {
    now += 10000;
    node_update(host, now);
    node_update(client, now);
    if (!sent && peer_connected(client, server)) {
      const uint8_t hello[3] = {'h', 'i', '!'};
      sent = node_send(client, server, 0, hello, sizeof hello);
    }
    node_flush(host, now);
    node_flush(client, now);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(sent);
  CHECK(tap.mirrored > 0 && tap.failed == 0);
  std::vector<CapturedDatagram> seen;
  uint8_t buffer[4096];
  Address from;
  for (int i = 0; i < 100; i++) {
    int n;
    while ((n = udp_receive(watcher, buffer, sizeof buffer, from)) >= 0) {
      CapturedDatagram d;
      CHECK(read_tap_frame(buffer, static_cast<size_t>(n), d));
      seen.push_back(d);
    }
    if (seen.size() >= tap.mirrored) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(seen.size() == tap.mirrored);
  int in = 0, out = 0;
  for (const CapturedDatagram& d : seen) {
    if (d.to == ht.local && d.from == ct.local) in++;
    if (d.from == ht.local && d.to == ct.local) out++;
  }
  CHECK(in > 0 && out > 0 && in + out == static_cast<int>(seen.size()));
  close_transport(ht);
  close_transport(ct);
  close_tap(tap);
  udp_close(watcher);
}

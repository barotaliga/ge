// engine/net/rates.hpp: the transport counts every datagram that leaves and comes, and the rates
// are the last whole window's packets, bytes and the caller's count a second.
//
// ge:summary Test: the transport's counts, the one-second rate windows, dns and net_stats
// ge:keywords test net rates transport counts packets bytes
// ge:skill net testing
#include "net/rates.hpp"

#include <cstdint>

#include "check.hpp"
#include "console/console.hpp"
#include "net/net_commands.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

using namespace ge::net;

TEST(transport_counts_what_leaves_and_comes) {
  SimNetwork net = make_sim_network(1, {});
  Transport a, b;
  CHECK(open_sim_transport(a, net, sim_address(0, 1)));
  CHECK(open_sim_transport(b, net, sim_address(1, 1)));
  const uint8_t data[10] = {};
  CHECK(transport_send(a, sim_address(1, 1), data, sizeof data, 0));
  CHECK(transport_send(a, sim_address(1, 1), data, 4, 0));
  CHECK(a.sent_packets == 2 && a.sent_bytes == 14);
  sim_advance(net, 1000000);
  uint8_t buf[64];
  Address from;
  while (transport_receive(b, buf, sizeof buf, from, 1000000) >= 0) {
  }
  CHECK(b.received_packets == 2 && b.received_bytes == 14);
  CHECK(b.sent_packets == 0 && a.received_packets == 0);
}

TEST(rates_are_the_last_whole_window) {
  Transport t;
  Rates r;
  rates_step(r, 10, t, 0);
  CHECK(r.sent_packets == 0);
  t.sent_packets = 30;
  t.sent_bytes = 3000;
  t.received_packets = 20;
  t.received_bytes = 8000;
  rates_step(r, 10.5, t, 36);  // half a window: nothing yet
  CHECK(r.sent_packets == 0 && r.counted == 0);
  t.sent_packets = 60;
  t.sent_bytes = 6000;
  t.received_packets = 40;
  t.received_bytes = 16000;
  rates_step(r, 12, t, 72);  // two seconds since the start
  CHECK(r.sent_packets == 30 && r.sent_bytes == 3000);
  CHECK(r.received_packets == 20 && r.received_bytes == 8000);
  CHECK(r.counted == 36);
  rates_step(r, 12.5, t, 72);  // the figures hold until the next window closes
  CHECK(r.sent_packets == 30);
  rates_step(r, 13, t, 72);  // a quiet second
  CHECK(r.sent_packets == 0 && r.counted == 0);
}

TEST(rates_begin_again_when_the_counts_go_back) {
  Transport t;
  t.sent_packets = 100;
  Rates r;
  rates_step(r, 0, t, 50);
  t.sent_packets = 5;  // another transport
  rates_step(r, 2, t, 50);
  CHECK(r.sent_packets == 0);
  t.sent_packets = 15;
  rates_step(r, 3, t, 60);
  CHECK(r.sent_packets == 10 && r.counted == 10);
  rates_reset(r);
  CHECK(r.from_s < 0 && r.counted == 0);
}

TEST(dns_and_net_stats_on_the_console) {
  ge::console::Console con{false};
  Transport t;
  const Transport* open = nullptr;
  add_net_commands(con, [&open] { return open; });
  con.execute("dns 127.0.0.1");
  CHECK(con.output().back() == "127.0.0.1 resolved to 127.0.0.1");
  con.execute("dns");
  CHECK(con.output().back() == "Usage: dns <name>");
  con.execute("net_stats");
  CHECK(con.output().back() == "Not connected to anything.");
  SimNetwork net = make_sim_network(1, {});
  CHECK(open_sim_transport(t, net, sim_address(0, 1)));
  const uint8_t data[3] = {};
  transport_send(t, sim_address(1, 1), data, sizeof data, 0);
  open = &t;
  con.execute("net_stats");
  const auto& out = con.output();
  CHECK(out[out.size() - 3] == "Sent:     1 packets, 3 bytes");
  CHECK(out[out.size() - 2] == "Received: 0 packets, 0 bytes");
}

TEST(a_clients_showip_names_its_socket) {
  ge::console::Console con{false};
  SimNetwork net = make_sim_network(1, {});
  Transport t;
  CHECK(open_sim_transport(t, net, sim_address(0, 27901)));
  const Transport* open = &t;
  add_client_showip(con, [&open] { return open; });
  con.execute("showip");
  const std::string& last = con.output().back();
  // The resolver may have no name for a container; either answer is the machine's own.
  CHECK_MSG(last.find("Client UDP socket bound to ") != std::string::npos ||
                last.find("host name") != std::string::npos,
            last);
}

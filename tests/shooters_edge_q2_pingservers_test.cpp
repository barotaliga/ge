// pingservers (games/shooters_edge/net/q2_pingservers.hpp) on the simulated network.
//
// ge:summary Tests pingservers: the address book asked, each answer printed once, the empty skipped
// ge:keywords pingservers test
// ge:skill net testing
#include "shooters_edge/net/q2_pingservers.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace gn = ge::net;
namespace sn = shooters_edge::net;

namespace {
double g_now = 100;
double now() { return g_now; }
}  // namespace

TEST(the_address_book_is_asked_and_each_server_answers_once) {
  gn::SimNetwork net = gn::make_sim_network(1, {});
  gn::Transport server, client;
  CHECK(gn::open_sim_transport(server, net, gn::sim_address(5, 27910)));
  CHECK(gn::open_sim_transport(client, net, gn::sim_address(1, 27901)));
  ge::console::Console con{false};
  for (const char* name : {"adr0", "adr1", "adr2"})
    con.cvars().add({.name = name, .kind = ge::console::CvarKind::text});
  con.cvars().set("adr1", gn::to_string(gn::sim_address(5, 27910)), ge::console::Source::local);
  sn::PingSweep sweep;
  sweep.link = &client;
  sn::add_pingservers_command(con, sweep, now);
  con.execute("pingservers");
  CHECK(sweep.asks.size() == 1);  // adr0 and adr2 empty, no broadcast on the simulated network
  int64_t us = 10000;
  gn::sim_advance(net, us);
  uint8_t buf[256];
  gn::Address from;
  const int n = gn::transport_receive(server, buf, sizeof buf, from, us);
  CHECK(n > 4 && std::string(reinterpret_cast<const char*>(buf) + 4) == "info 34");
  const std::string answer = "\xff\xff\xff\xffinfo\nHome         q2dm1    2/8\n";
  for (int k = 0; k < 2; k++)  // the same answer twice: printed once
    gn::transport_send(server, from, reinterpret_cast<const uint8_t*>(answer.data()), answer.size(),
                       us);
  us += 10000;
  gn::sim_advance(net, us);
  std::vector<std::string> said;
  sn::pingservers_step(sweep, now(), us, said);
  CHECK(said == std::vector<std::string>{"Home         q2dm1    2/8"});
  CHECK(sweep.asks.empty());
}

TEST(passive_waits_for_a_servers_call_and_takes_it) {
  gn::SimNetwork net = gn::make_sim_network(1, {});
  gn::Transport server, client;
  CHECK(gn::open_sim_transport(server, net, gn::sim_address(5, 27910)));
  CHECK(gn::open_sim_transport(client, net, gn::sim_address(1, 27901)));
  ge::console::Console con{false};
  sn::PassiveListen p;
  p.link = &client;
  sn::add_passive_command(con, p);
  con.execute("passive");
  CHECK(p.on);
  CHECK(con.output().back() ==
        "Listening for passive connections at " + gn::to_string(client.local) + ".");
  std::vector<std::string> said;
  CHECK(sn::passive_step(p, false, 0, said).empty() && p.on);
  const std::string call = "\xff\xff\xff\xffpassive_connect\n";
  gn::transport_send(server, client.local, reinterpret_cast<const uint8_t*>(call.data()),
                     call.size(), 0);
  gn::sim_advance(net, 10000);
  const std::string to = sn::passive_step(p, false, 10000, said);
  CHECK(to == gn::to_string(server.local));
  CHECK(said.back() == "Received passive connect from " + to + ".");
  CHECK(!p.on);
  con.execute("passive");
  con.execute("passive");
  CHECK(con.output().back() == "No longer listening for passive connections.");
}

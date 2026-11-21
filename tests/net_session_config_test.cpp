// Configs over a session (engine/net/session_config.hpp), held to M21c's promises: a client
// follows the server's settings from its info and cannot set them itself; the server reads a
// client's user info; stuffed text runs only what a server may stuff (a `quit` is refused); the
// fake link's cheats move only while the server's sv_cheats is 1, and go back when it is off.
//
// ge:summary Tests: server info followed, user info read, stuffed quit refused, fake link cheats
// ge:keywords test net session config server-info user-info stuffed quit sv_cheats net_fakeloss
// ge:skill net testing
#include <memory>
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "net/net_cvars.hpp"
#include "net/node.hpp"
#include "net/session.hpp"
#include "net/session_config.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace gn = ge::net;
namespace con = ge::console;

namespace {

gn::NodeConfig node_config(bool host, uint64_t seed) {
  gn::NodeConfig c;
  c.protocol = gn::protocol_id("session-config-test", 1);
  c.accept = host;
  c.max_peers = host ? 2 : 1;
  c.channels.count = 1;
  c.channels.delivery[0] = gn::Delivery::reliable;
  c.timeout_us = 2000000;
  c.seed = seed;
  return c;
}

// One host and one client on a clean link, each with a console and the network's cvars.
struct World {
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  gn::Transport host_t, client_t;
  std::unique_ptr<gn::Node> host_n = std::make_unique<gn::Node>();
  std::unique_ptr<gn::Node> client_n = std::make_unique<gn::Node>();
  std::unique_ptr<gn::SessionHost> host = std::make_unique<gn::SessionHost>();
  gn::SessionClient client;
  con::Console server{true};
  con::Console player{false};
  gn::ReplicateConfig server_net, player_net;
  gn::NetCvars server_cvars{&server_net}, player_cvars{&player_net};
  gn::FakeLinkCvars fake, applied;
  gn::FakeLink link;
  gn::ClientConfig config;
  int quits = 0, hellos = 0;
  int64_t now = 0;

  World() {
    gn::open_sim_transport(host_t, net, gn::sim_address(0, 1));
    gn::open_sim_transport(client_t, net, gn::sim_address(1, 1));
    gn::node_init(*host_n, node_config(true, 7), host_t);
    gn::node_init(*client_n, node_config(false, 8), client_t);
    gn::SessionConfig sc;
    sc.max_players = 2;
    gn::session_host_init(*host, sc, *host_n, 0);
    gn::session_client_init(client, *client_n, 0);
    gn::add_net_cvars(server.cvars(), server_cvars);
    gn::add_net_cvars(player.cvars(), player_cvars);
    gn::add_fake_link_cvars(player.cvars(), fake);
    player.add_command("quit", "", [this](con::Console&, const con::Args&) { quits++; });
    player.add_command(
        "hello", "", [this](con::Console&, const con::Args&) { hellos++; }, con::kStuffable);
    gn::session_client_join(client, gn::sim_address(0, 1), "player", 0, now);
  }

  void frames(int n) {
    for (int i = 0; i < n; i++) {
      now += 10000;
      gn::sim_advance(net, now);
      gn::node_update(*host_n, now);
      gn::session_host_update(*host, now);
      gn::session_host_follow_cvars(*host, server.cvars());
      gn::node_update(*client_n, now);
      gn::session_client_update(client, now);
      gn::session_client_follow_cvars(client, player, config);
      gn::fake_link_follow(client_t, link, fake, applied, 9);
      gn::node_flush(*host_n, now);
      gn::node_flush(*client_n, now);
    }
  }
  std::string value(con::Console& c, const char* name) {
    const con::Cvar* v = c.cvars().find(name);
    return v != nullptr ? v->value : std::string();
  }
};

}  // namespace

TEST(a_client_follows_the_servers_settings_and_cannot_set_them_itself) {
  World w;
  w.server.execute("sv_maxrate 25000");  // the server's cfg, before the client is seated
  w.frames(50);
  CHECK(w.client.state == gn::ClientState::seated);
  CHECK(w.value(w.player, "sv_maxrate") == "25000");  // from the welcome's info
  w.server.execute("sv_maxrate 40000");
  w.frames(5);
  CHECK(w.value(w.player, "sv_maxrate") == "40000");
  w.player.execute("sv_maxrate 1000");  // the player's own try: refused, the server's stands
  CHECK(w.value(w.player, "sv_maxrate") == "40000");
  CHECK(w.player_net.max_rate == 40000);  // the bound field moved with it
}

TEST(the_server_reads_a_clients_user_info) {
  World w;
  w.frames(50);
  w.player.execute("rate 15000");
  w.frames(5);
  CHECK(gn::session_user_value(*w.host, w.client.me, "rate") == "15000");
  CHECK(gn::session_user_value(*w.host, w.client.me, "nothing").empty());
}

TEST(stuffed_text_runs_only_what_a_server_may_stuff) {
  World w;
  w.frames(50);
  const std::string text = "quit\nrate 1\nhello\n";
  CHECK(gn::session_host_stuff(*w.host, w.client.me, text.data(), text.size()));
  w.frames(5);
  CHECK(w.quits == 0);
  CHECK(w.hellos == 1);
  CHECK(w.value(w.player, "rate") != "1");
  CHECK(w.config.stuffed == 1 && w.config.refused == 2);
}

TEST(the_fake_link_moves_only_while_the_server_allows_cheats) {
  World w;
  w.frames(50);
  w.player.execute("net_fakeloss 30");
  w.frames(2);
  CHECK(w.fake.loss_pct == 0);
  CHECK(w.client_t.fake == nullptr);
  w.server.execute("sv_cheats 1");
  w.frames(5);
  w.player.execute("net_fakeloss 30");
  w.player.execute("net_fakelag 50");
  w.frames(2);
  CHECK(w.fake.loss_pct == 30 && w.fake.lag_ms == 50);
  CHECK(w.client_t.fake == &w.link);
  CHECK(w.link.conditions.loss == 0.3 && w.link.conditions.latency_ms == 50);
  w.server.execute("sv_cheats 0");
  w.frames(30);  // the fake lag holds the client's packets back
  CHECK(w.fake.loss_pct == 0 && w.fake.lag_ms == 0);
  CHECK(w.client_t.fake == nullptr);
}

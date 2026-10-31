// engine/server: the server core. The sample game served dedicated, listen and over the simulated
// network gives one digest; a LAN browser sees the server by its name with its seats rising as
// players join; every player is seated; the networked run replays to the bit under a bad network.
// The settings: server.cfg, then the flags, then +commands, a cfg's mistake named at its line, a
// flag unknown or out of range refused. Shooter's Edge's ServerGame runs its host frame for frame
// as the host driven by hand.
//
// ge:summary Test: the server core: one digest three ways, the LAN browser, settings, a real game
// ge:keywords test server dedicated-server listen-server server-cfg flags lan-browser digest
// ge:skill net testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "server/main.hpp"
#include "server/sample.hpp"
#include "server/trial.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/server/dedicated.hpp"
#include "shooters_edge/server/host.hpp"
#include "shooters_edge/server/stand_in.hpp"

using namespace ge::server;

namespace {

TrialConfig sample_trial() {
  TrialConfig t;
  t.server.hostname = "Ring Night";
  t.server.bots = 4;
  t.server.maxclients = 8;
  t.server.seconds = 8;
  t.clients = 3;
  return t;
}

const ServerTrial& lan_trial() {
  static const ServerTrial t = run_server_trial(sample_trial());
  return t;
}

// A console whose only file is `server.cfg` with `text`.
void cfg_files(ge::console::Console& con, const std::string& text) {
  con.set_files(
      [text](std::string_view name, std::string& out) {
        if (name != "server.cfg") return false;
        out = text;
        return true;
      },
      [](std::string_view, std::string_view) { return false; });
}

}  // namespace

TEST(server_one_digest_dedicated_listen_and_networked) {
  const ServerTrial& t = lan_trial();
  CHECK(t.error.empty());
  CHECK(t.ok);
  CHECK(t.frames == 480);
  CHECK(t.dedicated == t.listen && t.listen == t.networked);
  CHECK(t.listen_client_frames == 1152);  // 8 s at 144 Hz
}

TEST(server_lan_browser_sees_it_by_name_with_its_seats) {
  const ServerTrial& t = lan_trial();
  CHECK(t.sightings.size() >= 6);
  CHECK(t.lan_answers >= 6);
  for (const TrialSighting& s : t.sightings) {
    CHECK(s.name == "Ring Night");
    CHECK(s.max_players == 8);
    CHECK(!s.locked);
  }
  // The four bots from the start, then a player a second.
  CHECK(t.sightings.front().seated == 4);
  CHECK(t.sightings.back().seated == 7);
  for (size_t i = 1; i < t.sightings.size(); i++)
    CHECK(t.sightings[i].seated >= t.sightings[i - 1].seated);
}

TEST(server_every_player_is_seated) {
  const ServerTrial& t = lan_trial();
  CHECK(t.clients_seated == 3);
  CHECK(t.seated_at_s.size() == 3);
  for (double s : t.seated_at_s) CHECK(s >= 0 && s < 0.5);
  CHECK(t.status.find("seated=4 clients=3") != std::string::npos);
  CHECK(t.banner.find("\"Ring Night\"") != std::string::npos);
}

TEST(server_networked_run_replays_under_a_bad_network) {
  TrialConfig c = sample_trial();
  c.server.password = "secret";
  CHECK(ge::net::conditions_preset("bad", c.link));
  const ServerTrial a = run_server_trial(c);
  const ServerTrial b = run_server_trial(c);
  CHECK(a.ok);
  CHECK(a.digest == b.digest);
  CHECK(a.sightings.size() == b.sightings.size());
  CHECK(!a.sightings.empty() && a.sightings[0].locked);
  CHECK(a.digest != lan_trial().digest);
}

TEST(server_settings_cfg_then_flags_then_commands) {
  ge::console::Console con(true);
  cfg_files(con, "sv_hostname \"From the cfg\"\nsv_maxclients 12\nsv_description \"cfg\"\n");
  ServerConfig c;
  std::string error;
  CHECK(server_configure(con,
                         {"--name=From the flag", "--port=27999", "--for=3", "+set",
                          "sv_description", "from a command"},
                         c, sample_game(), error));
  CHECK(error.empty());
  CHECK(c.hostname == "From the flag");  // the flag over the cfg
  CHECK(c.maxclients == 12);             // the cfg over the fallback
  CHECK(c.description == "from a command");
  CHECK(c.port == 27999);
  CHECK(c.map == "ring");  // the game's default
  CHECK(c.seconds == 3);
}

TEST(server_settings_refused_with_where_and_next) {
  {
    ge::console::Console con(true);
    cfg_files(con, "sv_hostname ok\nnot_a_command 1\n");
    ServerConfig c;
    std::string error;
    CHECK(!server_configure(con, {}, c, sample_game(), error));
    CHECK(error.find("server.cfg:2") != std::string::npos);
  }
  {
    ge::console::Console con(true);
    cfg_files(con, "");
    ServerConfig c;
    std::string error;
    CHECK(!server_configure(con, {"--colour=red"}, c, sample_game(), error));
    CHECK(error.find("unknown flag '--colour=red'") != std::string::npos);
    CHECK(error.find("--help") != std::string::npos);
  }
  {
    ServerConfig c;
    c.map = "ring";
    c.bots = 9;
    c.maxclients = 8;
    Server s;
    std::string error;
    CHECK(!server_open(s, sample_game(), c, nullptr, nullptr, error));
    CHECK(error.find("raise sv_maxclients") != std::string::npos);
  }
}

TEST(server_help_names_every_flag) {
  const std::string help = server_help(sample_game());
  for (const char* f : {"--name", "--description", "--ip", "--port", "--maxclients", "--map",
                        "--bots", "--for", "--realtime", "--offline", "--replay", "--dir"})
    CHECK(help.find(f) != std::string::npos);
}

// Shooter's Edge through the core is its host, frame for frame.
TEST(server_shooters_edge_runs_its_host_frame_for_frame) {
  namespace se = shooters_edge::server;
  ServerConfig c;
  c.map = "arena";
  c.bots = 4;
  c.maxclients = 8;
  c.seed = 3;
  Server s;
  std::string error;
  CHECK(server_open(s, se::dedicated_game(), c, nullptr, nullptr, error));
  CHECK(error.empty());

  se::Host h;
  se::HostSettings hs;
  hs.maxclients = 8;
  hs.max_edicts = 64 + 16 * 8;
  hs.seed = 3;
  hs.map = "arena";
  hs.record = false;
  CHECK(se::host_create(h, hs, error));
  std::vector<se::StandIn> stand_ins(4);
  for (int b = 0; b < 4; b++) {
    stand_ins[static_cast<size_t>(b)] = se::make_stand_in(3, b);
    se::host_add_bot(h, se::stand_in_source(stand_ins[static_cast<size_t>(b)]));
  }
  for (int n = 0; n < 300; n++) {
    server_frame(s);
    se::host_frame(h, nullptr, 0);
    if (server_digest(s) != ge::shooter::server_hash(h.sv)) {
      CHECK(false);
      break;
    }
  }
  CHECK(server_status(s).find("seated=4") != std::string::npos);
}

// A TEAM OF BOTS ON A SERVER IT CANNOT READ (games/shooters_edge/net/classic_bot_team.hpp): four
// bots join our classic server over the simulated network as a server somewhere else (the team
// reads nothing but what each client is sent and the map file it loads), set up on the map, say
// `ready`, play and talk. The console takes a line for one bot (`bot2 skill novice`: that bot set
// up again as a novice, the others untouched), refuses a bot that is not there with the next step,
// and answers `report` with every bot's line. The team's picture (classic_team_view.hpp) is drawn
// at the asked size with each bot in its colour.
//
// ge:summary Tests: a bot team joins a server, sets up, readies, plays; the console; the team view
// ge:keywords test bot-team live-server console per-bot-settings ready team-view picture
// ge:skill bots net testing
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "shooters_edge/net/classic_team_view.hpp"
#include "shooters_edge/net/classic_wire.hpp"
#include "shooters_edge/server/bsp_map.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace gn = ge::net;

TEST(a_bot_team_joins_a_server_sets_up_readies_and_takes_console_lines) {
  // The fixture halls furnished for a team game, as a map file the bots load too.
  ge::brush::Fixture fx = ge::brush::sample_fixture();
  std::vector<ge::progs::EntityBlock> blocks;
  std::string error;
  CHECK(ge::progs::parse_entity_text(cn::kTeamHallsEntities, blocks, error));
  for (const ge::progs::EntityBlock& b : blocks) {
    ge::brush::Entity e;
    e.pairs = b.pairs;
    if (e.get("classname") != "worldspawn") fx.entities.push_back(e);
  }
  std::vector<uint8_t> bsp;
  CHECK(ge::brush::write_bsp(fx, ge::brush::Family::v29, &bsp, &error));
  ge::progs::Progs progs;
  CHECK(cl::baseline_progs(progs, error));
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(bsp, "halls", error);
  CHECK(map != nullptr);
  if (!map) return;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  sv::RulesRunner rules = sv::qc_runner(sv::make_qc_server(progs, so, error));
  rules.console("deathmatch 1");
  rules.console("teamplay 1");
  CHECK(rules.start(map->spec, error));
  sv::use_map_sets(*sv::qc_of(rules), map->bsp);

  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport st;
  gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
  auto server = std::make_unique<cn::ClassicServer>();
  cn::ServerConfig scfg;
  CHECK(cn::classic_start(*server, rules, st, scfg, error));

  cn::BotTeamConfig bc;
  bc.bots = 4;
  bc.team = "blue";
  bc.colour = 13;
  bc.address = gn::sim_address(0, cn::kPortServer);
  bc.open = [&net](gn::Transport& t, int k, std::string&) {
    gn::open_sim_transport(t, net, gn::sim_address(static_cast<uint32_t>(k + 1), cn::kPortClient));
    return true;
  };
  bc.read_file = [&bsp](const std::string&, const std::string& path) {
    return path == "maps/halls.bsp" ? bsp : std::vector<uint8_t>{};
  };
  cn::BotTeam team;
  CHECK(cn::bot_team_start(team, bc, error));
  int64_t now = 0;
  const auto run = [&](int frames) {
    for (int i = 0; i < frames; i++) {
      now += 13000;
      gn::sim_advance(net, now);
      cn::classic_frame(*server, now);
      cn::bot_team_tick(team, now);
    }
  };
  run(2300);  // half a minute
  CHECK(team.nav != nullptr);
  for (const auto& s : team.seats) {
    CHECK(s->set_up);
    CHECK(s->said_ready);
    CHECK(s->commands > 1000);
  }
  bool ready_line = false;
  for (const std::string& l : team.log) ready_line |= l.find(": ready") != std::string::npos;
  CHECK(ready_line);

  // One bot's setting: that bot alone set up again.
  cn::bot_team_command(team, "bot2 skill novice");
  CHECK(team.seats[1]->skill == ge::shooter::AimSkill::novice && team.seats[1]->reset);
  CHECK(team.seats[0]->skill == ge::shooter::AimSkill::expert && !team.seats[0]->reset);
  run(10);
  CHECK(team.seats[1]->set_up && !team.seats[1]->reset);
  const size_t before = team.log.size();
  cn::bot_team_command(team, "bot9 skill expert");
  CHECK(team.log.size() == before + 1 && team.log.back().find("no bot9") != std::string::npos);
  cn::bot_team_command(team, "report");
  const std::string report = team.log.back();
  for (const auto& s : team.seats) CHECK(report.find(s->name) != std::string::npos);

  // The picture: the asked size, the first bot's colour on it.
  cn::TeamView view;
  const ge::lab::Picture p = cn::team_view(view, team, 960, 540);
  CHECK(p.width == 960 && p.height == 540);
  int64_t first = 0;
  for (size_t i = 0; i + 3 < p.rgba.size(); i += 4)
    first += p.rgba[i] == 80 && p.rgba[i + 1] == 200 && p.rgba[i + 2] == 255 ? 1 : 0;
  CHECK(first > 20);
  CHECK(cn::bot_team_stop(team, error));
}

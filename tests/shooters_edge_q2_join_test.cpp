// Tests: the window joining a Quake II server of the stock protocol (34) while its own map still
// loads behind the title (games/shooters_edge/client/load_line.hpp): the console that load holds
// down lets go once the game is joined, so the joined world is seen; it stays held while the
// window is on its own, and a game's start keeps the console down. A server's version check is
// answered with the client's version.
//
// ge:summary Test: a stock-protocol join while the window's map loads lets the console go
// ge:keywords test q2 protocol-34 stock-join load-line console-hold join-while-loading
// ge:keywords version-check
// ge:skill net shooter testing
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/load_line.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall with two deathmatch spawns.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "180"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  cn::Q2NetServer server;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig() {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset("lan", link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, {}, error), error);
  return r;
}

}  // namespace

// A stock server's protocol (34, as a stock client asks) joined from the window while its own
// map still loads behind the title: the console that load held down lets go once the game is
// joined, so the joined world is seen; the hold stays while the window is on its own.
TEST(the_window_joining_a_stock_protocol_server_lets_its_own_load_s_console_go) {
  auto r = rig();
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  ge::console::Console con;
  ge::console::ConsoleView view(con);
  cl::LoadLine line;
  cl::hold_load_line("base1", 0, line, con, view, 0.3);
  CHECK(view.full() && line.held);
  cl::release_load_line(true, false, line, view, false);  // still loading, nothing joined
  CHECK(view.full() && line.held);
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  remote.read_file = [](const std::string&, const std::string&) { return std::vector<uint8_t>{1}; };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cfg.q2_protocol = 34;
  cn::remote_start(remote, t, gn::sim_address(0, cn::kPortQ2), cfg);
  std::vector<std::string> said;
  int levels = 0;
  for (int i = 0; i < 150; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    levels += cn::remote_step(remote, 0.0131, said) == cn::RemoteStep::new_level ? 1 : 0;
    cn::q2_net_frame(r->server, r->now);
    cl::release_load_line(true, remote.active, line, view, false);  // as the window's frame
  }
  CHECK_MSG(remote.q2->stage == cn::ClientStage::active, remote.q2->refused);
  CHECK(remote.q2->protocol == 34);
  CHECK(levels == 1);
  CHECK(!view.full() && !line.held);
  // Kept by a game's start, the hold is let go but the console stays down.
  cl::hold_load_line("base1", 0, line, con, view, 1);
  cl::release_load_line(true, true, line, view, true);
  CHECK(view.full() && !line.held);
}

// A server's version check (`$version` in its stuffed text) is answered with the client's own
// version, never with nothing: `version` is the window's command, not a setting.
TEST(a_server_s_version_check_is_answered_with_the_client_s_version) {
  auto r = rig();
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(1, 27901));
  cn::Q2ClientEnd c;
  cn::Q2ClientConfig cfg;
  cfg.name = "eve";
  cfg.protocol = 34;
  cn::q2_client_start(c, t, gn::sim_address(0, cn::kPortQ2), cfg, 0);
  for (int i = 0; i < 120; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    cn::q2_client_tick(c, r->now);
    cn::q2_net_frame(r->server, r->now);
  }
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(r->server.clients[0].version_heard);
  CHECK_MSG(r->server.clients[0].version == cn::kProgramVersion,
            "the version said: \"" + r->server.clients[0].version + "\"");
}

// Quake II's dedicated server seats `sv_bots` (bots/bot_seats.hpp, the bots every classic game's
// server and lobby share): the bots take their slots with no connection, a client joining gets
// another, the status lists them as bots, they fight, and the match's end names them in its
// stats.
//
// ge:summary Tests Quake II's server's bots: seated, listed, fighting, named in the end stats
// ge:keywords q2 sv_bots bot-seats dedicated-bots q2-bots-server
// ge:skill bots shooter net
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "net/sim_network.hpp"
#include "net/status.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/q2_net.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = ge::server;
namespace cn = shooters_edge::net;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall with four deathmatch spots, written as <root>/baseq2/maps/hall.bsp; the root.
std::string hall_folder() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-512, -512, -16, 512, 512, 0));
  m.push_back(box(-528, -512, -16, -512, 512, 256));
  m.push_back(box(512, -512, -16, 528, 512, 256));
  m.push_back(box(-528, 512, -16, 528, 528, 256));
  m.push_back(box(-528, -528, -16, 528, -512, 256));
  w.entities.push_back(entity({{"classname", "worldspawn"}}));
  for (const char* at : {"-300 -300 24", "300 300 24", "-300 300 24", "300 -300 24"})
    w.entities.push_back(
        entity({{"classname", "info_player_deathmatch"}, {"origin", at}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "ge_q2_bots_server_test";
  std::filesystem::create_directories(root / "baseq2" / "maps");
  std::ofstream(root / "baseq2" / "maps" / "hall.bsp", std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return root.string();
}

sh::MoveCmd stand(int) { return {}; }

}  // namespace

TEST(a_q2_server_seats_its_bots_and_they_fight_a_match) {
  sv::ServerConfig c;
  c.root = hall_folder();
  c.map = "hall";
  c.maxclients = 4;
  c.bots = 2;
  c.seed = 5;
  c.rules["timelimit"] = "0.25";  // 15 s
  gn::SimNetwork net = gn::make_sim_network(3, gn::LinkConditions{});
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortQ2));
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, cn::q2_game(), c, &port, nullptr, error), error);
  if (!s.state) return;
  gn::Transport mine;
  gn::open_sim_transport(mine, net, gn::sim_address(1, 27901));
  cn::Q2ClientEnd ana;
  cn::Q2ClientConfig cc;
  cc.name = "ana";
  cc.brain = stand;
  cc.qport = 101;
  cn::q2_client_start(ana, mine, gn::sim_address(0, cn::kPortQ2), cc,
                      static_cast<double>(s.now_us) / 1e6);
  const auto step = [&] {
    gn::sim_advance(net, s.now_us);
    cn::q2_client_tick(ana, s.now_us);
    sv::server_frame(s);
  };
  for (int f = 0; f < 200; f++) step();  // she joins
  gn::Status st;
  s.game->fill_status(s.state, st);
  int bots = 0, people = 0;
  for (const gn::StatusPlayer& p : st.players) {
    if ((p.flags & gn::kPlayerBot) != 0) {
      bots++;
      CHECK_MSG(p.name == "bot1" || p.name == "bot2", p.name);
    } else {
      people++;
      CHECK_MSG(p.name == "ana", p.name);
    }
  }
  CHECK_MSG(bots == 2 && people == 1, std::to_string(bots) + " " + std::to_string(people));
  for (int f = 0; f < 1600; f++) step();  // past the time limit: the intermission
  const auto heard = [&](const std::string& text) {
    return std::any_of(ana.prints.begin(), ana.prints.end(),
                       [&](const std::string& p) { return p.find(text) != std::string::npos; });
  };
  CHECK(heard("Player statistics:"));
  CHECK(heard(" bot1: ") && heard(" bot2: ") && heard(" ana: "));
  // Each bot fought (an accuracy line); she stood still and fired nothing.
  int weapons = 0;
  for (const std::string& p : ana.prints)
    for (size_t at = p.find("  Weapons:"); at != std::string::npos;
         at = p.find("  Weapons:", at + 1))
      weapons++;
  CHECK_MSG(weapons == 2, std::to_string(weapons));
  sv::server_close(s);
}

TEST(a_q2_server_says_the_stats_when_the_time_runs_out_with_one_player) {
  sv::ServerConfig c;
  c.root = hall_folder();
  c.map = "hall";
  c.maxclients = 4;
  c.rules["timelimit"] = "0.1";  // 6 s
  gn::SimNetwork net = gn::make_sim_network(3, gn::LinkConditions{});
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortQ2));
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, cn::q2_game(), c, &port, nullptr, error), error);
  if (!s.state) return;
  gn::Transport mine;
  gn::open_sim_transport(mine, net, gn::sim_address(1, 27901));
  cn::Q2ClientEnd ana;
  cn::Q2ClientConfig cc;
  cc.name = "ana";
  cc.brain = stand;
  cc.qport = 101;
  cn::q2_client_start(ana, mine, gn::sim_address(0, cn::kPortQ2), cc,
                      static_cast<double>(s.now_us) / 1e6);
  for (int f = 0; f < 800; f++) {
    gn::sim_advance(net, s.now_us);
    cn::q2_client_tick(ana, s.now_us);
    sv::server_frame(s);
  }
  std::string all;
  for (const std::string& p : ana.prints) all += p;
  CHECK_MSG(all.find("Timelimit hit.") != std::string::npos, all);
  CHECK_MSG(all.find("Player statistics:") != std::string::npos, all);
  CHECK_MSG(all.find(" ana: ") != std::string::npos, all);
  sv::server_close(s);
}

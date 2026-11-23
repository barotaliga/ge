// Quake II's live tests' rig: our Quake II server on a hall of brushes on the simulated network,
// and as many scripted clients as a test joins, stepped together a tick at a time.
//
// ge:summary Tests' rig: Quake II's server on a brush hall, scripted clients, the simulated net
// ge:keywords test fixture q2 rig hall simulated-network q2-client q2-server
// ge:skill net shooter testing
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace q2_rig {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

inline br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

inline br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

inline std::shared_ptr<const br::Map> hall() {
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

// Our server on the simulated network, and as many clients as asked.
struct Rig {
  std::shared_ptr<const br::Map> map;  // the level's brushes
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::Q2NetServer server;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  int64_t now = 0;
};

inline std::unique_ptr<Rig> rig(const cn::Q2NetConfig& cfg, const char* conditions = "lan",
                                const std::shared_ptr<const br::Map>& brushes = nullptr,
                                int fps = 10) {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  r->rules.console("sv_fps " + std::to_string(fps));
  sv::MapSpec map;
  map.name = "hall";
  r->map = brushes != nullptr ? brushes : hall();
  map.brushes = r->map;
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset(conditions, link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

// A client from host `host` (0: a host of its own).
inline cn::Q2ClientEnd& join(Rig& r, cn::Q2ClientConfig cfg, uint32_t host = 0) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net,
                         gn::sim_address(host != 0 ? host : uint32_t(n + 1), uint16_t(27901 + n)));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::q2_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortQ2), cfg,
                      static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

inline void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::q2_client_tick(*c, r.now);
    cn::q2_net_frame(r.server, r.now);
  }
}

}  // namespace q2_rig

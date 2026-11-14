// Tests: the dedicated coop server (net/classic_coop_game.hpp): with `sv_coop 1` the server runs
// our campaign in coop with its companions, a client who connects enters it on a free coop start,
// a browser's status query lists it as coop, and the server's table hands every call to it.
//
// ge:summary Tests: the dedicated coop server: companions, a client joining, listed as coop
// ge:keywords test coop co-op dedicated-coop sv_coop sv_coopbots status server-browser
// ge:skill net shooter testing
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"
#include "progs/qc_compile.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/net/classic_coop_game.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_wire.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace se = shooters_edge::net;
namespace gn = ge::net;
namespace gs = ge::server;

constexpr uint16_t kPort = 27500, kClientPort = 27001, kBrowserPort = 27002;

// Our campaign mod, compiled from its sources.
const ge::progs::Progs& campaign_mod() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs out;
    std::string error;
    CHECK_MSG(shooters_edge::server::campaign_mod(out, error), error);
    return out;
  }();
  return p;
}

// The arena as a coop map: a start and three coop starts.
std::unique_ptr<cl::LoadedMap> coop_map(const std::string& name) {
  auto m = std::make_unique<cl::LoadedMap>();
  m->name = name;
  m->spec.name = name;
  m->spec.models = sv::arena_models(true);
  m->spec.entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "info_player_coop"
"origin" "-416 96 24"
}
{
"classname" "info_player_coop"
"origin" "-416 -96 24"
}
{
"classname" "info_player_coop"
"origin" "-320 0 24"
}
)";
  return m;
}

gs::ServerConfig coop_config(int bots) {
  gs::ServerConfig c;
  c.hostname = "coop test";
  c.map = "start";
  c.seed = 7;
  c.rules["sv_coop"] = "1";
  c.rules["sv_coopbots"] = std::to_string(bots);
  c.rules["skill"] = "2";
  return c;
}

// The coop server on a simulated network, a visitor and a browser's raw socket.
struct Rig {
  std::string error;
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  gn::Transport port, client_port, browser;
  std::unique_ptr<se::CoopGame> g;
  se::ClassicClientEnd visitor;
  int64_t now = 0;
  explicit Rig(int bots) {
    gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
    gn::open_sim_transport(client_port, net, gn::sim_address(1, kClientPort));
    gn::open_sim_transport(browser, net, gn::sim_address(2, kBrowserPort));
    g = se::coop_open(coop_config(bots), error, &campaign_mod(), &coop_map);
    CHECK_MSG(g != nullptr, error);
    if (!g) return;
    CHECK_MSG(se::coop_serve(*g, port, error), error);
    se::ClientConfig cc;
    cc.name = "visitor";
    se::client_start(visitor, client_port, gn::sim_address(0, kPort), cc, 0);
  }
  void run(int n) {
    for (int f = 0; f < n && g; f++) {
      now += static_cast<int64_t>(sv::kCampaignStep * 1e6);
      gn::sim_advance(net, now);
      se::client_tick(visitor, now);
      se::coop_net_frame(*g, now);
    }
  }
  // The server's answer to a browser's status query.
  std::string status() {
    const std::string d = "\xff\xff\xff\xffstatus 23\n";
    gn::transport_send(browser, gn::sim_address(0, kPort),
                       reinterpret_cast<const uint8_t*>(d.data()), d.size(), now);
    run(10);
    uint8_t buf[2048];
    gn::Address from;
    std::string out;
    int n;
    while ((n = gn::transport_receive(browser, buf, sizeof buf, from, now)) >= 0)
      out.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
    return out;
  }
};

}  // namespace

TEST(sv_coop_asks_for_the_coop_campaign) {
  CHECK(se::coop_asked(coop_config(0)));
  gs::ServerConfig match;
  CHECK(!se::coop_asked(match));
  match.rules["sv_coop"] = "0";
  CHECK(!se::coop_asked(match));
}

TEST(the_coop_server_runs_the_campaign_with_its_companions_before_anyone_joins) {
  std::string error;
  std::unique_ptr<se::CoopGame> g =
      se::coop_open(coop_config(2), error, &campaign_mod(), &coop_map);
  CHECK_MSG(g != nullptr, error);
  if (!g) return;
  for (int f = 0; f < 3 * 72; f++) se::coop_frame(*g);
  const sv::RulesCommon& s = *g->campaign.server;
  // The companions take slots 1 and 2: slot 0 waits for the first player to connect.
  CHECK(!s.clients[0].active && s.clients[1].active && s.clients[2].active);
  CHECK(g->campaign.players[1].active && g->campaign.players[2].active);
  CHECK(s.cvars.find("coop")->on() && s.cvars.find("skill")->value == "2");
  CHECK(se::coop_seated(*g) == 2);
  const std::string line = se::coop_status(*g);
  CHECK_MSG(line.find("map=start coop=1 skill=2 bots=2") == 0, line);
  CHECK(sv::qc_of(g->campaign.server)->q.faults == 0);
}

TEST(a_client_joins_the_dedicated_coop_game_on_a_free_coop_start) {
  Rig r(2);
  if (!r.g) return;
  for (int f = 0; f < 400 && r.visitor.stage != se::ClientStage::active; f++) r.run(1);
  CHECK_MSG(r.visitor.stage == se::ClientStage::active, r.visitor.refused);
  r.run(72);
  const sv::RulesCommon& s = *r.g->campaign.server;
  // Slots 1 and 2 the companions, 0 the visitor: all three in the campaign's world, apart.
  CHECK(s.clients[0].active && r.g->campaign.players[0].name == "visitor");
  CHECK(s.sv.edicts[1].health == 100);
  CHECK(!se::info_get(r.visitor.userinfos[1], "*bot").empty());
  const auto apart = [&](int a, int b) {
    const ge::shooter::Vec3f p = s.sv.edicts[static_cast<size_t>(a)].origin,
                             q = s.sv.edicts[static_cast<size_t>(b)].origin;
    const float dx = p.x - q.x, dy = p.y - q.y;
    return dx * dx + dy * dy > 40 * 40;
  };
  CHECK(apart(1, 3) && apart(2, 3));
  CHECK(se::coop_seated(*r.g) == 3);
  CHECK(se::coop_status(*r.g).find("joined=1") != std::string::npos);
}

TEST(a_browsers_status_query_lists_the_game_as_coop) {
  Rig r(1);
  if (!r.g) return;
  const std::string info = r.status();
  CHECK_MSG(info.find("\\coop\\1") != std::string::npos, info);
  CHECK_MSG(info.find("\\deathmatch\\0") != std::string::npos, info);
  CHECK_MSG(info.find("\\skill\\2") != std::string::npos, info);
  CHECK_MSG(info.find("\\map\\start") != std::string::npos, info);
  CHECK_MSG(info.find("\\hostname\\coop test") != std::string::npos, info);
  CHECK_MSG(info.find("\\maxclients\\8") != std::string::npos, info);
}

TEST(seven_companions_leave_the_eighth_seat_to_a_player) {
  Rig r(7);
  if (!r.g) return;
  for (int f = 0; f < 400 && r.visitor.stage != se::ClientStage::active; f++) r.run(1);
  CHECK_MSG(r.visitor.stage == se::ClientStage::active, r.visitor.refused);
  CHECK(se::coop_seated(*r.g) == 8);
}

TEST(the_server_table_hands_a_coop_game_to_the_campaign) {
  // Without a game folder holding a single-player program the coop server is refused, naming it.
  gs::ServerConfig c = coop_config(0);
  c.root = "/nonexistent-coop-root";
  std::string error;
  void* g = se::classic_game().create(c, error);
  CHECK(g == nullptr);
  CHECK_MSG(!error.empty(), "no error for a missing folder");
}

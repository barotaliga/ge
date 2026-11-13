// Tests: coop over the network (net/classic_coop.hpp): a client joins the window's coop campaign as
// a server's, sees the window's player and its companions, enters the campaign and is carried
// onto the next map with everyone; a coop game takes eight players; without coop or a port nobody
// may join.
//
// ge:summary Tests: a client joins the window's coop campaign and follows it to the next map
// ge:keywords test coop co-op coop-join campaign-door classic_coop sv_port most-players
// ge:skill net shooter testing
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "account/service.hpp"
#include "check.hpp"
#include "net/sim_network.hpp"
#include "progs/qc_compile.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace se = shooters_edge::net;
namespace gn = ge::net;

constexpr uint16_t kPort = 27500, kClientPort = 27001;
constexpr int64_t kUnix0 = 1794500000ll * 1000000;
int64_t g_unix = kUnix0;
int64_t unix_now() { return g_unix; }

std::unique_ptr<cl::LoadedMap> stub_map(const std::string& name) {
  auto m = std::make_unique<cl::LoadedMap>();
  m->name = name;
  m->spec.name = name;
  m->spec.models = sv::arena_models();
  m->spec.entities = sv::kStubEntities;
  return m;
}

// Our campaign mod, compiled from its sources.
const ge::progs::Progs& campaign_mod() {
  static const ge::progs::Progs p = [] {
    const ge::progs::QcProject built = ge::progs::compile_project(
        std::filesystem::path(GE_SOURCE_DIR) / "games/shooters_edge/qc/campaign/progs.src");
    for (const ge::progs::QcDiagnostic& d : built.result.diagnostics)
      CHECK_MSG(!d.error, ge::progs::diagnostic_text(d));
    return built.result.progs;
  }();
  return p;
}

// The arena as a coop map: a start and two coop starts.
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
)";
  return m;
}

// The window's coop game on the stub campaign, served on a simulated network, and a visitor.
struct Coop {
  ge::progs::Progs stub;
  std::string error;
  gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
  gn::Transport port, client_port;
  cl::Listen l;
  se::ClassicClientEnd visitor;
  explicit Coop(int companions, bool coop = true, bool ours = false,
                ge::account::Service* accounts = nullptr) {
    if (!ours) CHECK_MSG(sv::build_campaign_stub(stub, error), error);
    gn::open_sim_transport(port, net, gn::sim_address(0, kPort));
    gn::open_sim_transport(client_port, net, gn::sim_address(1, kClientPort));
    cl::ListenOptions opt = se::listen_options();
    opt.console = {"deathmatch 0", coop ? "coop 1" : "coop 0"};
    opt.companions = companions;
    opt.transport = &port;
    opt.player = "host";
    if (accounts != nullptr) {
      opt.login.login = 1;
      opt.accounts = accounts;
      opt.unix_us = unix_now;
    }
    CHECK_MSG(ours ? cl::listen_start(l, campaign_mod(), coop_map("start"), opt, error)
                   : cl::listen_start(l, stub, stub_map("first"), opt, error),
              error);
    se::ClientConfig cc;
    cc.name = "visitor";
    se::client_start(visitor, client_port, gn::sim_address(0, kPort), cc, 0);
    l.next = stub_map("second");  // the stub changes level half a second in
  }
  ~Coop() { cl::listen_stop(l); }
  // `n` frames of the window, the visitor ticking on the network's time.
  void run(int n) {
    for (int f = 0; f < n && l.server != nullptr; f++) {
      const auto now = static_cast<int64_t>((l.net_s + sv::kCampaignStep) * 1e6);
      g_unix = kUnix0 + now;
      gn::sim_advance(net, now);
      se::client_tick(visitor, now);
      cl::listen_frame(l, {});
    }
  }
};

}  // namespace

TEST(a_client_joins_the_windows_coop_game_and_enters_the_campaign) {
  Coop g(1);
  CHECK(g.l.door != nullptr);
  for (int f = 0; f < 400 && g.visitor.stage != se::ClientStage::active; f++) g.run(1);
  CHECK_MSG(g.visitor.stage == se::ClientStage::active, g.visitor.refused);
  // Slot 0 the host, 1 his companion, 2 the visitor: all three in the campaign's world.
  CHECK(g.l.server->clients[0].active && g.l.server->clients[1].active &&
        g.l.server->clients[2].active);
  CHECK(g.l.campaign_host->players[2].active && g.l.campaign_host->players[2].name == "visitor");
  CHECK(se::info_get(g.visitor.userinfos[0], "name") == "host");
  CHECK(se::info_get(g.visitor.userinfos[0], "*bot").empty());
  CHECK(!se::info_get(g.visitor.userinfos[1], "*bot").empty());  // the companion: a bot
  CHECK(g.l.door_calls->joined(g.l.door) == 1);
}

TEST(a_joined_client_is_carried_onto_the_next_map_with_everyone) {
  Coop g(1);
  int frames = 0;
  while (!g.l.map_changed && frames < 400) {
    g.run(1);
    frames++;
  }
  CHECK_MSG(g.l.map_changed && g.l.failed.empty(), g.l.failed);
  CHECK(g.l.campaign_host->history.back() == "change second");
  CHECK(g.l.server->clients[2].active);  // the campaign carried him, before he signed on again
  g.run(120);
  CHECK_MSG(g.visitor.stage == se::ClientStage::active, g.visitor.refused);
  CHECK(g.visitor.models.size() > 1 && g.visitor.models[1] == "maps/second.bsp");
  CHECK(g.l.campaign_host->players[2].active && g.l.server->clients[2].active);
  CHECK(g.l.door_calls->joined(g.l.door) == 1);
}

TEST(a_coop_game_takes_eight_players_and_refuses_the_ninth) {
  Coop g(7);  // the host and seven companions
  g.run(30);
  CHECK(g.visitor.stage != se::ClientStage::active);
  CHECK_MSG(g.visitor.refused.find("full") != std::string::npos, g.visitor.refused);
  CHECK(!g.l.campaign_host->players[8].active);
}

TEST(a_campaign_without_coop_lets_nobody_join) {
  Coop g(0, false);
  CHECK(g.l.door == nullptr);
  g.run(30);
  CHECK(g.visitor.stage != se::ClientStage::active);
}

TEST(a_client_joins_our_campaign_in_coop_on_a_free_coop_start) {
  Coop g(1, true, true);
  for (int f = 0; f < 400 && g.visitor.stage != se::ClientStage::active; f++) g.run(1);
  CHECK_MSG(g.visitor.stage == se::ClientStage::active, g.visitor.refused);
  g.run(72);
  const sv::RulesCommon& s = *g.l.server;
  CHECK(s.clients[2].active && s.sv.edicts[3].health == 100);
  // Three players on three starts, none on another's.
  const auto apart = [&](int a, int b) {
    const ge::shooter::Vec3f p = s.sv.edicts[static_cast<size_t>(a)].origin,
                             q = s.sv.edicts[static_cast<size_t>(b)].origin;
    const float dx = p.x - q.x, dy = p.y - q.y;
    return dx * dx + dy * dy > 40 * 40;
  };
  CHECK(apart(1, 2) && apart(1, 3) && apart(2, 3));
  CHECK(sv::qc_of(g.l.campaign_host->server)->q.faults == 0);
}

TEST(a_client_logs_in_on_the_windows_coop_game) {
  namespace ga = ge::account;
  g_unix = kUnix0;
  gn::SimNetwork accounts_net = gn::make_sim_network(2, gn::LinkConditions{});
  gn::Transport own_port;
  gn::open_sim_transport(own_port, accounts_net, gn::sim_address(8, ga::kAccountPort));
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::Fresh f{};
  ga::create(store, "Niclas", "correct horse", g_unix, f);
  ga::Service own;
  uint64_t fresh_n = 1;
  ga::service_init(
      own, ga::ServiceConfig{}, std::move(store), own_port,
      [](void* ctx, uint8_t* out, size_t size) {
        auto* n = static_cast<uint64_t*>(ctx);
        for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>(*n * 131 + i * 17);
        (*n)++;
      },
      &fresh_n);
  f[0] = 7;
  const std::string code = ga::make_code(own.store, 1, g_unix, f).code;
  Coop g(0, true, false, &own);
  CHECK(g.l.door != nullptr);
  for (int f2 = 0; f2 < 400 && g.visitor.stage != se::ClientStage::active; f2++) g.run(1);
  CHECK_MSG(g.visitor.stage == se::ClientStage::active, g.visitor.refused);
  // The login is the campaign's server's too: told how, and taken before the campaign sees it.
  g.run(20);
  bool told = false;
  for (const std::string& line : g.visitor.prints)
    told = told || line.find("login <name> <code>") != std::string::npos;
  CHECK(told);
  se::client_command(g.visitor, "login niclas " + code);
  g.run(60);
  CHECK(g.visitor.playernum >= 0);
  if (g.visitor.playernum < 0) return;
  CHECK(se::info_get(g.visitor.userinfos[g.visitor.playernum], "*auth") == "Niclas");
}

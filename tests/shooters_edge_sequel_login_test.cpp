// games/shooters_edge/net/sequel_login.hpp: logins on the sequel's server. A stock client of the
// sequel is told how to log in in lines its console shows whole, logs in with a one-time code by
// a console line the rules never see, is shown by its account's name; a guest under a registered
// name plays as "player"; a seat that must log in and does not is dropped; with `sv_ranked` the
// players' seats nobody has are offered to the matchmaker under the sequel's folder.
//
// ge:summary Test: logins on the sequel's server, our scripted sequel client on a simulated net
// ge:keywords test sequel-login login prompt one-time-code reserved-names timeout sv_ranked
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "account/service.hpp"
#include "account/store.hpp"
#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/login.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_login.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace ga = ge::account;
namespace gs = ge::server;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

constexpr int64_t kT0 = 1794500000ll * 1000000;
int64_t g_unix = kT0;  // the wall clock the logins read
int64_t fixed_unix() { return g_unix; }

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

ga::Fresh fresh_of(uint8_t b) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++) f[i] = static_cast<uint8_t>(size_t{b} * 13 + i);
  return f;
}

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

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

// The sequel's server with its own accounts (Niclas is account 1), and clients.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, service_port;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::SequelNetServer server;
  ga::Service svc;
  cn::SequelLogin login;
  uint64_t fresh_n = 1;
  std::vector<std::unique_ptr<cn::SequelClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(int login, double time_s = 60, const std::string& ranked = "") {
  auto r = std::make_unique<Rig>();
  r->rules = sv::sequel_runner(sv::make_sequel_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  r->net = gn::make_sim_network(7, gn::LinkConditions{});
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortSequel));
  gn::open_sim_transport(r->service_port, r->net, gn::sim_address(9, ga::kAccountPort));
  CHECK_MSG(cn::sequel_net_start(r->server, r->rules, r->st, cn::SequelNetConfig{}, error), error);
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::create(store, "Niclas", "correct horse", kT0, fresh_of(1));
  ga::ServiceConfig c;
  for (size_t i = 0; i < c.seed.size(); i++) c.seed[i] = static_cast<uint8_t>(7 + i);
  ga::service_init(r->svc, c, std::move(store), r->service_port, counter_fresh, &r->fresh_n);
  gs::LoginSettings set;
  set.login = login;
  set.time_s = time_s;
  set.ranked = ranked;
  CHECK_MSG(cn::sequel_login_open(r->login, r->server, set, &r->svc, nullptr, fixed_unix, error),
            error);
  return r;
}

cn::SequelClientEnd& join(Rig& r, const std::string& name) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net, gn::sim_address(uint32_t(n + 1), 27901));
  r.clients.push_back(std::make_unique<cn::SequelClientEnd>());
  cn::SequelClientConfig cfg;
  cfg.name = name;
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::sequel_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortSequel), cfg,
                          static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::sequel_client_tick(*c, r.now);
    cn::sequel_net_frame(r.server, r.now);
    cn::sequel_login_frame(r.login);
  }
}

bool printed(const cn::SequelClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (p.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(sequel_login_lines_fit_its_console) {
  const std::vector<std::string> lines = cn::sequel_wrap(
      "This server asks you to log in: type login and your name and the code from your account "
      "page, within 60 seconds.");
  CHECK(lines.size() >= 3);
  for (const std::string& l : lines) CHECK_MSG(l.size() <= cn::kSequelPromptWidth, l);
  CHECK(lines[0].back() != ' ' && lines[1].front() != ' ');
  CHECK(cn::sequel_wrap("short").size() == 1);
}

TEST(sequel_login_a_stock_client_logs_in_by_a_console_line) {
  auto r = rig(1);
  cn::SequelClientEnd& c = join(*r, "eve");
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK_MSG(printed(c, "login"), "no prompt");
  for (const std::string& p : c.prints)
    for (const std::string& line : cn::sequel_wrap(p, 1000))
      CHECK_MSG(line.size() <= cn::kSequelPromptWidth + 1, line);
  const std::string code = ga::make_code(r->svc.store, 1, kT0, fresh_of(30)).code;
  cn::sequel_client_command(c, "login Niclas " + code);
  run(*r, 60);
  CHECK(gs::login_account(r->login.logins, 0) == 1);
  CHECK_MSG(r->server.clients[0].name == "Niclas", r->server.clients[0].name);
  for (const std::string& p : c.prints) CHECK_MSG(p.find(code) == std::string::npos, p);
}

TEST(sequel_login_a_guest_under_a_registered_name_plays_as_player) {
  auto r = rig(0);
  cn::SequelClientEnd& c = join(*r, "niclas");
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK_MSG(r->server.clients[0].name == "player", r->server.clients[0].name);
  CHECK(gs::login_account(r->login.logins, 0) == 0);
}

TEST(sequel_login_a_seat_that_must_log_in_and_does_not_is_dropped) {
  auto r = rig(2, 2);
  cn::SequelClientEnd& c = join(*r, "eve");
  run(*r, 400);  // 5 s
  CHECK(cn::sequel_net_clients(r->server) == 0);
  CHECK_MSG(c.stage != cn::ClientStage::active, "still in");
}

TEST(sequel_login_a_ranked_server_offers_its_free_seats_under_its_folder) {
  g_unix = kT0;
  auto r = rig(0, 60, "1on1");
  run(*r, 1);  // empty: every seat offered
  const auto& offers = r->svc.matchmaker.offers;
  CHECK(offers.size() == 1 && offers[0].seats == cn::SequelNetConfig{}.max_clients);
  join(*r, "eve");
  run(*r, 120);
  CHECK(cn::sequel_net_clients(r->server) == 1);
  g_unix += gs::kOfferEvery_us;  // the next offer due
  run(*r, 1);
  g_unix = kT0;
  CHECK(offers.size() == 1);
  if (offers.empty()) return;
  CHECK_MSG(offers[0].game == "baseq2", offers[0].game);
  CHECK(offers[0].mode == "1on1");
  CHECK_MSG(offers[0].seats == cn::SequelNetConfig{}.max_clients - 1,
            std::to_string(offers[0].seats));
  CHECK(r->login.logins.stats.offers >= 1);
}

// games/shooters_edge/net/q2_login.hpp: logins on Quake II's server. A stock client of the
// Quake II is told how to log in in lines its console shows whole, logs in with a one-time code by
// a console line the rules never see, is shown by its account's name; a guest under a registered
// name plays as "player"; a seat that must log in and does not is dropped; with `sv_ranked` the
// players' seats nobody has are offered to the matchmaker under Quake II's folder.
//
// ge:summary Test: logins on Quake II's server, our scripted Quake II client on a simulated net
// ge:keywords test q2-login login prompt one-time-code reserved-names timeout sv_ranked
// ge:skill net shooter testing
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "account/account_file.hpp"
#include "account/service.hpp"
#include "account/store.hpp"
#include "account/ticket.hpp"
#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/login.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_login.hpp"
#include "shooters_edge/net/q2_net_server.hpp"

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

// Quake II's server with its own accounts (Niclas is account 1), and clients.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, service_port;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::Q2NetServer server;
  ga::Service svc;
  cn::Q2Login login;
  uint64_t fresh_n = 1;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(int login, double time_s = 60, const std::string& ranked = "") {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  r->net = gn::make_sim_network(7, gn::LinkConditions{});
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  gn::open_sim_transport(r->service_port, r->net, gn::sim_address(9, ga::kAccountPort));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cn::Q2NetConfig{}, error), error);
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
  CHECK_MSG(cn::q2_login_open(r->login, r->server, set, &r->svc, nullptr, fixed_unix, error),
            error);
  return r;
}

cn::Q2ClientEnd& join(Rig& r, const std::string& name) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net, gn::sim_address(uint32_t(n + 1), 27901));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cn::Q2ClientConfig cfg;
  cfg.name = name;
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::q2_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortQ2), cfg,
                      static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::q2_client_tick(*c, r.now);
    cn::q2_net_frame(r.server, r.now);
    cn::q2_login_frame(r.login);
  }
}

bool printed(const cn::Q2ClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (p.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(q2_login_lines_fit_its_console) {
  const std::vector<std::string> lines = cn::q2_wrap(
      "This server asks you to log in: type login and your name and the code from your account "
      "page, within 60 seconds.");
  CHECK(lines.size() >= 3);
  for (const std::string& l : lines) CHECK_MSG(l.size() <= cn::kQ2PromptWidth, l);
  CHECK(lines[0].back() != ' ' && lines[1].front() != ' ');
  CHECK(cn::q2_wrap("short").size() == 1);
}

TEST(q2_login_a_stock_client_logs_in_by_a_console_line) {
  auto r = rig(1);
  cn::Q2ClientEnd& c = join(*r, "eve");
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK_MSG(printed(c, "login"), "no prompt");
  for (const std::string& p : c.prints)
    for (const std::string& line : cn::q2_wrap(p, 1000))
      CHECK_MSG(line.size() <= cn::kQ2PromptWidth + 1, line);
  const std::string code = ga::make_code(r->svc.store, 1, kT0, fresh_of(30)).code;
  cn::q2_client_command(c, "login Niclas " + code);
  run(*r, 60);
  CHECK(gs::login_account(r->login.logins, 0) == 1);
  CHECK_MSG(r->server.clients[0].name == "Niclas", r->server.clients[0].name);
  for (const std::string& p : c.prints) CHECK_MSG(p.find(code) == std::string::npos, p);
}

TEST(q2_login_a_guest_under_a_registered_name_plays_as_player) {
  auto r = rig(0);
  cn::Q2ClientEnd& c = join(*r, "niclas");
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK_MSG(r->server.clients[0].name == "player", r->server.clients[0].name);
  CHECK(gs::login_account(r->login.logins, 0) == 0);
}

TEST(q2_login_a_seat_that_must_log_in_and_does_not_is_dropped) {
  auto r = rig(2, 2);
  cn::Q2ClientEnd& c = join(*r, "eve");
  run(*r, 400);  // 5 s
  CHECK(cn::q2_net_clients(r->server) == 0);
  CHECK_MSG(c.stage != cn::ClientStage::active, "still in");
}

TEST(q2_login_a_ranked_server_offers_its_free_seats_under_its_folder) {
  g_unix = kT0;
  auto r = rig(0, 60, "1on1");
  run(*r, 1);  // empty: every seat offered
  const auto& offers = r->svc.matchmaker.offers;
  CHECK(offers.size() == 1 && offers[0].seats == cn::Q2NetConfig{}.max_clients);
  join(*r, "eve");
  run(*r, 120);
  CHECK(cn::q2_net_clients(r->server) == 1);
  g_unix += gs::kOfferEvery_us;  // the next offer due
  run(*r, 1);
  g_unix = kT0;
  CHECK(offers.size() == 1);
  if (offers.empty()) return;
  CHECK_MSG(offers[0].game == "baseq2", offers[0].game);
  CHECK(offers[0].mode == "1on1");
  CHECK_MSG(offers[0].seats == cn::Q2NetConfig{}.max_clients - 1, std::to_string(offers[0].seats));
  CHECK(r->login.logins.stats.offers >= 1);
}

// Our own window joined to our Quake II server logs itself in by its account file, as on the first
// game's: the server tells the tag of its logins' key (svc 127), the ticket is traded for a code
// with the master, and the login line goes to the server.
TEST(q2_login_our_window_logs_itself_in_by_its_ticket) {
  g_unix = kT0;
  auto r = rig(1);
  const ga::Ticket t = ga::ticket_issue(r->svc.keys, 1, "Niclas", g_unix / 1000000);
  uint8_t bytes[ga::kTicketMax];
  const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
  ga::AccountFile f;
  f.master = gn::to_string(gn::sim_address(9, ga::kAccountPort));
  f.key = gn::key_to_hex(ga::service_key(r->svc));
  f.name = "Niclas";
  f.ticket = ga::bytes_to_hex(std::vector<uint8_t>(bytes, bytes + n));
  const std::string path =
      (std::filesystem::temp_directory_path() / "ge_se_q2_login_test_account.txt").string();
  ga::account_file_write(path, f);
  gn::Transport t_play, t_login;
  gn::open_sim_transport(t_play, r->net, gn::sim_address(20, 27901));
  gn::open_sim_transport(t_login, r->net, gn::sim_address(20, 5000));
  cn::Remote remote;
  remote.q2_game = [] { return true; };
  cn::ClientConfig cc;
  cc.name = "nic";
  cn::remote_start(remote, t_play, gn::sim_address(0, cn::kPortQ2), cc);
  cn::remote_login_from(remote, path, fixed_unix, &t_login);
  remote.login->fresh = counter_fresh;
  remote.login->fresh_ctx = &r->fresh_n;
  std::vector<std::string> said;
  for (int i = 0; i < 77 * 6; i++) {
    r->now += 13000;
    g_unix = kT0 + r->now;
    gn::sim_advance(r->net, r->now);
    cn::remote_step(remote, 0.013, said);
    cn::q2_net_frame(r->server, r->now);
    cn::q2_login_frame(r->login);
    ga::service_update(r->svc, g_unix);
  }
  std::string all;
  for (const std::string& s : said) all += s + "|";
  CHECK_MSG(remote.demo.account_tag == gn::key_to_hex(ga::service_key(r->svc)).substr(0, 16),
            remote.demo.account_tag);
  CHECK_MSG(all.find("Logging in as Niclas.") != std::string::npos, all);
  CHECK(gs::login_account(r->login.logins, 0) == 1);
  CHECK_MSG(r->server.clients[0].name == "Niclas", r->server.clients[0].name + " " + all);
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

// Tests: the administrator's words on the classic endpoint (games/shooters_edge/net/
// classic_admin.hpp) over engine/net's simulated network: `users` and `status` list every seat by
// user id; `speedjump <id> on|off|<share>` grants one player the easy jump as the server-only
// `*speedjump` key every client sees, tells everyone and takes it back; a client cannot give it to
// itself in its connect; a client asks `users` itself; and the remote console refuses without a
// password, refuses a wrong one, runs a right one's line (the words above, then the rules'
// console) and answers the scripted client's `client_rcon`.
//
// ge:summary Tests: users, status, the easy jump granted per player, rcon on the classic endpoint
// ge:keywords test users status speedjump grant rcon rcon_password classic_admin easy-jump
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_trial.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

// A served arena on a clean simulated network, a raw socket beside it and two scripted clients.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;  // over `qc`
  gn::SimNetwork net;
  gn::Transport st, raw, ct[2];
  cn::ClassicServer server;
  cn::ClassicClientEnd c[2];
  int64_t now = 0;

  bool start(const cn::ServerConfig& cfg) {
    std::string error;
    if (!sv::build_test_mod(progs, error)) return false;
    sv::QcServerOptions so;
    so.loop.max_edicts = 1024;
    qc = sv::make_qc_server(progs, so, error);
    if (!qc) return false;
    sv::MapSpec map;
    map.name = "arena";
    map.models = sv::arena_models();
    map.entities = sv::kArenaEntities;
    if (!sv::load_map(*qc, map, error)) return false;
    gn::LinkConditions lan;
    gn::conditions_preset("lan", lan);
    net = gn::make_sim_network(1, lan);
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    gn::open_sim_transport(raw, net, gn::sim_address(9, cn::kPortClient));
    rules = sv::qc_runner_over(*qc);
    return cn::classic_start(server, rules, st, cfg, error);
  }
  void frame() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    for (cn::ClassicClientEnd& e : c)
      if (e.transport != nullptr) cn::client_tick(e, now);
  }
  void frames(int n) {
    for (int i = 0; i < n; i++) frame();
  }
  // Two scripted clients in play, the second asking for the easy jump (cl_speedjump 1).
  bool play() {
    for (int i = 0; i < 2; i++) {
      gn::open_sim_transport(ct[i], net,
                             gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
      cn::ClientConfig cc;
      cc.name = i == 0 ? "anna" : "bert";
      cc.qport = static_cast<uint16_t>(100 + i);
      cc.seed = i + 1;
      cc.speedjump = i == 1;
      cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
    }
    frames(600);
    return c[0].stage == cn::ClientStage::active && c[1].stage == cn::ClientStage::active;
  }
  void oob(const std::string& text) {
    std::string d = "\xff\xff\xff\xff" + text;
    gn::transport_send(raw, gn::sim_address(0, cn::kPortServer),
                       reinterpret_cast<const uint8_t*>(d.data()), d.size(), now);
  }
  // Every out-of-band answer the raw socket got, the marker and the print's `n` stripped.
  std::vector<std::string> answers() {
    std::vector<std::string> out;
    frames(20);
    uint8_t buf[2048];
    gn::Address from;
    int n;
    while ((n = gn::transport_receive(raw, buf, sizeof buf, from, now)) >= 0) {
      const auto size = static_cast<size_t>(n);
      if (size > 5 && cn::is_out_of_band(buf, size))
        out.emplace_back(reinterpret_cast<const char*>(buf) + 5);
    }
    return out;
  }
  int id_of(int slot) const { return server.clients[slot].userid; }
  std::string admin(const std::string& line) {
    std::string out;
    CHECK(cn::classic_admin(server, line, out));
    return out;
  }
};

bool has(const std::string& text, const std::string& what) {
  return text.find(what) != std::string::npos;
}

bool any_has(const std::vector<std::string>& v, const std::string& what) {
  for (const std::string& s : v)
    if (has(s, what)) return true;
  return false;
}

}  // namespace

TEST(users_and_status_list_every_seat_by_user_id) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  CHECK(rig.play());
  const std::string users = rig.admin("users");
  CHECK(has(users, std::to_string(rig.id_of(0)) + "     0 anna"));
  CHECK(has(users, "bert"));
  CHECK(has(users, "2 total users"));
  const std::string status = rig.admin("status");
  CHECK(has(status, "2 players, pm_speedjump 0"));
  CHECK(has(status, "anna"));
  // bert asked for the easy jump and the server allows nobody: `asks`; anna did not: `-`.
  for (size_t at = 0; at < status.size();) {
    const size_t end = status.find('\n', at);
    const std::string row = status.substr(at, end - at);
    if (has(row, "bert")) CHECK(row.substr(row.size() - 4) == "asks");
    if (has(row, "anna")) CHECK(row.back() == '-');
    at = end + 1;
  }
  std::string out;
  CHECK(!cn::classic_admin(rig.server, "fraglimit 10", out));  // not an administrator's word
}

TEST(speedjump_grants_one_player_disclosed_and_takes_it_back) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  CHECK(rig.play());
  const int bert = rig.c[1].playernum;
  CHECK(bert >= 0);
  CHECK(has(rig.admin("speedjump"), "granted to nobody"));
  // Granted by user id: the server-only key, every client told, the table shows it.
  const std::string answer = rig.admin("speedjump " + std::to_string(rig.id_of(bert)) + " on");
  CHECK(has(answer, "bert"));
  CHECK(has(answer, "granted at 0.8"));
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump") == "0.8");
  CHECK(cn::classic_speedjump_granted(rig.server, bert) == 0.8f);
  rig.frames(40);
  for (const cn::ClassicClientEnd& e : rig.c) {  // anna sees it on bert, bert on himself
    CHECK(cn::info_get(e.userinfos[bert], "*speedjump") == "0.8");
    bool told = false;
    for (const std::string& p : e.prints) told = told || has(p, "bert is granted the easy jump");
    CHECK(told);
  }
  CHECK(has(rig.admin("status"), "0.8 granted"));
  CHECK(has(rig.admin("speedjump"), "granted to 1:"));
  // By name, at a share of its own; then taken back.
  CHECK(has(rig.admin("speedjump bert 0.6"), "granted at 0.6"));
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump") == "0.6");
  CHECK(has(rig.admin("speedjump bert off"), "not granted"));
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump").empty());
  rig.frames(40);
  CHECK(cn::info_get(rig.c[0].userinfos[bert], "*speedjump").empty());
  // Mistakes say what to do next.
  CHECK(has(rig.admin("speedjump 77 on"), "next: `users`"));
  CHECK(has(rig.admin("speedjump bert 2"), "not on, off or a share"));
  std::string error;
  CHECK(!cn::classic_speedjump(rig.server, 77, 0.5f, error));
  CHECK(has(error, "user id 77"));
}

TEST(a_client_cannot_grant_itself_the_easy_jump) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  rig.oob("getchallenge\n");
  const std::vector<std::string> a = rig.answers();  // `c<challenge>`, its type stripped
  CHECK(a.size() == 1);
  const std::string challenge = a.empty() ? std::string() : a[0];
  CHECK(!challenge.empty());
  rig.oob("connect 28 1 " + challenge + " \"\\name\\x\\*speedjump\\1\"\n");
  rig.frames(20);
  int slot = -1;
  for (int i = 0; i < 32; i++)
    if (rig.server.clients[i].state != cn::ConnState::free) slot = i;
  CHECK(slot >= 0);
  CHECK(cn::info_get(rig.server.clients[slot].userinfo, "*speedjump").empty());
  CHECK(cn::classic_speedjump_granted(rig.server, slot) < 0);
}

TEST(a_client_asks_users_itself) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  CHECK(rig.play());
  cn::client_command(rig.c[0], "users");
  rig.frames(40);
  bool listed = false;
  for (const std::string& p : rig.c[0].prints) listed = listed || has(p, "2 total users");
  CHECK(listed);
}

TEST(rcon_needs_its_password_and_runs_a_line) {
  {  // No password set: not offered.
    auto owned = std::make_unique<Rig>();
    Rig& rig = *owned;
    CHECK(rig.start(cn::ServerConfig{}));
    rig.oob("rcon x users\n");
    CHECK(any_has(rig.answers(), "Remote console is not offered"));
  }
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.rcon_password = "secret";
  CHECK(rig.start(cfg));
  CHECK(rig.play());
  rig.oob("rcon wrong users\n");
  CHECK(any_has(rig.answers(), "Bad rcon_password."));
  CHECK(rig.server.counts.rcon_refused == 1);
  rig.oob("rcon secret users\n");
  CHECK(any_has(rig.answers(), "2 total users"));
  rig.oob("rcon \"secret\" speedjump bert on\n");
  CHECK(any_has(rig.answers(), "granted at 0.8"));
  const int bert = rig.c[1].playernum;
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump") == "0.8");
  // A line none of the administrator's words take goes to the rules' console.
  rig.oob("rcon secret fraglimit\n");
  CHECK(any_has(rig.answers(), "fraglimit is"));
  rig.oob("rcon secret nosuchword\n");
  CHECK(any_has(rig.answers(), "Unknown command"));
  CHECK(rig.server.counts.rcon_run == 4);
  // The scripted client's own remote console: the answer comes back out of band.
  cn::client_rcon(rig.c[0], "secret", "speedjump bert off");
  rig.frames(20);
  bool answered = false;
  for (const std::string& p : rig.c[0].oob_prints) answered = answered || has(p, "not granted");
  CHECK(answered);
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump").empty());
}

TEST(rcon_answers_at_most_its_tries_a_second) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.rcon_password = "secret";
  CHECK(rig.start(cfg));
  for (int i = 0; i < cn::kRconTriesPerSecond + 5; i++) rig.oob("rcon wrong users\n");
  const std::vector<std::string> a = rig.answers();
  int bad = 0;
  for (const std::string& s : a) bad += has(s, "Bad rcon_password.") ? 1 : 0;
  CHECK(bad == cn::kRconTriesPerSecond);
  CHECK(rig.server.counts.rcon_refused == cn::kRconTriesPerSecond + 5);
}

// Tests: the administrator's words on the classic endpoint (games/shooters_edge/net/
// classic_admin.hpp) over engine/net's simulated network: `users` and `status` list every seat by
// user id; `speedjump <id> on|off|<share>` grants one player the easy jump as the server-only
// `*speedjump` key every client sees, tells everyone and takes it back; a client cannot give it to
// itself in its connect; `aimhelp <name> on` grants the aim help, `users` shows it and a player
// who joins is told; a client asks `users` itself; and the remote console refuses without a
// password, refuses a wrong one, runs a right one's line (the words above, then the rules'
// console) and answers the scripted client's `client_rcon`.
//
// ge:summary Tests: users, status, the easy jump granted per player, rcon on the classic endpoint
// ge:keywords test users status speedjump grant rcon rcon_password classic_admin easy-jump
// ge:keywords aimhelp aim-help player-aids join-line
// ge:skill net shooter testing
#include <filesystem>
#include <fstream>
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
  // Scripted client `i` started: anna (0), or bert (1) asking for the easy jump (cl_speedjump 1).
  void begin(int i) {
    gn::open_sim_transport(ct[i], net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = i == 0 ? "anna" : "bert";
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    cc.speedjump = i == 1;
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc,
                     static_cast<double>(now) / 1e6);
  }
  // Both scripted clients in play.
  bool play() {
    for (int i = 0; i < 2; i++) begin(i);
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
  // bert asked for the easy jump and the server allows nobody: used, not allowed; anna did not:
  // `-`. `users` shows the same column.
  for (const std::string& table : {status, users})
    for (size_t at = 0; at < table.size();) {
      const size_t end = table.find('\n', at);
      const std::string row = table.substr(at, end - at);
      if (has(row, "bert")) CHECK(has(row, "easy jump (not allowed, cl_speedjump 1)"));
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
  CHECK(has(rig.admin("status"), "easy jump (granted 0.8, cl_speedjump 1)"));
  CHECK(has(rig.admin("users"), "easy jump (granted 0.8, cl_speedjump 1)"));
  CHECK(has(rig.admin("speedjump"), "granted to 1:"));
  // By name, at a share of its own; then taken back.
  CHECK(has(rig.admin("speedjump bert 0.6"), "granted at 0.6"));
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump") == "0.6");
  CHECK(has(rig.admin("speedjump bert off"), "not granted"));
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*speedjump").empty());
  rig.frames(40);
  CHECK(cn::info_get(rig.c[0].userinfos[bert], "*speedjump").empty());
  // Mistakes say what to do next.
  CHECK(has(rig.admin("speedjump 77 on"), "Type \"users\""));
  CHECK(has(rig.admin("speedjump bert 2"), "\"on\" or \"off\" or given a share"));
  std::string error;
  CHECK(!cn::classic_speedjump(rig.server, 77, 0.5f, error));
  CHECK(has(error, "user id 77"));
}

TEST(aimhelp_is_granted_by_name_shown_in_users_and_told_to_who_joins) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  CHECK(rig.start(cn::ServerConfig{}));
  rig.begin(1);  // bert first, alone
  rig.frames(600);
  CHECK(rig.c[1].stage == cn::ClientStage::active);
  const int bert = rig.c[1].playernum;
  CHECK(bert >= 0);
  CHECK(has(rig.admin("aimhelp"), "granted to nobody"));
  CHECK(cn::classic_aids_join_line(rig.server).empty());
  const std::string answer = rig.admin("aimhelp bert on");
  CHECK(has(answer, "aim help granted (24 across, 48 up and down)"));
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*aimhelp") == "1");
  CHECK(rig.server.game->sv.clients[bert].aimhelp);  // the rules read the grant
  CHECK(has(rig.admin("users"), "aim help (granted, 24 across, 48 up and down, not used)"));
  CHECK(has(rig.admin("aimhelp"), "granted to 1:"));
  CHECK(has(rig.admin("aimhelp bert 0.5"), "\"on\" or \"off\", not \"0.5\""));
  // anna joins and is told, in a game's words.
  rig.begin(0);
  rig.frames(600);
  CHECK(rig.c[0].stage == cn::ClientStage::active);
  bool told = false;
  for (const std::string& p : rig.c[0].prints)
    told = told || has(p,
                       "Player aids on this server: bert has aim help (24 across, 48 up and "
                       "down) and easy jump");
  CHECK(!told);  // bert's easy jump is asked for, not granted: only the aim help is told
  for (const std::string& p : rig.c[0].prints)
    told = told || has(p,
                       "Player aids on this server: bert has aim help (24 across, 48 up and "
                       "down).");
  CHECK(told);
  CHECK(has(rig.admin("aimhelp bert off"), "aim help not granted"));
  CHECK(!rig.server.game->sv.clients[bert].aimhelp);
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
  rig.oob("connect 28 1 " + challenge + " \"\\name\\x\\*speedjump\\1\\*aimhelp\\1\"\n");
  rig.frames(20);
  int slot = -1;
  for (int i = 0; i < 32; i++)
    if (rig.server.clients[i].state != cn::ConnState::free) slot = i;
  CHECK(slot >= 0);
  CHECK(cn::info_get(rig.server.clients[slot].userinfo, "*speedjump").empty());
  CHECK(cn::classic_speedjump_granted(rig.server, slot) < 0);
  CHECK(cn::info_get(rig.server.clients[slot].userinfo, "*aimhelp").empty());
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
  for (int i = 0; i < cfg.rcon_rate + 5; i++) rig.oob("rcon wrong users\n");
  const std::vector<std::string> a = rig.answers();
  int bad = 0;
  for (const std::string& s : a) bad += has(s, "Bad rcon_password.") ? 1 : 0;
  CHECK(bad == cfg.rcon_rate);
  CHECK(rig.server.counts.rcon_refused == cfg.rcon_rate + 5);
}

// sv_rconcrypt: a hashed password field (the SHA-1 of the password, the time and the line's words,
// then the time) runs the line without the password travelling; a stale time, a line other than
// the one hashed, or with sv_rconcrypt 1 the plain password, are refused.
namespace {
int64_t fixed_clock() { return 1800000000; }
}  // namespace

TEST(rcon_takes_a_hashed_password_and_sv_rconcrypt_requires_it) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.rcon_password = "secret";
  cfg.rcon_crypt = true;
  cfg.unix_time = &fixed_clock;
  CHECK(rig.start(cfg));
  CHECK(rig.play());
  rig.oob("rcon secret users\n");  // the plain password: refused
  CHECK(any_has(rig.answers(), "Bad rcon_password."));
  const std::string ok = cn::rcon_digest_field("secret", "users", fixed_clock() + 30);
  CHECK(ok.size() == 56);
  rig.oob("rcon " + ok + " users\n");
  CHECK(any_has(rig.answers(), "2 total users"));
  const std::string stale = cn::rcon_digest_field("secret", "users", fixed_clock() - 61);
  rig.oob("rcon " + stale + " users\n");
  CHECK(any_has(rig.answers(), "Bad rcon_password."));
  rig.oob("rcon " + ok + " status\n");  // a line other than the one hashed
  CHECK(any_has(rig.answers(), "Bad rcon_password."));
  CHECK(rig.server.counts.rcon_run == 1);
  CHECK(rig.server.counts.rcon_refused == 3);
}

// The ban list over the remote console: `addip` bans bert's address (a simulated node counts as
// 198.18.0.<node>), so he is refused at connect and anna plays; `writeip` saves the list as
// `addip` lines; an address the list bans gets no rcon answer; sv_banmode 0 turns the list round.
TEST(the_ban_list_refuses_its_addresses_and_saves_itself) {
  const std::string file =
      (std::filesystem::temp_directory_path() / "se_admin_listip.cfg").string();
  std::filesystem::remove(file);
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.rcon_password = "secret";
  cfg.ban_file = file;
  CHECK(rig.start(cfg));
  rig.oob("rcon secret addip 198.18.0.2\n");
  CHECK(any_has(rig.answers(), "added 198.18.0.2"));
  rig.oob("rcon secret listip\n");
  CHECK(any_has(rig.answers(), "1 addresses listed"));
  for (int i = 0; i < 2; i++) rig.begin(i);
  rig.frames(600);
  CHECK(rig.c[0].stage == cn::ClientStage::active);
  CHECK(rig.c[1].stage != cn::ClientStage::active);
  CHECK_MSG(rig.c[1].refused.find("banned") != std::string::npos, rig.c[1].refused);
  rig.oob("rcon secret writeip\n");
  CHECK(any_has(rig.answers(), "Saved 1 addresses"));
  std::ifstream f(file);
  std::string line;
  CHECK(std::getline(f, line) && line == "addip 198.18.0.2");
  rig.oob("rcon secret addip 198.18.0.9\n");  // the console's own address
  CHECK(any_has(rig.answers(), "added"));
  rig.oob("rcon secret listip\n");
  CHECK(rig.answers().empty());  // banned: no answer
  rig.server.cfg.ban_mode = 0;   // only the list joins: the console's address again
  rig.oob("rcon secret listip\n");
  CHECK(any_has(rig.answers(), "2 addresses listed"));
  std::filesystem::remove(file);
}

// VIPs: a client whose password is a word of sv_vippassword skips the server's password and
// carries `*VIP <level>` (sv_vipvalues' number); one on the VIP address list gets its level too.
TEST(a_vip_skips_the_password_and_carries_its_level) {
  auto owned = std::make_unique<Rig>();
  Rig& rig = *owned;
  cn::ServerConfig cfg;
  cfg.password = "door";
  cfg.vip_passwords = {"silver", "gold"};
  cfg.vip_values = {2, 7};
  cfg.rcon_password = "secret";
  CHECK(rig.start(cfg));
  rig.oob("rcon secret vip_addip 198.18.0.2 3\n");  // bert's address
  CHECK(any_has(rig.answers(), "added 198.18.0.2 3"));
  for (int i = 0; i < 2; i++) rig.begin(i);
  rig.c[0].cfg.password = "gold";  // anna: a VIP word; bert: no password, but a VIP address
  rig.frames(600);
  CHECK(rig.c[0].stage == cn::ClientStage::active);
  CHECK(rig.c[1].stage == cn::ClientStage::active);
  const int anna = rig.c[0].playernum, bert = rig.c[1].playernum;
  CHECK(cn::info_get(rig.server.clients[anna].userinfo, "*VIP") == "7");
  CHECK(cn::info_get(rig.server.clients[bert].userinfo, "*VIP") == "3");
  CHECK(cn::info_get(rig.server.clients[anna].userinfo, "password").empty());
}

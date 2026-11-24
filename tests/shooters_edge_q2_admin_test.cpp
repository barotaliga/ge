// games/shooters_edge/net/q2_admin.hpp: Quake II server's administrator words over its remote
// console. No password and a wrong one are refused alike; `status` lists the clients; a rules
// line runs on the rules' console; `kick` and `dumpuser` take a slot or a name and answer a bad
// one in the server's words; `addip` drops a client it comes to refuse and refuses its next
// connect ("Banned."); `writeip` saves the list and the server reads it back; a banned address's
// remote console is not answered. Spectators: their password is the `spectator` key, their own
// slots under `max_spectators` (or players' slots when 0), and `status` tells both in its info.
// The game's own `sv addip`, `sv removeip`, `sv listip` and `sv writeip` in its words; the status
// info's rules keys, the build's, the port and an administrator's own.
//
// ge:summary Test: Quake II server's rcon, status, kick, dumpuser, bans and spectator seats
// ge:keywords test q2 admin rcon rcon_password status kick dumpuser addip writeip listip
// ge:keywords banned filterban sv_banmode ban_file_read spectator maxspectators needpass
// ge:keywords spectator_password sv_maxspectators sv_spectatorpassword zombie zombietime
// ge:keywords sv_zombietime sv_filter q2_status_keys gamedir
// ge:skill net shooter
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/num.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/bans.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace gs = ge::server;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
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
  for (const char* at : {"-400 -400 24", "400 400 24"}) {
    br::Entity e;
    e.pairs = {{"classname", "info_player_deathmatch"}, {"origin", at}};
    w.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

// Our server, two players and an administrator's socket on the simulated network.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, admin;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::Q2NetServer server;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::Q2NetConfig& cfg) {
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
  gn::open_sim_transport(r->admin, r->net, gn::sim_address(9, 27960));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

// A client named `name`; a spectator when `watch` is set, giving it as the spectator's password
// ("1": none).
cn::Q2ClientEnd& join(Rig& r, const std::string& name, const std::string& watch = "") {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net, gn::sim_address(uint32_t(n + 1), 27901));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cn::Q2ClientConfig cfg;
  cfg.name = name;
  cfg.spectator = !watch.empty();
  cfg.spectator_password = watch == "1" ? "" : watch;
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
  }
}

// An administrator's out-of-band line, and the server's answer ("" when none came).
std::string rcon(Rig& r, const std::string& text) {
  const std::vector<uint8_t> b = cn::q2_oob(text);
  gn::transport_send(r.admin, gn::sim_address(0, cn::kPortQ2), b.data(), b.size(), r.now);
  run(r, 8);
  std::string answer;
  uint8_t buf[2048];
  gn::Address from;
  for (int n; (n = gn::transport_receive(r.admin, buf, sizeof buf, from, r.now)) >= 0;)
    answer += cn::q2_oob_text(buf, static_cast<size_t>(n));
  return answer;
}

bool has(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}

// The ban pattern of a simulated node's address.
std::string pattern_of(uint32_t node) {
  uint8_t ip[4];
  gn::sim_to_ipv4(gn::sim_address(node, 0), ip);
  char t[24];
  std::snprintf(t, sizeof t, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return t;
}

bool printed(const cn::Q2ClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (has(p, part)) return true;
  return has(c.refused, part);
}

}  // namespace

TEST(a_remote_console_without_the_password_is_refused) {
  auto none = rig({});
  CHECK(rcon(*none, "rcon anything status") == "print\nBad rcon_password.\n");
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  CHECK(rcon(*r, "rcon wrong status") == "print\nBad rcon_password.\n");
  CHECK(r->server.rcon_refused == 1 && r->server.rcon_runs == 0);
  for (const std::string& line : r->server.log) CHECK_MSG(!has(line, "wrong"), line);
}

// sv_rconrate (1 a second, 5 at once): a guesser's sixth try in a breath gets no answer; once the
// bucket refills the administrator is answered, and the good try costs nothing.
TEST(guesses_past_sv_rconrate_are_not_answered) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  for (int i = 0; i < 5; i++) CHECK(rcon(*r, "rcon wrong status") == "print\nBad rcon_password.\n");
  CHECK(rcon(*r, "rcon wrong status").empty());
  CHECK(rcon(*r, "rcon letmein status").empty());
  run(*r, 80);  // a second
  for (int i = 0; i < 3; i++) CHECK(has(rcon(*r, "rcon letmein status"), "map"));
  CHECK(r->server.rcon_runs == 3);
}

// sv_status and sv_uptime: the info with how long the server has run, the players' lines only at
// 2, no answer at 0; sv_statusrate (15 a second, 5 at once) leaves a flood unanswered.
TEST(a_status_query_shows_what_sv_status_allows_and_the_uptime) {
  cn::Q2NetConfig cfg;
  cfg.status.uptime = 1;
  auto r = rig(cfg);
  join(*r, "ana");
  run(*r, 80);
  const std::string full = rcon(*r, "status");
  CHECK_MSG(has(full, "\\uptime\\00.0") && has(full, "\"ana\""), full);
  r->server.cfg.status.show = 1;
  const std::string info = rcon(*r, "status");
  CHECK_MSG(has(info, "\\uptime\\") && !has(info, "\"ana\""), info);
  r->server.cfg.status.show = 0;
  CHECK(rcon(*r, "status").empty());
  r->server.cfg.status.show = 2;
  run(*r, 80);
  int answered = 0;
  for (int i = 0; i < 8; i++) {  // eight at once (no frames between): the burst's five answered
    const std::vector<uint8_t> b = cn::q2_oob("status");
    gn::transport_send(r->admin, gn::sim_address(0, cn::kPortQ2), b.data(), b.size(), r->now);
  }
  run(*r, 8);
  uint8_t buf[2048];
  gn::Address from;
  while (gn::transport_receive(r->admin, buf, sizeof buf, from, r->now) >= 0) answered++;
  CHECK_MSG(answered == 5, std::to_string(answered));
}

// lrcon_password: a helper runs the lines the administrator allowed over the full console, and
// nothing else; the full password still runs everything.
TEST(a_helper_runs_only_the_allowed_lines) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "boss";
  cfg.lrcon.password = "helper";
  auto r = rig(cfg);
  CHECK(rcon(*r, "rcon helper status") == "print\nThis command is not permitted.\n");
  CHECK(has(rcon(*r, "rcon boss addlrconcmd sv_locked"), "Helpers may now run"));
  CHECK(rcon(*r, "rcon helper sv_locked 1") == "print\nServer locked: no new clients.\n");
  CHECK(r->server.cfg.locked);
  CHECK(rcon(*r, "rcon helper lrcon_add status") == "print\nThis command is not permitted.\n");
  CHECK(has(rcon(*r, "rcon boss status"), "map"));
}

TEST(status_lists_the_clients_and_a_rules_line_runs_on_their_console) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  join(*r, "eve");
  join(*r, "bob");
  run(*r, 200);
  const std::string status = rcon(*r, "rcon letmein status");
  CHECK_MSG(has(status, "Current map: hall\n\n") && has(status, " eve ") && has(status, " bob "),
            status);
  CHECK_MSG(
      has(status, "num score ping name            lastmsg address                rate pr fps\n"),
      status);
  CHECK_MSG(has(status, " 15000 34 "), status);  // the rate in force and the protocol
  rcon(*r, "rcon letmein fraglimit 7");
  const ge::console::Cvar* frags = r->rules->cvars.find("fraglimit");
  CHECK(frags != nullptr && frags->value == "7");
  CHECK(has(rcon(*r, "rcon letmein nosuchword"), "Unknown command \"nosuchword\""));
  CHECK(r->server.rcon_runs == 3);
  for (const std::string& line : r->server.log) CHECK_MSG(!has(line, "letmein"), line);
}

TEST(kick_and_dumpuser_take_a_slot_or_a_name) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  cn::Q2ClientEnd& eve = join(*r, "eve");
  cn::Q2ClientEnd& bob = join(*r, "bob");
  run(*r, 200);
  CHECK_MSG(eve.stage == cn::ClientStage::active && bob.stage == cn::ClientStage::active,
            eve.refused + bob.refused);
  const std::string dump = rcon(*r, "rcon letmein dumpuser bob");
  CHECK_MSG(has(dump, "\nuserinfo\n--------\nname                 bob\n"), dump);
  CHECK_MSG(has(dump, "\nmiscinfo\n--------\nversion              "), dump);
  CHECK_MSG(has(dump, "protocol (maj/min)   34/0\n") && has(dump, "netchan type         old\n"),
            dump);
  CHECK(has(dump, "ip                   sim:2:"));  // the address the server saw, forced
  CHECK(rcon(*r, "rcon letmein kick 99") == "print\nBad client slot number: 99\n");
  CHECK(rcon(*r, "rcon letmein kick 5") == "print\nClient slot 5 is not active.\n");
  CHECK(rcon(*r, "rcon letmein kick nobody") == "print\nUserid 'nobody' is not on the server.\n");
  CHECK(rcon(*r, "rcon letmein kick BO") == "print\nUserid 'BO' is not on the server.\n");
  CHECK(has(rcon(*r, "rcon letmein kick bob"), "bob[sim:2:"));
  run(*r, 40);
  CHECK(bob.stage == cn::ClientStage::dropped);
  CHECK(printed(bob, "bob was kicked"));
  CHECK(printed(eve, "bob was kicked"));
  CHECK(eve.stage == cn::ClientStage::active);
}

TEST(a_dropped_slot_is_a_zombie_for_zombie_time_then_free) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  cfg.max_clients = 2;
  auto r = rig(cfg);
  cn::Q2ClientEnd& eve = join(*r, "eve");
  cn::Q2ClientEnd& bob = join(*r, "bob");
  run(*r, 200);
  CHECK_MSG(eve.stage == cn::ClientStage::active && bob.stage == cn::ClientStage::active,
            eve.refused + bob.refused);
  CHECK(has(rcon(*r, "rcon letmein kick bob"), "] was kicked"));
  CHECK(r->server.clients[1].stage == cn::Q2Slot::zombie);
  CHECK(cn::q2_net_clients(r->server) == 1);
  const std::string status = rcon(*r, "rcon letmein status");
  CHECK_MSG(has(status, "ZMBI bob"), status);
  cn::Q2ClientEnd& cy = join(*r, "cy");  // the zombie still holds the last slot
  run(*r, 40);
  CHECK(bob.stage == cn::ClientStage::dropped);  // the goodbye reached him
  CHECK_MSG(has(cy.refused, "Server is full."), cy.refused);
  run(*r, 160);  // two seconds after bob's last packet
  CHECK(r->server.clients[1].stage == cn::Q2Slot::free);
  cn::Q2ClientEnd& di = join(*r, "di");
  run(*r, 200);
  CHECK_MSG(di.stage == cn::ClientStage::active, di.refused);
  CHECK(r->server.clients[1].name == "di");
}

TEST(a_banned_address_is_dropped_refused_and_not_answered) {
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "ge_q2_admin_listip.cfg";
  std::filesystem::remove(file);
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  cfg.ban_file = file.string();
  auto r = rig(cfg);
  cn::Q2ClientEnd& eve = join(*r, "eve");  // node 1
  run(*r, 200);
  CHECK(eve.stage == cn::ClientStage::active);
  CHECK(has(rcon(*r, "rcon letmein addip " + pattern_of(1)), "added"));
  run(*r, 40);
  CHECK(eve.stage == cn::ClientStage::dropped);
  CHECK(has(rcon(*r, "rcon letmein writeip"), "Saved 1 addresses"));
  cn::Q2ClientEnd& again = join(*r, "eve");  // node 2: let in
  run(*r, 200);
  CHECK_MSG(again.stage == cn::ClientStage::active, again.refused);

  // A server started again reads the file back, and refuses node 1's connect.
  auto second = rig(cfg);
  gs::ban_file_read(second->server.bans, cfg.ban_file, 0);
  CHECK(second->server.bans.entries.size() == 1);
  cn::Q2ClientEnd& banned = join(*second, "eve");
  run(*second, 80);
  CHECK(banned.stage == cn::ClientStage::dropped);
  CHECK(banned.refused == "Banned.\n");

  // A remote console from a banned address gets no answer; `sv_banmode` 0 lets in only the list.
  CHECK(has(rcon(*second, "rcon letmein addip " + pattern_of(9)), "added"));
  CHECK(rcon(*second, "rcon letmein status").empty());
  cfg.ban_mode = 0;
  auto only = rig(cfg);
  gs::ban_file_read(only->server.bans, cfg.ban_file, 0);
  cn::Q2ClientEnd& listed = join(*only, "eve");    // node 1, on the list
  cn::Q2ClientEnd& unlisted = join(*only, "bob");  // node 2
  run(*only, 200);
  CHECK_MSG(listed.stage == cn::ClientStage::active, listed.refused);
  CHECK(unlisted.refused == "Banned.\n");
  std::filesystem::remove(file);
}

// The game's own filter words: `sv addip` with a short or zero mask as a wildcard, `sv listip` in
// the game's columns, `sv removeip` by the same mask, `sv writeip` as the game's cfg (the mode and
// `sv addip` lines), read back with its mode.
TEST(the_game_s_sv_filter_words_reach_the_ban_list) {
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "ge_q2_admin_sv_listip.cfg";
  std::filesystem::remove(file);
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  cfg.ban_file = file.string();
  auto r = rig(cfg);
  CHECK(rcon(*r, "rcon letmein sv addip") == "print\nUsage:  addip <ip-mask>\n");
  CHECK(rcon(*r, "rcon letmein sv addip 10.x") == "print\nBad filter address: 10.x\n");
  CHECK(rcon(*r, "rcon letmein sv addip 10.0") == "print\n");
  CHECK(rcon(*r, "rcon letmein sv addip 192.168.4.7") == "print\n");
  CHECK(r->server.bans.entries.size() == 2 && r->server.bans.entries[0].text == "10.*.*.*");
  CHECK_MSG(rcon(*r, "rcon letmein sv listip") ==
                "print\nFilter list:\n 10.  0.  0.  0\n192.168.  4.  7\n",
            rcon(*r, "rcon letmein sv listip"));
  CHECK(rcon(*r, "rcon letmein sv removeip") == "print\nUsage:  sv removeip <ip-mask>\n");
  CHECK(rcon(*r, "rcon letmein sv removeip 10.0.0.0") == "print\nRemoved.\n");
  CHECK(rcon(*r, "rcon letmein sv removeip 10.0.0.0") == "print\nDidn't find 10.0.0.0.\n");
  CHECK(rcon(*r, "rcon letmein sv writeip") == "print\nWriting " + file.string() + ".\n");
  CHECK(rcon(*r, "rcon letmein sv test") == "print\nSvcmd_Test_f()\n");  // the rules' own
  r->server.cfg.ban_mode = 0;  // only the list joins (the administrator's address too: no rcon)
  std::string wrote;
  CHECK(cn::q2_admin(r->server, "sv writeip", wrote) && has(wrote, "Writing"));
  int mode = 1;
  gs::BanList back;
  gs::ban_file_read(back, file.string(), 0, &mode);
  CHECK(back.entries.size() == 1 && mode == 0);
  CHECK(back.entries[0].text == "192.168.4.7");
  std::filesystem::remove(file);
}

// The status info carries the rules' live settings and the build's keys.
TEST(the_status_info_carries_the_rules_keys) {
  cn::Q2NetConfig cfg;
  cfg.version = "ShootersEdge 0.1";
  cfg.admin_info = [] { return std::string("\\owner\\the clan\\league\\spring"); };
  auto r = rig(cfg);
  r->rules.console("fraglimit 25");
  r->rules.console("timelimit 10");
  r->rules.console("dmflags 16");
  const std::string info = rcon(*r, "status");
  for (const char* part :
       {"\\deathmatch\\1", "\\coop\\0", "\\dmflags\\16", "\\fraglimit\\25", "\\timelimit\\10",
        "\\gamename\\baseq2", "\\gamedir\\baseq2", "\\version\\ShootersEdge 0.1",
        "\\owner\\the clan", "\\league\\spring", "\\port\\27910"})
    CHECK_MSG(has(info, part), part + std::string(" in ") + info);
}

TEST(spectators_give_their_password_and_take_their_own_slots) {
  cn::Q2NetConfig cfg;
  cfg.max_clients = 1;
  cfg.max_spectators = 1;
  cfg.spectator_password = "watch";
  auto r = rig(cfg);
  cn::Q2ClientEnd& eve = join(*r, "eve");
  cn::Q2ClientEnd& wrong = join(*r, "sam", "1");
  run(*r, 200);
  CHECK_MSG(eve.stage == cn::ClientStage::active, eve.refused);
  CHECK(wrong.refused == "Spectator password required or incorrect.\nConnection refused.\n");
  cn::Q2ClientEnd& sam = join(*r, "sam", "watch");
  run(*r, 200);
  CHECK_MSG(sam.stage == cn::ClientStage::active, sam.refused);
  CHECK(r->server.clients[1].spectator && !r->server.clients[0].spectator);
  cn::Q2ClientEnd& more = join(*r, "ann", "watch");
  cn::Q2ClientEnd& player = join(*r, "bob");
  run(*r, 200);
  CHECK(more.refused == "Server spectator limit is full.\nConnection refused.\n");
  CHECK(player.refused == "Server is full.\n");
  const std::string status = rcon(*r, "status");
  CHECK_MSG(has(status, "\\maxspectators\\1") && has(status, "\\needpass\\2"), status);
  CHECK(has(status, "\"eve\"") && has(status, "\"sam\""));
}

TEST(with_no_spectator_slots_a_spectator_takes_a_players) {
  cn::Q2NetConfig cfg;
  cfg.max_clients = 2;
  cfg.password = "secret";
  auto r = rig(cfg);
  cn::Q2ClientEnd& sam = join(*r, "sam", "1");  // no player password asked of a spectator
  cn::Q2ClientEnd& eve = join(*r, "eve");
  run(*r, 200);
  CHECK_MSG(sam.stage == cn::ClientStage::active, sam.refused);
  CHECK(eve.refused == "Password required or incorrect.\nConnection refused.\n");
  CHECK(has(rcon(*r, "status"), "\\needpass\\1"));
  r->server.cfg.password.clear();
  cn::Q2ClientEnd& bob = join(*r, "bob");
  run(*r, 200);
  cn::Q2ClientEnd& ann = join(*r, "ann", "1");
  run(*r, 200);
  CHECK_MSG(bob.stage == cn::ClientStage::active, bob.refused);
  CHECK(ann.refused == "Server is full.\n");
}

TEST(a_filtered_command_never_reaches_the_rules_and_a_blackholed_address_is_never_heard) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  cn::Q2ClientEnd& eve = join(*r, "eve");  // node 1
  run(*r, 200);
  CHECK(eve.stage == cn::ClientStage::active);
  // In our words and the modern server's.
  CHECK(has(rcon(*r, "rcon letmein filtercmd_add god print \"Not on this server.\""), "added"));
  CHECK(has(rcon(*r, "rcon letmein addfiltercmd noclip stuff \"echo no\""), "added"));
  CHECK(has(rcon(*r, "rcon letmein listfiltercmds"), "2 commands filtered"));
  cn::q2_client_command(eve, "god");
  cn::q2_client_command(eve, "NOCLIP");
  run(*r, 40);
  CHECK(printed(eve, "Not on this server."));
  CHECK(std::find(eve.stuffed.begin(), eve.stuffed.end(), "echo no") != eve.stuffed.end());
  CHECK(r->server.filtered == 2);
  CHECK(has(rcon(*r, "rcon letmein filtercmd_add kill kick"), "added"));
  cn::q2_client_command(eve, "kill");
  run(*r, 40);
  CHECK(eve.stage == cn::ClientStage::dropped);
  CHECK(std::any_of(r->server.log.begin(), r->server.log.end(), [](const std::string& l) {
    return l.rfind("eve[", 0) == 0 && has(l, "]: issued banned command: kill");
  }));

  // A blackholed address: its datagrams are dropped unread, a query's and a connect's alike.
  CHECK(has(rcon(*r, "rcon letmein addblackhole " + pattern_of(2)), "added"));
  CHECK(has(rcon(*r, "rcon letmein blackhole_listip"), pattern_of(2)));
  cn::Q2ClientEnd& unheard = join(*r, "bob");  // node 2
  run(*r, 200);
  CHECK(unheard.stage != cn::ClientStage::active && unheard.refused.empty());
  CHECK(r->server.blackholed > 0);
  CHECK(has(rcon(*r, "rcon letmein delblackhole " + pattern_of(2)), "removed"));
}

namespace {

// A client whose console's gl_modulate is `modulate` and whose skin is `skin` (node `n + 1`).
cn::Q2ClientEnd& join_as(Rig& r, const std::string& name, const std::string& modulate,
                         const std::string& skin = "male/grunt") {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net, gn::sim_address(uint32_t(n + 1), 27901));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cn::Q2ClientConfig cfg;
  cfg.name = name;
  cfg.skin = skin;
  cfg.qport = static_cast<uint16_t>(100 + n);
  auto value = std::make_shared<std::string>(modulate);
  cfg.setting = [value](const std::string& v) { return v == "gl_modulate" ? *value : ""; };
  cn::q2_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortQ2), cfg,
                      static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

}  // namespace

TEST(a_setting_ban_asks_each_client_and_acts_on_its_answer) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  CHECK(has(rcon(*r, "rcon letmein addcvarban gl_modulate >3 print \"Too bright here.\""),
            "Added cvarban 1"));
  CHECK(has(rcon(*r, "rcon letmein cvarban_add gl_modulate >5 kick \"Far too bright.\""),
            "Added cvarban 2"));
  CHECK(has(rcon(*r, "rcon letmein adduserinfoban skin ~cheat kick \"No such skins here.\""),
            "Added userinfoban 1"));
  cn::Q2ClientEnd& eve = join_as(*r, "eve", "4");                  // printed to, stays
  cn::Q2ClientEnd& bob = join_as(*r, "bob", "6");                  // kicked
  cn::Q2ClientEnd& ann = join_as(*r, "ann", "1");                  // nothing
  cn::Q2ClientEnd& zed = join_as(*r, "zed", "1", "male/cheater");  // refused at connect
  run(*r, 300);
  CHECK(eve.stage == cn::ClientStage::active && printed(eve, "Too bright here."));
  CHECK(bob.stage == cn::ClientStage::dropped && printed(bob, "Far too bright."));
  CHECK(ann.stage == cn::ClientStage::active && !printed(ann, "bright"));
  CHECK_MSG(zed.refused == "No such skins here.\nConnection refused.\n", zed.refused);
  CHECK(std::any_of(r->server.log.begin(), r->server.log.end(), [](const std::string& l) {
    return l.rfind("bob[", 0) == 0 && has(l, "]: matched cvarban: \"gl_modulate\" is \"6\"");
  }));
  CHECK(has(rcon(*r, "rcon letmein checkcvarbans"), "Asked 2 players"));
  CHECK(has(rcon(*r, "rcon letmein listcvarbans"), "Far too bright."));
  CHECK(has(rcon(*r, "rcon letmein delcvarban all"), "Removed 2 cvarbans."));
}

TEST(the_ping_is_the_least_or_the_average_of_the_last_round_trips_by_sv_pingmethod) {
  for (int method = 0; method <= 2; method++) {
    cn::Q2NetConfig cfg;
    cfg.ping_method = method;
    auto r = rig(cfg);
    join(*r, "eve");
    run(*r, 300);
    const cn::Q2NetClient& c = r->server.clients[0];
    CHECK(c.stage == cn::Q2Slot::active && c.trip_count > 16);
    CHECK((*r->rules).clients[0].ping_ms == c.ping_ms);
    double least = c.trips[0], sum = 0;
    for (const double t : c.trips) {
      least = t < least ? t : least;
      sum += t;
    }
    const double want = method == 0 ? 0 : method == 1 ? sum / 16 : least;
    CHECK_MSG(c.ping_ms == static_cast<int>(ge::core::num::round(want * 1000)),
              std::to_string(c.ping_ms));
    CHECK(method == 0 || c.ping_ms > 0);
    CHECK(has(rcon(*r, "status"), "0 " + std::to_string(c.ping_ms) + " \"eve\""));
  }
}

TEST(sv_nodelta_0_drops_a_client_that_keeps_asking_for_whole_frames) {
  for (const bool allow : {true, false}) {
    cn::Q2NetConfig cfg;
    cfg.allow_nodelta = allow;
    auto r = rig(cfg);
    const int n = static_cast<int>(r->clients.size());
    r->ct.push_back(std::make_unique<gn::Transport>());
    gn::open_sim_transport(*r->ct.back(), r->net, gn::sim_address(uint32_t(n + 1), 27901));
    r->clients.push_back(std::make_unique<cn::Q2ClientEnd>());
    cn::Q2ClientConfig cc;
    cc.name = "eve";
    cc.whole_every = 0.3;  // a spell of 150 ms asking for whole frames every 0.3 s
    cn::q2_client_start(*r->clients.back(), *r->ct.back(), gn::sim_address(0, cn::kPortQ2), cc, 0);
    run(*r, 600);
    CHECK_MSG((r->clients.back()->stage == cn::ClientStage::active) == allow,
              r->clients.back()->refused);
    CHECK(allow || std::find(r->server.log.begin(), r->server.log.end(),
                             "eve dropped: too many nodelta frames") != r->server.log.end());
  }
}

TEST(a_slot_taken_by_a_connect_and_never_used_is_free_again_after_sv_ghosttime) {
  cn::Q2NetConfig cfg;
  auto r = rig(cfg);
  std::string answer = rcon(*r, "getchallenge");
  CHECK_MSG(answer.rfind("challenge ", 0) == 0, answer);
  const std::string challenge = answer.substr(10, answer.find_first_of(" \n", 10) - 10);
  answer = rcon(*r, "connect 34 7 " + challenge + " \"\\name\\ghost\"");
  CHECK_MSG(answer.rfind("client_connect", 0) == 0, answer);
  CHECK(r->server.clients[0].stage == cn::Q2Slot::signon);
  run(*r, 400);  // 5.2 s: still held
  CHECK(r->server.clients[0].stage == cn::Q2Slot::signon);
  run(*r, 100);  // past 6 s
  CHECK(r->server.clients[0].stage == cn::Q2Slot::free);
  CHECK(std::any_of(r->server.log.begin(), r->server.log.end(), [](const std::string& l) {
    return has(l, "ghost [") && has(l, "never came");
  }));
}

TEST(sv_maxents_keeps_a_frame_to_its_cap_and_the_rest_wait) {
  for (const int most : {0, 1}) {
    cn::Q2NetConfig cfg;
    cfg.wire.max_entities = most;
    cfg.wire.prioritize = true;
    auto r = rig(cfg);
    join(*r, "eve");
    join(*r, "bob");
    run(*r, 300);
    CHECK(r->server.clients[0].stage == cn::Q2Slot::active);
    CHECK((r->server.clients[0].wire.capped > 0) == (most == 1));
  }
}

// THE OPERATOR'S WORDS of a modern server (porting/shooters-edge/q2-operator.md): the status
// letters, stuff and print to one or all, a client's setting asked, the stuff lists run at connect
// and at begin, partial names, kickban and killserver.
TEST(the_operators_words_reach_one_client_or_all_in_the_servers_words) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  CHECK(rcon(*r, "rcon letmein addstuffcmd connect echo hello there") == "print\n");
  CHECK(rcon(*r, "rcon letmein addstuffcmd begin echo welcome") == "print\n");
  CHECK(rcon(*r, "rcon letmein addstuffcmd begin echo welcome") ==
        "print\nStuffcmd already exists: echo welcome\n");
  CHECK(rcon(*r, "rcon letmein liststuffcmds begin") ==
        "print\nid command\n-- -------\n1  echo welcome\n");
  CHECK(rcon(*r, "rcon letmein liststuffcmds nowhere") ==
        "print\nUnknown stuffcmd list: nowhere\n");
  cn::Q2ClientEnd& eve = join(*r, "eve");
  eve.cfg.setting = [](const std::string& name) {
    return name == "version" ? std::string("q2 test 1.0") : name == "sensitivity" ? "3" : "";
  };
  cn::Q2ClientEnd& bob = join(*r, "Bobby");
  run(*r, 200);
  CHECK_MSG(eve.stage == cn::ClientStage::active && bob.stage == cn::ClientStage::active,
            eve.refused + bob.refused);
  const auto stuffed = [](const cn::Q2ClientEnd& c, const std::string& line) {
    return std::find(c.stuffed.begin(), c.stuffed.end(), line) != c.stuffed.end();
  };
  CHECK(stuffed(eve, "echo hello there") && stuffed(eve, "echo welcome"));
  // The version probe: its answer kept and listed.
  CHECK_MSG(has(rcon(*r, "rcon letmein status v"), "  1 eve             q2 test 1.0\n"),
            rcon(*r, "rcon letmein status v"));
  CHECK(r->server.clients[0].version_heard && r->server.clients[0].version == cn::kProgramVersion);
  CHECK(has(rcon(*r, "rcon letmein status x"), "Usage: status [d|l|p|s|t|v]\nCurrent map: hall"));
  CHECK(
      has(rcon(*r, "rcon letmein status p"), "  1 eve                34     0   1390  no   old\n"));
  CHECK(has(rcon(*r, "rcon letmein status l"), "PLs2c PLc2s Rmin Ravg Rmax dup scale"));
  CHECK(has(rcon(*r, "rcon letmein status t"), "num name            idle time\n"));
  CHECK(has(rcon(*r, "rcon letmein status d"), "num name            download"));
  // Stuff, print and a setting asked.
  rcon(*r, "rcon letmein stuff eve \"echo one; echo two\"");
  rcon(*r, "rcon letmein stuffall echo all");
  rcon(*r, "rcon letmein printall the server says hi");
  run(*r, 20);
  CHECK(stuffed(eve, "echo one") && stuffed(eve, " echo two") && stuffed(bob, "echo all"));
  CHECK(printed(eve, "the server says hi\n") && printed(bob, "the server says hi\n"));
  rcon(*r, "rcon letmein stuffcvar eve sensitivity");
  run(*r, 20);
  bool told = false;
  for (const std::string& line : r->server.log)
    told = told || has(line, "eve[sim:1:27901]: \"sensitivity\" is \"3\"");
  CHECK(told);
  CHECK(rcon(*r, "rcon letmein stuff eve") == "print\nUsage: stuff <userid> <raw text>\n");
  // Partial names only with sv_partialnames.
  CHECK(rcon(*r, "rcon letmein dumpuser bob") == "print\nUserid 'bob' is not on the server.\n");
  r->server.cfg.enhanced_setplayer = true;
  CHECK(has(rcon(*r, "rcon letmein dumpuser bob"), "name                 Bobby\n"));
  CHECK(has(rcon(*r, "rcon letmein dumpuser BOB"), "name                 Bobby\n"));
  CHECK(rcon(*r, "rcon letmein dumpuser zed") == "print\nNo clients matching 'zed' found.\n");
  // kickban: dropped, and his address refused.
  CHECK(has(rcon(*r, "rcon letmein kickban bob"), "Bobby[sim:2:27901] was kicked"));
  run(*r, 40);
  CHECK(printed(eve, "Bobby was kicked") && printed(bob, "Bobby was kicked"));
  CHECK(has(rcon(*r, "rcon letmein listip"), pattern_of(2)));
  // killserver: every client told and dropped; only the operator answered until the next map.
  rcon(*r, "rcon letmein killserver");
  run(*r, 20);
  CHECK(printed(eve, "Server was killed.") && eve.stage == cn::ClientStage::dropped);
  CHECK(r->server.killed && cn::q2_net_clients(r->server) == 0);
  CHECK(rcon(*r, "rcon letmein killserver") == "print\nNo server running.\n");
  CHECK(rcon(*r, "info 34").empty());  // the browsers' questions go unanswered
}

// A long rcon answer comes in packets of at most 1384 bytes after `print\n`, whole lines each.
TEST(a_long_rcon_answer_is_cut_into_packets_of_whole_lines) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  for (int i = 0; i < 60; i++)
    rcon(*r, "rcon letmein addstuffcmd connect echo line " + std::to_string(i) +
                 " of the welcome that goes on a while");
  const std::vector<uint8_t> b = cn::q2_oob("rcon letmein liststuffcmds connect");
  gn::transport_send(r->admin, gn::sim_address(0, cn::kPortQ2), b.data(), b.size(), r->now);
  run(*r, 8);
  std::vector<std::string> packets;
  uint8_t buf[2048];
  gn::Address from;
  for (int n; (n = gn::transport_receive(r->admin, buf, sizeof buf, from, r->now)) >= 0;)
    packets.push_back(cn::q2_oob_text(buf, static_cast<size_t>(n)));
  CHECK_MSG(packets.size() >= 2, std::to_string(packets.size()) + " packets");
  std::string whole;
  for (const std::string& p : packets) {
    CHECK(p.rfind("print\n", 0) == 0 && p.size() <= 6 + 1384 && p.back() == '\n');
    whole += p.substr(6);
  }
  CHECK(has(whole, "60 echo line 59 of the welcome"));
  CHECK(rcon(*r, "rcon letmein addstuffcmd begin echo hi") == "print\n");  // an empty answer
}

// THE PLAYER'S QUERIES: info, sinfo, lag, packetdup, nogamedata, download http.
TEST(a_player_asks_the_server_for_its_info_his_connection_and_another_players_lag) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  cfg.packetdup_most = 1;
  auto r = rig(cfg);
  cn::Q2ClientEnd& eve = join(*r, "eve");
  join(*r, "Bobby");
  run(*r, 200);
  cn::q2_client_command(eve, "info");
  cn::q2_client_command(eve, "sinfo");
  cn::q2_client_command(eve, "lag bob");
  cn::q2_client_command(eve, "lag nobody");
  run(*r, 20);
  CHECK(printed(eve, "hostname             Shooter's Edge\n"));
  CHECK(printed(eve, "maxclients           8\n"));
  CHECK(printed(eve, "protocol (maj/min)   34/0\n") && printed(eve, "connection time      00.0"));
  CHECK(printed(eve, "Lag stats for:       Bobby\nRTT (min/avg/max):   "));
  CHECK(printed(eve, "Timescale          : "));
  CHECK(printed(eve, "No clients matching 'nobody' found.\n"));
  cn::q2_client_command(eve, "packetdup 2");
  cn::q2_client_command(eve, "packetdup 1");
  run(*r, 20);
  CHECK(printed(eve, "Packetdup of 2 is not allowed on this server.\n"));
  CHECK(printed(eve, "Server is sending 1 duplicate packet to you.\n"));
  CHECK(r->server.clients[0].packet_dup == 1);
  const int64_t before = r->server.clients[0].frames;
  cn::q2_client_command(eve, "nogamedata");
  run(*r, 40);
  CHECK(r->server.clients[0].no_game_data && r->server.clients[0].frames - before <= 2);
  cn::q2_client_command(eve, "nogamedata");
  run(*r, 200);  // the command waits for the last one's acknowledgement, a second away
  CHECK_MSG(r->server.clients[0].frames - before > 2,
            std::to_string(r->server.clients[0].frames - before));
  CHECK(eve.stage == cn::ClientStage::active);
}

// THE USER INFO a modern server holds a client to: refusals at the connect, a name kept when
// a change would empty it, the name cut to 15, and the drops a timeout and a reconnect tell.
TEST(a_user_info_is_held_to_the_strict_rule_and_a_name_is_kept) {
  cn::Q2NetConfig cfg;
  cfg.rcon_password = "letmein";
  cfg.timeout = 3;
  auto r = rig(cfg);
  cn::Q2ClientEnd& blank = join(*r, "   ");
  cn::Q2ClientEnd& odd = join(*r, "a;b");
  cn::Q2ClientEnd& eve = join(*r, "eve with a very long name");
  cn::Q2ClientEnd& bob = join(*r, "bob");
  run(*r, 200);
  CHECK_MSG(has(blank.refused, "Please set your name before connecting."), blank.refused);
  CHECK_MSG(has(odd.refused, "Malformed userinfo string."), odd.refused);
  CHECK(eve.stage == cn::ClientStage::active && bob.stage == cn::ClientStage::active);
  CHECK(r->server.clients[0].name == "eve with a very");
  bob.cfg.name = "";
  cn::q2_client_userinfo(bob);
  run(*r, 20);
  CHECK(printed(bob, "You can't have an empty name.\n"));
  CHECK(r->server.clients[1].name == "bob" && bob.stage == cn::ClientStage::active);
  // Silence past `timeout`: the slot freed at once, everyone told.
  r->clients.pop_back();  // bob goes quiet
  run(*r, 300);
  CHECK(printed(eve, "bob timed out\n"));
  CHECK(r->server.clients[1].stage == cn::Q2Slot::free);
}

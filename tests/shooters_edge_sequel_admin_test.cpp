// games/shooters_edge/net/sequel_admin.hpp: the sequel server's administrator words over its remote
// console. No password and a wrong one are refused alike; `status` lists the clients; a rules
// line runs on the rules' console; `kick` and `dumpuser` take a slot or a name and answer a bad
// one in the server's words; `addip` drops a client it comes to refuse and refuses its next
// connect ("Banned."); `writeip` saves the list and the server reads it back; a banned address's
// remote console is not answered.
//
// ge:summary Test: the sequel server's rcon, status, kick, dumpuser and ban list over the sim net
// ge:keywords test sequel admin rcon rcon_password status kick dumpuser addip writeip listip
// ge:keywords banned filterban sv_banmode ban_file_read
// ge:skill net shooter
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/bans.hpp"
#include "shooters_edge/net/sequel_admin.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

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
  cn::SequelNetServer server;
  std::vector<std::unique_ptr<cn::SequelClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::SequelNetConfig& cfg) {
  auto r = std::make_unique<Rig>();
  r->rules = sv::sequel_runner(sv::make_sequel_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset("lan", link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortSequel));
  gn::open_sim_transport(r->admin, r->net, gn::sim_address(9, 27960));
  CHECK_MSG(cn::sequel_net_start(r->server, r->rules, r->st, cfg, error), error);
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
  }
}

// An administrator's out-of-band line, and the server's answer ("" when none came).
std::string rcon(Rig& r, const std::string& text) {
  const std::vector<uint8_t> b = cn::sequel_oob(text);
  gn::transport_send(r.admin, gn::sim_address(0, cn::kPortSequel), b.data(), b.size(), r.now);
  run(r, 8);
  std::string answer;
  uint8_t buf[2048];
  gn::Address from;
  for (int n; (n = gn::transport_receive(r.admin, buf, sizeof buf, from, r.now)) >= 0;)
    answer += cn::sequel_oob_text(buf, static_cast<size_t>(n));
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

bool printed(const cn::SequelClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (has(p, part)) return true;
  return has(c.refused, part);
}

}  // namespace

TEST(a_remote_console_without_the_password_is_refused) {
  auto none = rig({});
  CHECK(rcon(*none, "rcon anything status") == "print\nBad rcon_password.\n");
  cn::SequelNetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  CHECK(rcon(*r, "rcon wrong status") == "print\nBad rcon_password.\n");
  CHECK(r->server.rcon_refused == 1 && r->server.rcon_runs == 0);
  for (const std::string& line : r->server.log) CHECK_MSG(!has(line, "wrong"), line);
}

TEST(status_lists_the_clients_and_a_rules_line_runs_on_their_console) {
  cn::SequelNetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  join(*r, "eve");
  join(*r, "bob");
  run(*r, 200);
  const std::string status = rcon(*r, "rcon letmein status");
  CHECK_MSG(has(status, "map              : hall") && has(status, " eve ") && has(status, " bob "),
            status);
  rcon(*r, "rcon letmein fraglimit 7");
  const ge::console::Cvar* frags = r->rules->cvars.find("fraglimit");
  CHECK(frags != nullptr && frags->value == "7");
  CHECK(has(rcon(*r, "rcon letmein nosuchword"), "Unknown command \"nosuchword\""));
  CHECK(r->server.rcon_runs == 3);
  for (const std::string& line : r->server.log) CHECK_MSG(!has(line, "letmein"), line);
}

TEST(kick_and_dumpuser_take_a_slot_or_a_name) {
  cn::SequelNetConfig cfg;
  cfg.rcon_password = "letmein";
  auto r = rig(cfg);
  cn::SequelClientEnd& eve = join(*r, "eve");
  cn::SequelClientEnd& bob = join(*r, "bob");
  run(*r, 200);
  CHECK_MSG(eve.stage == cn::ClientStage::active && bob.stage == cn::ClientStage::active,
            eve.refused + bob.refused);
  CHECK(has(rcon(*r, "rcon letmein dumpuser bob"), "name                bob"));
  CHECK(rcon(*r, "rcon letmein kick 99") == "print\nBad client slot: 99\n");
  CHECK(rcon(*r, "rcon letmein kick 5") == "print\nClient 5 is not active\n");
  CHECK(rcon(*r, "rcon letmein kick nobody") == "print\nUserid nobody is not on the server\n");
  CHECK(rcon(*r, "rcon letmein kick bob") == "print\nbob was kicked\n");
  run(*r, 40);
  CHECK(bob.stage == cn::ClientStage::dropped);
  CHECK(printed(bob, "You were kicked from the game"));
  CHECK(printed(eve, "bob was kicked"));
  CHECK(eve.stage == cn::ClientStage::active);
}

TEST(a_banned_address_is_dropped_refused_and_not_answered) {
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / "ge_sequel_admin_listip.cfg";
  std::filesystem::remove(file);
  cn::SequelNetConfig cfg;
  cfg.rcon_password = "letmein";
  cfg.ban_file = file.string();
  auto r = rig(cfg);
  cn::SequelClientEnd& eve = join(*r, "eve");  // node 1
  run(*r, 200);
  CHECK(eve.stage == cn::ClientStage::active);
  CHECK(has(rcon(*r, "rcon letmein addip " + pattern_of(1)), "added"));
  run(*r, 40);
  CHECK(eve.stage == cn::ClientStage::dropped);
  CHECK(has(rcon(*r, "rcon letmein writeip"), "Saved 1 addresses"));
  cn::SequelClientEnd& again = join(*r, "eve");  // node 2: let in
  run(*r, 200);
  CHECK_MSG(again.stage == cn::ClientStage::active, again.refused);

  // A server started again reads the file back, and refuses node 1's connect.
  auto second = rig(cfg);
  gs::ban_file_read(second->server.bans, cfg.ban_file, 0);
  CHECK(second->server.bans.entries.size() == 1);
  cn::SequelClientEnd& banned = join(*second, "eve");
  run(*second, 80);
  CHECK(banned.stage == cn::ClientStage::dropped);
  CHECK(banned.refused == "Banned.\n");

  // A remote console from a banned address gets no answer; `sv_banmode` 0 lets in only the list.
  CHECK(has(rcon(*second, "rcon letmein addip " + pattern_of(9)), "added"));
  CHECK(rcon(*second, "rcon letmein status").empty());
  cfg.ban_mode = 0;
  auto only = rig(cfg);
  gs::ban_file_read(only->server.bans, cfg.ban_file, 0);
  cn::SequelClientEnd& listed = join(*only, "eve");    // node 1, on the list
  cn::SequelClientEnd& unlisted = join(*only, "bob");  // node 2
  run(*only, 200);
  CHECK_MSG(listed.stage == cn::ClientStage::active, listed.refused);
  CHECK(unlisted.refused == "Banned.\n");
  std::filesystem::remove(file);
}

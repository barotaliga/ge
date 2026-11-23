// Tests: Quake II's server rows that no other test reached, each checked against a joined client
// on the simulated network (games/shooters_edge/net/q2_net_server.hpp): `stopdl` ends a download
// half sent, a rename is logged on the server's console and told to the players, a client whose
// moves ask for more time than passes is warned and then dropped for its time skew, and a connect
// whose user info grows past the limit with the address added is refused in so many words.
//
// ge:summary Tests: Quake II server rows checked end to end against joined clients
// ge:keywords test q2 server stopdl rename-log time-skew sv_speedkick oversize-userinfo
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
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
  br::Entity spawn;
  spawn.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}};
  w.entities.push_back(spawn);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

// Our server on the simulated network; the clients joined to it.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::Q2NetServer server;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  int64_t now = 0;
  int64_t client_now = 0;  // the clients' own clock (time skew runs it ahead)
  int64_t client_step = 13000;
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
  r->net = gn::make_sim_network(13, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

cn::Q2ClientEnd& join(Rig& r, cn::Q2ClientConfig cfg) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net, gn::sim_address(uint32_t(n + 1), 27901));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::q2_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortQ2), cfg,
                      static_cast<double>(r.client_now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    r.client_now += r.client_step;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::q2_client_tick(*c, r.client_now);
    cn::q2_net_frame(r.server, r.now);
  }
}

bool logged(const Rig& r, const std::string& part) {
  for (const std::string& line : r.server.log)
    if (line.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

// `stopdl`: a download asked for and half sent stops; the client is told it ended, the server
// sends no more of it and says so on its console.
TEST(stopdl_ends_a_download_half_sent) {
  cn::Q2NetConfig cfg;
  cfg.read_file = [](const std::string&, std::vector<uint8_t>* out) {
    out->assign(400000, uint8_t{7});  // far more than a few packets carry
    return true;
  };
  auto r = rig(cfg);
  cn::Q2ClientEnd& c = join(*r, {});
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  cn::q2_client_command(c, "download sound/big.wav");
  run(*r, 5);
  CHECK(r->server.clients[0].downloading);
  cn::q2_client_command(c, "stopdl");
  run(*r, 10);
  CHECK(!r->server.clients[0].downloading);
  CHECK(logged(*r, "stopped a download"));
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
}

// A rename while in the game: the server's console logs it with the address, and with
// sv_shownames 2 the players are told.
TEST(a_rename_is_logged_on_the_console_and_told_to_the_players) {
  cn::Q2NetConfig cfg;
  cfg.names.show = 2;
  auto r = rig(cfg);
  cn::Q2ClientConfig a;
  a.name = "eve";
  cn::Q2ClientEnd& eve = join(*r, a);
  cn::Q2ClientConfig b;
  b.name = "bob";
  cn::Q2ClientEnd& bob = join(*r, b);
  run(*r, 150);
  CHECK_MSG(eve.stage == cn::ClientStage::active && bob.stage == cn::ClientStage::active,
            eve.refused + bob.refused);
  eve.cfg.name = "mallory";
  cn::q2_client_userinfo(eve);
  run(*r, 20);
  CHECK(logged(*r, "eve [") && logged(*r, "] renamed mallory"));
  bool told = false;
  for (const std::string& p : bob.prints) told = told || p.find("mallory") != std::string::npos;
  CHECK_MSG(told, "the other player was not told the new name");
}

// Time skew: a client whose moves ask for half again as much time as passes is warned on the
// console, then dropped for it once past the kick ratio.
TEST(a_client_asking_for_more_time_than_passes_is_warned_then_dropped) {
  cn::Q2NetConfig cfg;
  cfg.skew.window_s = 4;
  cfg.skew.warn = 1.2;
  cfg.skew.kick = 1.4;
  auto r = rig(cfg);
  cn::Q2ClientConfig cc;
  cc.brain = [](int) {
    sh::MoveCmd m;
    m.forward = 200;
    return m;
  };
  cn::Q2ClientEnd& c = join(*r, cc);
  run(*r, 120);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(!logged(*r, "time skew"));
  r->client_step = 13000 * 3 / 2;  // its clock runs half again as fast as the server's
  run(*r, 1200);
  CHECK(logged(*r, "time skew"));
  CHECK(logged(*r, "time skew too high"));
  CHECK(cn::q2_net_clients(r->server) == 0);
}

// A connect whose user info, with the address the server adds, reaches 512 bytes is refused with
// "Oversize userinfo string."
TEST(a_connect_with_too_long_a_user_info_is_refused) {
  auto r = rig({});
  gn::Transport t;
  gn::open_sim_transport(t, r->net, gn::sim_address(9, 27901));
  const gn::Address server = gn::sim_address(0, cn::kPortQ2);
  const auto send = [&](const std::string& text) {
    const std::string packet = "\xff\xff\xff\xff" + text;
    gn::transport_send(t, server, reinterpret_cast<const uint8_t*>(packet.data()), packet.size(),
                       r->now);
  };
  std::vector<std::string> heard;
  const auto listen = [&](int ticks) {
    for (int i = 0; i < ticks; i++) {
      run(*r, 1);
      uint8_t buf[2048];
      gn::Address from;
      for (int n; (n = gn::transport_receive(t, buf, sizeof buf, from, r->now)) >= 0;)
        heard.emplace_back(reinterpret_cast<const char*>(buf) + 4, static_cast<size_t>(n) - 4);
    }
  };
  send("getchallenge\n");
  listen(10);
  CHECK(!heard.empty() && heard.back().starts_with("challenge "));
  if (heard.empty()) return;
  const std::string challenge = heard.back().substr(10, heard.back().find_first_of(" \n", 10) - 10);
  std::string info = "\\name\\eve";
  for (int k = 0; info.size() < 505; k++) info += "\\k" + std::to_string(k) + "\\0123456789";
  send("connect 34 7 " + challenge + " \"" + info + "\"\n");
  listen(10);
  CHECK_MSG(!heard.empty() && heard.back() == "print\nOversize userinfo string.\n",
            heard.empty() ? std::string("nothing heard") : heard.back());
  CHECK(cn::q2_net_clients(r->server) == 0);
}

// sv_force_reconnect: a newcomer is sent random settings and the address and dropped without a
// word; our client runs them, connects again, brings the setting back and plays.
TEST(a_forced_reconnect_is_made_and_the_client_plays) {
  cn::Q2NetConfig cfg;
  cfg.force_reconnect = gn::to_string(gn::sim_address(0, cn::kPortQ2));
  auto r = rig(cfg);
  cn::Q2ClientEnd& c = join(*r, {});
  run(*r, 400);
  CHECK_MSG(c.stage == cn::ClientStage::active, c.refused);
  CHECK(r->server.clients[0].reconnected);
  CHECK(cn::q2_net_clients(r->server) == 1);
}

// `pickclient <address:port>`: the operator's server reaches out to a client waiting for it
// (`passive_connect`); an address with no port or that does not read is told, and a single-player
// server refuses.
TEST(pickclient_calls_a_waiting_client_and_tells_a_bad_address) {
  cn::Q2NetConfig cfg;
  auto r = rig(cfg);
  gn::Transport waiting;
  gn::open_sim_transport(waiting, r->net, gn::sim_address(9, 27910));
  std::string out;
  CHECK(cn::q2_admin(r->server, "pickclient " + gn::to_string(gn::sim_address(9, 27910)), out));
  CHECK_MSG(out.empty(), out);
  run(*r, 5);
  uint8_t buf[256];
  gn::Address from;
  const int n = gn::transport_receive(waiting, buf, sizeof buf, from, r->now);
  CHECK(n > 4 && std::string(reinterpret_cast<const char*>(buf) + 4, static_cast<size_t>(n) - 4) ==
                     "passive_connect\n");
  CHECK(cn::q2_admin(r->server, "pickclient", out) && out == "Usage: pickclient <address>\n");
  CHECK(cn::q2_admin(r->server, "pickclient not..an..address", out) &&
        out == "Bad client address: not..an..address\n");
  cn::Q2NetConfig single;
  single.max_clients = 1;
  auto one = rig(single);
  CHECK(cn::q2_admin(one->server, "pickclient 10.0.0.1:27901", out) &&
        out == "Single player server running.\n");
}

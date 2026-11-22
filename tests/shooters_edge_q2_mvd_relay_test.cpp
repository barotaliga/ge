// games/shooters_edge/net/q2_mvd_relay.hpp: Quake II's multiview relay. A recording our server
// made plays on a channel (`mvdplay`) and a stock client joins the relay: it lands in the Waiting
// Room, goes on to the only channel and watches the match; its words answer as the relay's do
// (the list of commands, chat among the channel's viewers and its flood rule, admin, follow, the
// switch rules) and the operator's words list, pause, skip and end it. A relay dials our live
// server's recorder over TCP (`mvdconnect`), says hello, starts the stream and lists its channel;
// a wrong password destroys the connection in the server's words. A viewer's new name and leaving
// are told to the channel, and `mvd_filter_version` hides the version trigger in the players' chat.
//
// ge:summary Test: Quake II's multiview relay: mvdplay, mvdconnect, the Waiting Room, viewers
// ge:keywords test mvd-relay mvdplay mvdconnect waiting-room channel viewer chat flood follow
// ge:skill net shooter testing
#include <cstdint>
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
#include "net/tcp_socket.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_mvd_relay.hpp"
#include "shooters_edge/net/q2_mvd_server.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto box = [](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
  };
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  br::Entity spot;
  spot.pairs = {
      {"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}};
  w.entities.push_back(spot);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

// Our server (node 0) with a walking player (node 1) and its recorder; the relay on node 5;
// viewers from node 6 up.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct, rt;
  cn::Q2NetServer server;
  cn::Q2ClientEnd player;
  cn::Q2Relay relay;
  std::vector<std::unique_ptr<gn::Transport>> viewer_ports;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> viewers;
  std::vector<std::string> log;
  int64_t now = 0;
};

sh::MoveCmd walk(int tick) {
  sh::MoveCmd m;
  m.forward = 300;
  m.angles[1] = static_cast<float>(tick % 360);
  return m;
}

std::unique_ptr<Rig> rig(const cn::Q2MvdConfig& mvd, const std::filesystem::path& demos) {
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
  cn::Q2NetConfig cfg;
  cfg.mvd = mvd;
  cfg.mvd.demos = demos.string();
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  gn::open_sim_transport(r->ct, r->net, gn::sim_address(1, 27901));
  cn::Q2ClientConfig cc;
  cc.qport = 101;
  cc.name = "runner";
  cc.brain = walk;
  cn::q2_client_start(r->player, r->ct, gn::sim_address(0, cn::kPortQ2), cc, 0);
  gn::open_sim_transport(r->rt, r->net, gn::sim_address(5, 27910));
  r->relay.cfg.demos = demos.string();
  r->relay.cfg.suspend_min = 0;
  r->relay.cfg.wait_delay = 1;
  Rig* raw = r.get();
  r->relay.log = [raw](const std::string& line) { raw->log.push_back(line); };
  r->relay.spot = [](const std::string& name, float origin[3], float angles[3], int32_t& checksum,
                     std::string& why) {
    if (name != "q2dm1") {
      why = "not found";
      return false;
    }
    origin[0] = 100;
    origin[1] = 200;
    origin[2] = 300;
    angles[1] = 90;
    checksum = 1234;
    return true;
  };
  return r;
}

cn::Q2ClientEnd& watch(Rig& r, const std::string& name) {
  const auto n = static_cast<uint32_t>(r.viewers.size());
  r.viewer_ports.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.viewer_ports.back(), r.net, gn::sim_address(6 + n, 27901));
  r.viewers.push_back(std::make_unique<cn::Q2ClientEnd>());
  cn::Q2ClientConfig cfg;
  cfg.name = name;
  cfg.qport = static_cast<uint16_t>(200 + n);
  cn::q2_client_start(*r.viewers.back(), *r.viewer_ports.back(), gn::sim_address(5, 27910), cfg,
                      static_cast<double>(r.now) / 1e6);
  return *r.viewers.back();
}

void run(Rig& r, int ticks, bool game = true) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    if (game) {
      cn::q2_client_tick(r.player, r.now);
      cn::q2_net_frame(r.server, r.now);
    }
    for (auto& v : r.viewers) cn::q2_client_tick(*v, r.now);
    cn::q2_relay_frame(r.relay, r.now);
  }
}

std::string admin(Rig& r, const std::string& line) {
  std::string out;
  CHECK_MSG(cn::q2_admin(r.server, line, out), line);
  return out;
}

std::string word(Rig& r, const std::string& line) {
  std::string out;
  const std::vector<std::string> w = cn::q2_words(line);
  CHECK_MSG(cn::q2_relay_command(r.relay, w, line, &r.rt, double(r.now) / 1e6, out), line);
  return out;
}

bool logged(const Rig& r, const std::string& line) {
  for (const std::string& l : r.log)
    if (l == line) return true;
  return false;
}

bool printed(const cn::Q2ClientEnd& c, const std::string& text) {
  for (const std::string& p : c.prints)
    if (p.find(text) != std::string::npos) return true;
  return false;
}

std::string hi(std::string s) {
  for (char& ch : s) ch = static_cast<char>(static_cast<uint8_t>(ch) | 0x80);
  return s;
}

// A recording of the player walking, made by our server's recorder.
std::filesystem::path record(const std::filesystem::path& dir) {
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  cn::Q2MvdConfig cfg;
  cfg.enable = 1;
  auto r = rig(cfg, dir);
  run(*r, 100);
  CHECK(r->server.clients[0].stage == cn::Q2Slot::active);
  admin(*r, "mvdrecord duel");
  run(*r, 100);
  cn::q2_client_command(r->player, "say !version please");
  run(*r, 1100);  // about 15 s in all
  admin(*r, "mvdstop");
  return dir / "duel.mvd2";
}

}  // namespace

TEST(the_operators_words_answer_before_anything_is_relayed) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_relay_words";
  auto r = rig({}, dir);
  CHECK(word(*r, "mvdconnect") ==
        "Missing address argument.\nTry 'mvdconnect --help' for more information.\n");
  CHECK(word(*r, "mvdconnect -h").rfind("Usage: mvdconnect [-hn:u:p:] <address[:port]>\n", 0) == 0);
  CHECK(word(*r, "mvdconnect what") == "Bad server address: what\n");
  CHECK(word(*r, "mvdchannels") == "No MVD channels.\n");
  CHECK(word(*r, "mvdservers") == "No GTV connections.\n");
  CHECK(word(*r, "mvdkill") == "No active channels.\n");
  CHECK(word(*r, "mvdisconnect") == "No GTV connections.\n");
  CHECK(word(*r, "mvdskip") == "Usage: mvdskip [chan_id] [count]\n");
  CHECK(word(*r, "mvdseek") == "Usage: mvdseek [+-]<timespec|percent>[%] [chanid]\n");
  CHECK(word(*r, "mvdcontrol") ==
        "At least one option needed.\nTry 'mvdcontrol --help' for more information.\n");
  CHECK(word(*r, "mvdplay nothing") == "Couldn't open nothing: No such file or directory\n");
  CHECK(!r->relay.spawned);
}

TEST(a_recording_plays_on_a_channel_and_a_stock_client_watches_it) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_relay_play";
  record(dir);
  auto r = rig({}, dir);
  CHECK(word(*r, "mvdplay -l 0 duel").empty());
  CHECK(r->relay.spawned);
  CHECK(logged(*r, "[dem0] -=- Reading from " + (dir / "duel.mvd2").string()));
  cn::Q2ClientEnd& v = watch(*r, "watcher");
  run(*r, 400, false);
  CHECK_MSG(v.stage == cn::ClientStage::active, v.refused + " " + v.error);
  CHECK_MSG(v.error.empty(), v.error);
  CHECK_MSG(v.demo.state.map == "hall", v.demo.state.map);  // moved on from the Waiting Room
  CHECK(v.demo.last_frame > 0);
  const std::string list = word(*r, "mvdchannels");
  CHECK_MSG(list.find(" 0 dem0         hall       1   1 READ") != std::string::npos, list);
  // The viewer's words.
  cn::q2_client_command(v, "commands");
  cn::q2_client_command(v, "!mvdadmin guess");
  cn::q2_client_command(v, "follow 40");
  cn::q2_client_command(v, "follow nobody");
  cn::q2_client_command(v, "join 0");
  run(*r, 40, false);
  CHECK(printed(v, "join [channel_id]      join specified channel"));
  CHECK(printed(v, "[MVD] Invalid password."));
  CHECK(printed(v, "[MVD] Player slot number 40 is invalid."));
  CHECK(printed(v, "[MVD] No players matching 'nobody' found."));
  CHECK(printed(v, "[MVD] You are already on this channel."));
  cn::q2_client_command(v, "follow run");
  run(*r, 300, false);
  CHECK(printed(v, "[MVD] Chasing runner."));
  CHECK_MSG(v.demo.state.map == "hall", v.demo.state.map);  // the view again, on the player
  bool chasing = false;
  for (const auto& [key, rec] : r->relay.viewers)
    chasing |= rec.name == "watcher" && rec.target == 0;
  CHECK(chasing);
  // A second viewer: chat among the channel's viewers, then the flood rule.
  cn::Q2ClientEnd& w = watch(*r, "second");
  run(*r, 400, false);
  CHECK_MSG(w.demo.state.map == "hall", w.demo.state.map);
  for (int i = 0; i < 6; i++) cn::q2_client_command(v, "say hello " + std::to_string(i));
  run(*r, 60, false);
  CHECK(printed(w, hi("[MVD] watcher: hello 0")));
  CHECK(printed(w, hi("[MVD] watcher: hello 4")));
  CHECK(!printed(w, hi("[MVD] watcher: hello 5")));
  CHECK(printed(v, "[MVD] You can't talk for 10 seconds."));
  // Back to the Waiting Room, its map and its chooser.
  cn::q2_client_command(w, "leave");
  run(*r, 20, false);
  cn::q2_client_command(w, "join dem0");
  run(*r, 300, false);
  CHECK(printed(w, "[MVD] You may not switch channels too soon."));
  CHECK_MSG(w.demo.state.map == "q2dm1", w.demo.state.map);
  CHECK(w.demo.state.layout.find("Channel Chooser") != std::string::npos);
  // The operator's words on the channel.
  CHECK(word(*r, "mvdpause").empty());
  CHECK(r->relay.channels[0]->paused);
  CHECK(word(*r, "mvdskip 0 2") == "[dem0] -=- Skipping maps...\n");
  CHECK(word(*r, "mvdcontrol -n match") == "[dem0] Channel renamed to match.\n");
  CHECK(word(*r, "mvdkill") == "");
  CHECK(logged(*r, "[match] =X= Channel was killed."));
  run(*r, 300, false);
  CHECK_MSG(v.demo.state.map == "q2dm1", v.demo.state.map);  // sent to the Waiting Room
  cn::q2_relay_stop(r->relay);
  std::filesystem::remove_all(dir);
}

TEST(a_viewers_new_name_and_leaving_are_told_and_the_version_trigger_hidden) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_relay_names";
  record(dir);
  auto r = rig({}, dir);
  r->relay.cfg.filter_version = true;
  word(*r, "mvdplay -l 0 duel");
  cn::Q2ClientEnd& v = watch(*r, "watcher");
  cn::Q2ClientEnd& w = watch(*r, "second");
  run(*r, 600, false);
  CHECK_MSG(w.demo.state.map == "hall", w.demo.state.map);
  w.cfg.name = "third";
  cn::q2_client_userinfo(w);
  run(*r, 40, false);
  CHECK(printed(v, "[MVD] second changed name to third"));
  cn::q2_client_stop(w);
  run(*r, 40, false);
  CHECK(printed(v, "[MVD] third disconnected"));
  run(*r, 800, false);  // the recording's chat, its trigger made inert
  CHECK(printed(v, "!versi0n please"));
  CHECK(!printed(v, "!version please"));
  cn::q2_relay_stop(r->relay);
  std::filesystem::remove_all(dir);
}

TEST(a_play_list_ends_and_its_channel_goes) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_q2_relay_end";
  record(dir);
  auto r = rig({}, dir);
  word(*r, "mvdplay -n once duel");
  run(*r, 2000, false);  // the whole recording and more
  CHECK(logged(*r, "[once] =X= End of play list reached"));
  CHECK(r->relay.channels.empty());
  std::filesystem::remove_all(dir);
}

TEST(a_relay_dials_a_live_servers_recorder_and_lists_its_channel) {
  cn::Q2MvdConfig m;
  m.enable = 2;
  m.suspend_min = 0;
  auto r = rig(m, std::filesystem::temp_directory_path());
  CHECK(cn::q2_mvd_listen(r->server.mvd, gn::loopback_v4(0)));
  run(*r, 100);
  const std::string at = "127.0.0.1:" + std::to_string(r->server.mvd.listener.local.port);
  CHECK(word(*r, "mvdconnect " + at) == "[net0] -=- Connecting to " + at + "...\n");
  CHECK(word(*r, "mvdconnect " + at) == "[net0] <warn> Connection to " + at + " already exists.\n");
  for (int i = 0; i < 600 && r->relay.channels.empty(); i++) run(*r, 1);
  run(*r, 300);
  CHECK(logged(*r, "[net0] -=- Connected to the game server!"));
  CHECK(logged(*r, "[net0] -=- Sending client hello..."));
  CHECK(logged(*r, "[net0] -=- Server hello done."));
  CHECK(logged(*r, "[net0] -=- Sending stream start request..."));
  CHECK(logged(*r, "[net0] -=- Stream start ack received."));
  CHECK(logged(*r, "[net0] -=- Waiting finished, reading..."));
  CHECK(r->relay.spawned);
  const std::string list = word(*r, "mvdchannels");
  CHECK_MSG(list.find(" 0 net0         hall") != std::string::npos, list);
  const std::string servers = word(*r, "mvdservers");
  CHECK_MSG(servers.find(" 0 net0         reading       100%") != std::string::npos, servers);
  // A viewer watches the live match through it.
  cn::Q2ClientEnd& v = watch(*r, "fan");
  run(*r, 400);
  CHECK_MSG(v.demo.state.map == "hall", v.demo.state.map);
  CHECK(v.demo.last_frame > 0);
  CHECK(word(*r, "mvdisconnect") == "[net0] =X= Connection destroyed.\n");
  cn::q2_relay_stop(r->relay);
}

TEST(a_wrong_password_destroys_the_connection) {
  cn::Q2MvdConfig m;
  m.enable = 2;
  m.suspend_min = 0;
  m.password = "sesame";
  auto r = rig(m, std::filesystem::temp_directory_path());
  CHECK(cn::q2_mvd_listen(r->server.mvd, gn::loopback_v4(0)));
  const std::string at = "127.0.0.1:" + std::to_string(r->server.mvd.listener.local.port);
  word(*r, "mvdconnect -p guess " + at);
  for (int i = 0; i < 600 && !r->relay.conns.empty(); i++) run(*r, 1);
  CHECK(
      logged(*r, "[net0] =X= You don't have permission to access MVD/GTV stream on this server."));
  CHECK(r->relay.conns.empty());
}

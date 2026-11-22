// Tests: Quake II's dedicated server recorded by the server core (engine/server/record.hpp) and
// played back (engine/server/playback.hpp) through its broadcast and watch hooks
// (games/shooters_edge/net/q2_game_tv.hpp). Two clients of Quake II play a hall served on the
// simulated network while the core records it with a keyframe every 2 s; played back, every slot's
// view equals the live server's at every frame, from the start and after a seek into the middle.
//
// ge:summary Tests: Quake II's dedicated server recorded and played back from every view
// ge:keywords test q2-game record playback player-view seek server-record q2_player_view
// ge:skill net replay testing
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "net/sim_network.hpp"
#include "server/playback.hpp"
#include "server/record.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_game.hpp"
#include "shooters_edge/net/q2_net.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = ge::server;
namespace cn = shooters_edge::net;

constexpr int kSlots = 8;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall written as <root>/baseq2/maps/hall.bsp; the root.
std::string hall_folder() {
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
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "ge_q2_record_server_test";
  std::filesystem::create_directories(root / "baseq2" / "maps");
  std::ofstream(root / "baseq2" / "maps" / "hall.bsp", std::ios::binary)
      .write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return root.string();
}

sh::MoveCmd forward(int tick) {
  sh::MoveCmd m;
  m.forward = 300;
  m.angles[1] = static_cast<float>(tick % 360);
  return m;
}

sh::MoveCmd strafe(int tick) {
  sh::MoveCmd m;
  m.side = (tick / 40) % 2 == 0 ? 300 : -300;
  m.angles[0] = static_cast<float>((tick % 60) - 30);
  return m;
}

}  // namespace

TEST(a_q2_match_plays_back_as_every_player_saw_it) {
  sv::ServerConfig c;
  c.root = hall_folder();
  c.map = "hall";
  c.maxclients = kSlots;
  c.seed = 3;
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(3, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortQ2));
  sv::Server s;
  std::string error;
  const sv::ServerGame& g = cn::q2_game();
  CHECK_MSG(sv::server_open(s, g, c, &port, nullptr, error), error);
  if (!s.state) return;
  std::vector<std::unique_ptr<gn::Transport>> ports;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  for (int n = 0; n < 2; n++) {
    ports.push_back(std::make_unique<gn::Transport>());
    gn::open_sim_transport(*ports.back(), net, gn::sim_address(uint32_t(n + 1), 27901));
    clients.push_back(std::make_unique<cn::Q2ClientEnd>());
    cn::Q2ClientConfig cc;
    cc.name = n == 0 ? "runner" : "dodger";
    cc.brain = n == 0 ? forward : strafe;
    cc.qport = static_cast<uint16_t>(100 + n);
    cn::q2_client_start(*clients.back(), *ports.back(), gn::sim_address(0, cn::kPortQ2), cc,
                        static_cast<double>(s.now_us) / 1e6);
  }
  const auto step = [&] {
    gn::sim_advance(net, s.now_us);
    for (auto& cl : clients) cn::q2_client_tick(*cl, s.now_us);
    sv::server_frame(s);
  };
  for (int f = 0; f < 200; f++) step();  // the clients join and play
  sv::Recording rec;
  rec.keyframe_seconds = 2;
  CHECK_MSG(sv::record_begin(rec, s, error), error);
  std::vector<std::vector<std::vector<uint8_t>>> live;
  int seen = 0;
  const auto views = [&] {
    std::vector<std::vector<uint8_t>> row(kSlots);
    for (int p = 0; p < kSlots; p++) {
      g.player_view(s.state, p, row[static_cast<size_t>(p)]);
      seen += row[static_cast<size_t>(p)].size() > 1 ? 1 : 0;
    }
    live.push_back(std::move(row));
  };
  views();
  const int frames = static_cast<int>(7.0 / s.frame_s);
  for (int f = 0; f < frames; f++) {
    step();
    sv::record_frame(rec, s);
    views();
  }
  sv::record_finish(rec, s);
  sv::server_close(s);
  CHECK_MSG(seen >= 2 * frames, std::to_string(seen) + " views of a player in play");

  sv::Playback pb;
  CHECK_MSG(sv::playback_open(pb, g, rec.bytes, error), error);
  CHECK_MSG(static_cast<int>(pb.frames.size()) == frames,
            std::to_string(pb.frames.size()) + " frames");
  CHECK(pb.keys.size() >= 3);
  int wrong = 0;
  const auto same = [&](int64_t frame) {
    for (int p = 0; p < kSlots; p++) {
      std::vector<uint8_t> v;
      sv::playback_view(pb, p, v);
      wrong += v != live[static_cast<size_t>(frame)][static_cast<size_t>(p)] ? 1 : 0;
    }
  };
  same(0);
  while (sv::playback_step(pb, error)) same(pb.at);
  CHECK_MSG(error.empty(), error);
  CHECK_MSG(wrong == 0, std::to_string(wrong) + " views differ from the live server's");
  for (const int64_t at :
       {int64_t{1}, int64_t{frames / 3}, int64_t{frames / 2 + 1}, static_cast<int64_t>(frames)}) {
    CHECK_MSG(sv::playback_seek(pb, at, error), error);
    same(at);
  }
  CHECK_MSG(wrong == 0, std::to_string(wrong) + " views differ after a seek");
  // The views move: the two players' places are not the same at the start and the end.
  CHECK(live.front()[0] != live.back()[0] && live.front()[1] != live.back()[1]);
}

// `setmaster` on Quake II's server: it turns public, and its master hears `heartbeat` with the
// status text, then `shutdown` when the server quits.
TEST(a_q2_server_beats_to_its_master_once_set) {
  sv::ServerConfig c;
  c.root = hall_folder();
  c.map = "hall";
  c.maxclients = kSlots;
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(4, lan);
  gn::Transport port, master;
  gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortQ2));
  gn::open_sim_transport(master, net, gn::sim_address(7, 27900));
  sv::Server s;
  std::string error;
  const sv::ServerGame& g = cn::q2_game();
  CHECK_MSG(sv::server_open(s, g, c, &port, nullptr, error), error);
  if (!s.state) return;
  CHECK(g.console(s.state, "setmaster sim:7"));
  std::vector<std::string> heard;
  const auto listen = [&] {
    uint8_t buf[2048];
    gn::Address from;
    for (int n; (n = gn::transport_receive(master, buf, sizeof buf, from, s.now_us)) >= 0;)
      heard.emplace_back(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
  };
  for (int f = 0; f < 20; f++) {
    gn::sim_advance(net, s.now_us);
    sv::server_frame(s);
    listen();
  }
  std::vector<std::string> lines;
  g.console_lines(s.state, lines);
  std::string told;
  for (const std::string& l : lines) told += l + " | ";
  CHECK_MSG(heard.size() == 1, told);
  if (!heard.empty()) {
    CHECK(heard[0].starts_with("\xff\xff\xff\xffheartbeat\n"));
    CHECK(heard[0].find("\\mapname\\hall") != std::string::npos);
  }
  sv::server_close(s);
  gn::sim_advance(net, s.now_us + 1000000);
  listen();
  CHECK(!heard.empty() && heard.back() == "\xff\xff\xff\xffshutdown");
}

// `quit [reason]` and `recycle [reason]`: every player told why at the high level and sent the
// disconnect, or the reconnect for a recycle; the server holds no player after.
TEST(a_q2_server_says_goodbye_with_the_operators_reason) {
  for (const bool restart : {false, true}) {
    sv::ServerConfig c;
    c.root = hall_folder();
    c.map = "hall";
    c.maxclients = kSlots;
    gn::LinkConditions lan;
    gn::SimNetwork net = gn::make_sim_network(5, lan);
    gn::Transport port, mine;
    gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortQ2));
    gn::open_sim_transport(mine, net, gn::sim_address(1, 27901));
    sv::Server s;
    std::string error;
    const sv::ServerGame& g = cn::q2_game();
    CHECK_MSG(sv::server_open(s, g, c, &port, nullptr, error), error);
    if (!s.state) return;
    cn::Q2ClientEnd eve;
    cn::Q2ClientConfig cc;
    cc.name = "eve";
    cn::q2_client_start(eve, mine, gn::sim_address(0, cn::kPortQ2), cc,
                        static_cast<double>(s.now_us) / 1e6);
    const auto step = [&] {
      gn::sim_advance(net, s.now_us);
      cn::q2_client_tick(eve, s.now_us);
      sv::server_frame(s);
    };
    for (int f = 0; f < 200; f++) step();
    CHECK_MSG(eve.stage == cn::ClientStage::active, eve.refused);
    g.quit(s.state, "going down for now", restart);
    for (int f = 0; f < 4; f++) step();
    const std::string want =
        restart ? "Server restarted: going down for now\n" : "Server quit: going down for now\n";
    CHECK(std::find(eve.prints.begin(), eve.prints.end(), want) != eve.prints.end());
    CHECK(restart || eve.stage == cn::ClientStage::dropped);
    sv::server_close(s);
  }
}

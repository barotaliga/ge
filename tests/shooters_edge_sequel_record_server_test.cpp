// Tests: the sequel's dedicated server recorded by the server core (engine/server/record.hpp) and
// played back (engine/server/playback.hpp) through its broadcast and watch hooks
// (games/shooters_edge/net/sequel_game_tv.hpp). Two clients of the sequel play a hall served on the
// simulated network while the core records it with a keyframe every 2 s; played back, every slot's
// view equals the live server's at every frame, from the start and after a seek into the middle.
//
// ge:summary Tests: the sequel's dedicated server recorded and played back from every view
// ge:keywords test sequel-game record playback player-view seek server-record sequel_player_view
// ge:skill net replay testing
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/playback.hpp"
#include "server/record.hpp"
#include "server/server.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_game.hpp"
#include "shooters_edge/net/sequel_net.hpp"

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
      std::filesystem::temp_directory_path() / "ge_sequel_record_server_test";
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

TEST(a_sequel_match_plays_back_as_every_player_saw_it) {
  sv::ServerConfig c;
  c.root = hall_folder();
  c.map = "hall";
  c.maxclients = kSlots;
  c.seed = 3;
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(3, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, cn::kPortSequel));
  sv::Server s;
  std::string error;
  const sv::ServerGame& g = cn::sequel_game();
  CHECK_MSG(sv::server_open(s, g, c, &port, nullptr, error), error);
  if (!s.state) return;
  std::vector<std::unique_ptr<gn::Transport>> ports;
  std::vector<std::unique_ptr<cn::SequelClientEnd>> clients;
  for (int n = 0; n < 2; n++) {
    ports.push_back(std::make_unique<gn::Transport>());
    gn::open_sim_transport(*ports.back(), net, gn::sim_address(uint32_t(n + 1), 27901));
    clients.push_back(std::make_unique<cn::SequelClientEnd>());
    cn::SequelClientConfig cc;
    cc.name = n == 0 ? "runner" : "dodger";
    cc.brain = n == 0 ? forward : strafe;
    cc.qport = static_cast<uint16_t>(100 + n);
    cn::sequel_client_start(*clients.back(), *ports.back(), gn::sim_address(0, cn::kPortSequel), cc,
                            static_cast<double>(s.now_us) / 1e6);
  }
  const auto step = [&] {
    gn::sim_advance(net, s.now_us);
    for (auto& cl : clients) cn::sequel_client_tick(*cl, s.now_us);
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

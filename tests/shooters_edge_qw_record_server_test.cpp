// Tests: Shooter's Edge's dedicated server recorded by the server core (engine/server/record.hpp)
// and played back (engine/server/playback.hpp) through Quake's broadcast and watch
// hooks (games/shooters_edge/net/qw_game_tv.hpp). A match of bots on the arena, served on the
// simulated network, is recorded with a keyframe every 2 s; played back, every slot's view equals
// the live server's at every frame, from the start and after a seek into the middle. Offline (no
// port, no endpoint) the broadcast is empty.
//
// ge:summary Tests: the classic dedicated server's match recorded and played back from every view
// ge:keywords test classic-game record playback player-view seek keyframe multiview server-record
// ge:skill net replay testing
#include <string>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/playback.hpp"
#include "server/record.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/qw_game.hpp"

namespace gn = ge::net;
namespace sv = ge::server;
namespace se = shooters_edge::net;

namespace {

constexpr int kSlots = 8;

sv::ServerConfig arena() {
  sv::ServerConfig c;
  c.hostname = "Arena Night";
  c.map = "arena";
  c.maxclients = kSlots;
  c.bots = 4;
  c.seed = 5;
  return c;
}

}  // namespace

TEST(a_classic_match_plays_back_as_every_player_saw_it) {
  gn::LinkConditions lan;
  gn::SimNetwork net = gn::make_sim_network(5, lan);
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, 27500));
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, se::qw_game(), arena(), &port, nullptr, error), error);
  const sv::ServerGame& g = se::qw_game();
  // A second of the match before the recording begins.
  for (int f = 0; f < 77; f++) {
    gn::sim_advance(net, s.now_us);
    sv::server_frame(s);
  }
  sv::Recording rec;
  rec.keyframe_seconds = 2;
  CHECK_MSG(sv::record_begin(rec, s, error), error);
  std::vector<std::vector<std::vector<uint8_t>>> live;
  int seen = 0;
  const auto views = [&] {
    std::vector<std::vector<uint8_t>> row(kSlots);
    for (int p = 0; p < kSlots; p++) {
      g.player_view(s.state, p, row[static_cast<size_t>(p)]);
      if (row[static_cast<size_t>(p)].size() > 1) seen++;
    }
    live.push_back(std::move(row));
  };
  views();
  const int frames = 77 * 7;
  for (int f = 0; f < frames; f++) {
    gn::sim_advance(net, s.now_us);
    sv::server_frame(s);
    sv::record_frame(rec, s);
    views();
  }
  sv::record_finish(rec, s);
  sv::server_close(s);
  CHECK_MSG(seen > frames * 3, std::to_string(seen) + " views of a player in play");

  sv::Playback pb;
  CHECK_MSG(sv::playback_open(pb, g, rec.bytes, error), error);
  CHECK_MSG(static_cast<int>(pb.frames.size()) == frames,
            std::to_string(pb.frames.size()) + " frames");
  CHECK_MSG(pb.keys.size() >= 3, std::to_string(pb.keys.size()) + " keyframes");
  int wrong = 0, first = -1;
  const auto same = [&](int64_t frame) {
    for (int p = 0; p < kSlots; p++) {
      std::vector<uint8_t> v;
      sv::playback_view(pb, p, v);
      if (v != live[static_cast<size_t>(frame)][static_cast<size_t>(p)]) {
        if (first < 0) first = static_cast<int>(frame) * 100 + p;
        wrong++;
      }
    }
  };
  same(0);
  while (sv::playback_step(pb, error)) same(pb.at);
  CHECK_MSG(error.empty(), error);
  CHECK_MSG(wrong == 0, std::to_string(wrong) + " views differ, the first at frame*100+slot " +
                            std::to_string(first));
  for (const int64_t at : {1, 160, 300, 461, frames}) {
    CHECK_MSG(sv::playback_seek(pb, at, error), error);
    same(at);
  }
  CHECK_MSG(wrong == 0, std::to_string(wrong) + " views differ after a seek");
}

TEST(an_offline_classic_server_broadcasts_nothing) {
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, se::qw_game(), arena(), nullptr, nullptr, error), error);
  sv::Recording rec;
  CHECK_MSG(sv::record_begin(rec, s, error), error);
  for (int f = 0; f < 100; f++) {
    sv::server_frame(s);
    sv::record_frame(rec, s);
  }
  sv::record_finish(rec, s);
  CHECK(rec.frame_bytes == 0);
  sv::server_close(s);
}

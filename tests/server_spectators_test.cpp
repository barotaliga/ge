// Tests: spectators on the server itself (engine/server/spectators.hpp). The core's sample game is
// served on the simulated network with its own TV on a second port, fed by its recording with a
// 2 s delay. A spectator there from the start gets the whole match, no block sooner than the delay
// after it closed, and plays it back to every player's view as the live server gave it; one who
// comes late starts at the newest keyframe out and ends at the server's last views; one past
// `sv_tvmaxviewers` is turned away with the reason. The settings are the server's cvars.
//
// ge:summary Tests: spectators on the server's own TV, delayed, late, whole and turned away
// ge:keywords test spectators server-tv sv_tvport sv_tvdelay sv_tvmaxviewers late-joiner playback
// ge:skill replay net testing
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/cvar.hpp"
#include "net/sim_network.hpp"
#include "replay/stream.hpp"
#include "server/playback.hpp"
#include "server/record.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"
#include "server/spectators.hpp"
#include "tv/net_link.hpp"

namespace sv = ge::server;
namespace tv = ge::tv;
namespace gn = ge::net;
namespace rp = ge::replay;

namespace {

using Views = std::vector<std::vector<uint8_t>>;

Views live_views(const sv::Server& s) {
  const sv::ServerGame& g = *s.game;
  Views row(16);
  for (int p = 0; p < 16; p++) g.player_view(s.state, p, row[static_cast<size_t>(p)]);
  return row;
}

struct Spectator {
  gn::Transport port;
  tv::TvClient client;
  int64_t too_soon = 0;  // times it held a block sooner than the delay allows
};

}  // namespace

TEST(spectators_on_the_servers_own_tv_see_the_match_behind_its_delay) {
  gn::SimNetwork net = gn::make_sim_network(5, gn::LinkConditions{});
  gn::Transport game_port, tv_port;
  gn::open_sim_transport(game_port, net, gn::sim_address(0, 27500));
  gn::open_sim_transport(tv_port, net, gn::sim_address(0, 27600));
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = 10;
  c.maxclients = 16;
  c.seed = 7;
  c.hostname = "Ring Hall";
  c.tv_port = 27600;
  c.tv_delay_s = 2;
  c.tv_max_viewers = 2;
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, sv::sample_game(), c, &game_port, nullptr, error), error);
  if (!s.state) return;
  sv::Recording rec;
  rec.keyframe_seconds = 4;
  CHECK_MSG(sv::record_begin(rec, s, error), error);
  sv::ServerTv t;
  sv::server_tv_open(t, s, tv_port);
  CHECK(sv::server_tv_frame(t, sv::record_fresh(rec), s.now_us));

  std::vector<std::unique_ptr<Spectator>> spec;
  const auto join = [&](const char* name) {
    spec.push_back(std::make_unique<Spectator>());
    Spectator& sp = *spec.back();
    gn::open_sim_transport(sp.port, net, gn::sim_address(uint32_t(spec.size()), 40000));
    tv::tv_client_init(sp.client, sp.port, tv_port.local, name, false, spec.size() * 31u, s.now_us);
  };
  join("early");
  std::vector<Views> live{live_views(s)};
  const int frames = static_cast<int>(15.0 / s.frame_s);
  const auto tick = [&](int64_t now_us, std::span<const uint8_t> fresh) {
    CHECK(sv::server_tv_frame(t, fresh, now_us));
    gn::sim_advance(net, now_us);
    for (auto& sp : spec) {
      tv::tv_client_update(sp->client, now_us);
      // Block k closes at (k + 1) s and may go out 2 s after it starts: the newest a spectator
      // holds at `now` is at most block now - 2, so it holds no more than now - 1 blocks.
      const int64_t most = now_us / 1000000 - 1;
      if (sp->client.watch.blocks > (most < 0 ? 0 : most)) sp->too_soon++;
    }
  };
  for (int f = 0; f < frames; f++) {
    sv::server_frame(s);
    sv::record_frame(rec, s);
    live.push_back(live_views(s));
    tick(s.now_us, sv::record_fresh(rec));
    if (f == static_cast<int>(9.0 / s.frame_s)) join("late");  // the keyframe at 8 s not yet out
    if (f == static_cast<int>(10.0 / s.frame_s)) join("one more");  // past sv_tvmaxviewers
  }
  CHECK(sv::server_tv_watching(t) == 2);
  CHECK(sv::server_tv_status(t) == "tv=2");
  sv::record_finish(rec, s);
  sv::server_tv_end(t);
  std::span<const uint8_t> rest = sv::record_fresh(rec);
  for (int64_t at = s.now_us; at < s.now_us + 4000000; at += 20000) {
    tick(at, rest);
    rest = {};
  }

  const Spectator& early = *spec[0];
  const Spectator& late = *spec[1];
  const Spectator& extra = *spec[2];
  CHECK_MSG(early.too_soon == 0, std::to_string(early.too_soon) + " ticks ahead of the delay");
  CHECK_MSG(late.too_soon == 0, std::to_string(late.too_soon) + " ticks ahead of the delay");
  CHECK(early.client.watch.ended && late.client.watch.ended);
  CHECK_MSG(extra.client.watch.refused == "this TV is full", extra.client.watch.refused);
  // The early spectator holds the server's recording whole (all but its trailer, which the TV
  // keeps), and it plays back to every view.
  const std::vector<uint8_t>& got = early.client.watch.stream;
  CHECK(got.size() < rec.bytes.size() && std::equal(got.begin(), got.end(), rec.bytes.begin()));
  sv::Playback pb;
  CHECK_MSG(sv::playback_open(pb, sv::sample_game(), early.client.watch.stream, error), error);
  int wrong = 0;
  const auto same = [&](int64_t frame) {
    for (int p = 0; p < 16; p++) {
      std::vector<uint8_t> v;
      sv::playback_view(pb, p, v);
      wrong += v != live[static_cast<size_t>(frame)][static_cast<size_t>(p)] ? 1 : 0;
    }
  };
  same(0);
  while (sv::playback_step(pb, error)) same(pb.at);
  CHECK_MSG(wrong == 0, std::to_string(wrong) + " views differ from the live server's");
  CHECK(pb.at == frames);
  // The late one started at a keyframe after it came, and ends at the server's last views.
  rp::StreamReader r;
  CHECK_MSG(rp::read_stream_header(late.client.watch.stream, r, error), error);
  const sv::ServerGame& g = sv::sample_game();
  void* w = g.watch_open();
  rp::Block b;
  int64_t first = -1;
  bool read = true;
  while (rp::next_block(r, b, error)) {
    if (first < 0) {
      first = b.index;
      CHECK(!b.chunks.empty() && b.chunks.front().track == sv::kRecordKeyframeTrack);
    }
    for (const rp::Chunk& ch : b.chunks)
      read = (ch.track == sv::kRecordKeyframeTrack ? g.watch_keyframe(w, ch.bytes, error)
                                                   : g.watch_frame(w, ch.bytes, error)) &&
             read;
  }
  CHECK_MSG(read && error.empty(), error);
  // At 9 s the blocks out are those up to 6 s (the delay): the newest keyframe among them is 4 s's.
  CHECK_MSG(first == 4, "the late spectator's first block is " + std::to_string(first));
  for (int p = 0; p < 16; p++) {
    std::vector<uint8_t> v;
    g.watch_view(w, p, v);
    CHECK(v == live.back()[static_cast<size_t>(p)]);
  }
  g.watch_close(w);
  sv::server_tv_close(t, s.now_us);
  CHECK(sv::server_tv_status(t).empty());
  sv::server_close(s);
}

TEST(the_servers_tv_is_set_by_its_cvars) {
  ge::console::Cvars cvars;
  sv::ServerConfig c;
  sv::add_server_cvars(cvars, c, sv::sample_game());
  CHECK(c.tv_port == 0 && c.tv_delay_s == 10 && c.tv_max_viewers == 32);
  namespace cn = ge::console;
  CHECK(cvars.set("sv_tvport", "27600", cn::Source::local) == cn::SetResult::latched);
  CHECK(cvars.set("sv_tvdelay", "30", cn::Source::local) == cn::SetResult::latched);
  CHECK(cvars.set("sv_tvmaxviewers", "8", cn::Source::local) == cn::SetResult::latched);
  cvars.apply_latched();
  CHECK(c.tv_port == 27600 && c.tv_delay_s == 30 && c.tv_max_viewers == 8);
}

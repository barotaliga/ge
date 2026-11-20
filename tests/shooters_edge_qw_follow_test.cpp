// Tests: a spectator following the play (games/shooters_edge/net/qw_follow.hpp) and the
// server's side of it (`ptrack`, net/qw_signon.cpp and qw_frame.cpp), on the simulated
// network: two players play, the watcher tracks one, the server sends it his stats and keeps its
// camera on him by its teleports, and its recording plays back as his view (client/demo.cpp's
// tracking spectator). A player's `ptrack` is ignored; a bad slot is refused in a player's words.
//
// ge:summary Tests: a watcher tracking a player: ptrack, his stats, tmove, his view on playback
// ge:keywords test follow ptrack tmove track spectator watcher playback ride-tracked
// ge:skill net shooter testing
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_follow.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/net/qw_watcher.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

// Held on the heap: a whole server is too big for Windows' main-thread stack.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, at, bt, wt;
  cn::QwServer server;
  cn::QwClientEnd a, b;
  cn::QwWatcher watcher;
  int64_t now = 0;

  bool start(std::string& error) {
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
    const gn::Address server_at = gn::sim_address(0, cn::kPortServer);
    gn::open_sim_transport(st, net, server_at);
    gn::open_sim_transport(at, net, gn::sim_address(1, cn::kPortClient));
    gn::open_sim_transport(bt, net, gn::sim_address(2, cn::kPortClient));
    gn::open_sim_transport(wt, net, gn::sim_address(3, cn::kPortClient));
    rules = sv::qc_runner_over(*qc);
    if (!cn::qw_start(server, rules, st, cn::ServerConfig{}, error)) return false;
    cn::ClientConfig ac;
    ac.name = "runner";
    ac.qport = 4242;
    ac.seed = 3;
    cn::client_start(a, at, server_at, ac, 0);
    cn::ClientConfig bc;
    bc.name = "circler";
    bc.qport = 4243;
    bc.seed = 9;
    cn::client_start(b, bt, server_at, bc, 0);
    cn::ClientConfig wc;
    wc.name = "watcher";
    wc.qport = 4343;
    wc.seed = 4;
    cn::watcher_start(watcher, wt, server_at, wc, {}, 0);
    return true;
  }
  void tick() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::qw_frame(server, now);
    cn::client_tick(a, now);
    cn::client_tick(b, now);
    cn::watcher_tick(watcher, now);
  }
  const cn::QwClientEnd& player(int slot) const { return a.playernum == slot ? a : b; }
};

float apart(ge::shooter::Vec3f p, ge::shooter::Vec3f q) {
  return std::fabs(p.x - q.x) + std::fabs(p.y - q.y) + std::fabs(p.z - q.z);
}

bool has_print(const cn::QwClientEnd& c, const std::string& text) {
  for (const std::string& p : c.prints)
    if (p.find(text) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(the_watcher_tracks_a_player_and_its_recording_plays_as_his_view) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  CHECK_MSG(r.start(error), error);
  int tracked_ticks = 0, near_ticks = 0, stats_same = 0, server_same = 0;
  for (int i = 0; i < 900; i++) {
    r.tick();
    const int slot = r.watcher.camera.slot;
    if (slot < 0 || r.watcher.client.playernum < 0) continue;
    tracked_ticks++;
    // The server tracks the same player for the watcher (a switch's `ptrack` a moment behind).
    if (r.server.clients[r.watcher.client.playernum].track == slot) server_same++;
    // The watcher stands where he stands (its teleports), give or take a tick or two of travel.
    const auto me = r.server.game->sv.edicts[static_cast<size_t>(r.watcher.client.playernum) + 1];
    const auto him = r.server.game->sv.edicts[static_cast<size_t>(slot) + 1];
    if (apart(me.origin, him.origin) < 64) near_ticks++;
    // His stats are the watcher's: the server sends them to it as to him.
    const cn::QwClientEnd& p = r.player(slot);
    if (r.watcher.client.stats[2] == p.stats[2] && r.watcher.client.stats[0] == p.stats[0])
      stats_same++;
  }
  CHECK(r.watcher.camera.slot == r.a.playernum || r.watcher.camera.slot == r.b.playernum);
  CHECK_MSG(tracked_ticks > 700, std::to_string(tracked_ticks));
  CHECK_MSG(server_same > tracked_ticks * 9 / 10, std::to_string(server_same));
  CHECK_MSG(near_ticks > tracked_ticks * 9 / 10, std::to_string(near_ticks));
  CHECK_MSG(stats_same > tracked_ticks * 9 / 10, std::to_string(stats_same));

  // Played back, the recording rides the tracked player: his slot is the view's, the eye his.
  cn::watcher_stop(r.watcher);
  cl::Demo d;
  CHECK_MSG(cl::open_demo(r.watcher.recording.bytes, d, error), error);
  int frames = 0, ridden = 0;
  cl::DemoRead got = cl::DemoRead::frame;
  while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) {
    frames++;
    if (d.state.player == r.a.playernum || d.state.player == r.b.playernum) ridden++;
  }
  CHECK_MSG(got == cl::DemoRead::end, error);
  CHECK_MSG(ridden > frames * 3 / 4, std::to_string(ridden) + " of " + std::to_string(frames));
}

TEST(ptrack_is_a_spectators_and_a_bad_slot_is_refused) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  r.watcher.follow = false;  // the commands by hand
  CHECK_MSG(r.start(error), error);
  for (int i = 0; i < 300; i++) r.tick();
  const int w = r.watcher.client.playernum;
  CHECK(w >= 0 && r.a.playernum >= 0);
  cn::client_command(r.a, "ptrack " + std::to_string(r.b.playernum));  // a player: ignored
  cn::client_command(r.watcher.client, "ptrack " + std::to_string(r.a.playernum));
  for (int i = 0; i < 20; i++) r.tick();
  CHECK(r.server.clients[r.a.playernum].track == -1);
  CHECK(r.server.clients[w].track == r.a.playernum);
  cn::client_command(r.watcher.client, "ptrack 31");  // nobody there
  for (int i = 0; i < 20; i++) r.tick();
  CHECK(r.server.clients[w].track == -1);
  CHECK(has_print(r.watcher.client, "Invalid client to track"));
  cn::client_command(r.watcher.client, "ptrack " + std::to_string(w));  // itself: a spectator
  cn::client_command(r.watcher.client, "ptrack " + std::to_string(r.b.playernum));
  for (int i = 0; i < 20; i++) r.tick();
  CHECK(r.server.clients[w].track == r.b.playernum);
  cn::client_command(r.watcher.client, "ptrack");  // bare: stops
  for (int i = 0; i < 20; i++) r.tick();
  CHECK(r.server.clients[w].track == -1);
}

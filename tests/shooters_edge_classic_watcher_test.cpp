// Tests: the watcher (games/shooters_edge/net/classic_watcher.hpp) on the simulated network. A
// spectator joins our classic server, which has no broadcaster of its own here, while a player
// plays; its recording is broadcast as it grows through a TV node to a viewer there from the start
// and one who joins half way. Both are given the watcher's whole recording, byte for byte, and it
// reads to its end as a client recording; the late one was sent the match from its start.
//
// ge:summary Tests: a spectator's view broadcast through a TV node to early and late viewers
// ge:keywords test watcher spectator tv broadcast late-viewer recording-broadcast
// ge:skill net replay testing
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/recording_tv.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_watcher.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "tv/node.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace tv = ge::tv;

namespace {

// Held on the heap: a whole server is too big for Windows' main-thread stack.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, pt, wt;
  cn::ClassicServer server;
  cn::ClassicClientEnd player;
  cn::ClassicWatcher watcher;
  tv::Node node;
  tv::Watch early, late;
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
    gn::open_sim_transport(pt, net, gn::sim_address(1, cn::kPortClient));
    gn::open_sim_transport(wt, net, gn::sim_address(2, cn::kPortClient));
    rules = sv::qc_runner_over(*qc);
    if (!cn::classic_start(server, rules, st, cn::ServerConfig{}, error)) return false;
    cn::ClientConfig pc;
    pc.name = "runner";
    pc.qport = 4242;
    pc.seed = 3;
    cn::client_start(player, pt, server_at, pc, 0);
    cn::ClientConfig wc;
    wc.name = "watcher";
    wc.qport = 4343;
    wc.seed = 4;
    cn::watcher_start(watcher, wt, server_at, wc, {{"title", "test"}}, 0);
    node.config.delay_ms = 1000;
    return true;
  }
  void tick() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    cn::client_tick(player, now);
    cn::watcher_tick(watcher, now);
    feed();
  }
  void feed() {
    const std::span<const uint8_t> fresh = cl::recording_broadcast_fresh(watcher.cast);
    if (!fresh.empty()) CHECK(tv::tv_feed(node, fresh, now / 1000));
    tv::Watch* watch[2] = {&early, &late};
    tv::tv_update(node, now / 1000, [&](int link, std::span<const uint8_t> msg) {
      return tv::watch_receive(*watch[link], msg);
    });
  }
};

}  // namespace

TEST(a_spectators_view_reaches_every_viewer_whole_through_a_tv_node) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  CHECK_MSG(r.start(error), error);
  tv::tv_join(r.node, 0);
  CHECK(tv::tv_receive(r.node, 0, tv::tv_hello("early", false)));
  int64_t late_catch_up = -1;
  for (int i = 0; i < 1200; i++) {
    r.tick();
    if (i == 600) {
      late_catch_up = tv::tv_catch_up_blocks(r.node);
      tv::tv_join(r.node, 1);
      CHECK(tv::tv_receive(r.node, 1, tv::tv_hello("late", false)));
    }
  }
  CHECK_MSG(r.watcher.client.stage == cn::ClientStage::active,
            std::string("the watcher is at ") + cn::client_stage_name(r.watcher.client.stage));
  CHECK(r.watcher.client.full_frames + r.watcher.client.delta_frames > 500);
  cn::watcher_stop(r.watcher);
  r.feed();
  tv::tv_end(r.node);
  for (int i = 0; i < 300 && !(r.early.ended && r.late.ended); i++) {
    r.now += 13000;
    r.feed();
  }
  CHECK(late_catch_up > 3);  // the match from its start, not from a later keyframe
  for (const tv::Watch* w : {&r.early, &r.late}) {
    CHECK(w->ended);
    std::vector<uint8_t> back;
    CHECK_MSG(cl::unpack_demo(w->stream, back, error), error);
    CHECK(back == r.watcher.recording.bytes);
    // It reads to its end as a client recording.
    cl::Demo d;
    CHECK_MSG(cl::open_demo(back, d, error), error);
    int frames = 0;
    cl::DemoRead got = cl::DemoRead::frame;
    while ((got = cl::next_frame(d, error)) == cl::DemoRead::frame) frames++;
    CHECK_MSG(got == cl::DemoRead::end, error);
    CHECK(frames > 500);
  }
}

TEST(a_dropped_watcher_ends_its_broadcast) {
  auto owned = std::make_unique<Rig>();
  Rig& r = *owned;
  std::string error;
  CHECK_MSG(r.start(error), error);
  for (int i = 0; i < 200; i++) r.tick();
  CHECK(!r.watcher.ended);
  r.watcher.client.stage = cn::ClientStage::dropped;  // as a server's disconnect leaves it
  r.tick();
  CHECK(r.watcher.ended && r.watcher.cast.ended);
  std::vector<uint8_t> back;
  CHECK_MSG(cl::unpack_demo(r.watcher.cast.out, back, error), error);
  CHECK(back == r.watcher.recording.bytes);
}

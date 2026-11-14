// games/shooters_edge/net/sequel_tv_face.hpp: our TV node's sequel face on the simulated network.
// A watcher of ours joins our sequel server as a spectator and asks for a whole frame every two
// seconds; its recording's blocks go to the face; viewers (our sequel client, as an unmodified
// one would) connect to the face as to a server, go through its signon and watch: one from the
// start, one that joins late and starts at a whole frame, one over a lossy line; a wrong password
// is refused in a player's words; and the relay end to end (watcher, TV node, face) brings a
// viewer the match behind by its delay, its camera on the player.
//
// ge:summary Test: the sequel face of our TV, viewers watching a match relayed from a watcher
// ge:keywords test sequel tv-node sequel-face viewer late-joiner whole-frame lossy password
// ge:skill net shooter testing
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/net/sequel_client.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"
#include "shooters_edge/net/sequel_tv_demo.hpp"
#include "shooters_edge/net/sequel_tv_face.hpp"
#include "shooters_edge/net/sequel_tv_relay.hpp"

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
  for (const char* origin : {"-400 -400 24", "400 400 24"}) {
    br::Entity e;
    e.pairs = {{"classname", "info_player_deathmatch"}, {"origin", origin}, {"angle", "0"}};
    w.entities.push_back(e);
  }
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

sh::MoveCmd walk(int tick) {
  sh::MoveCmd m;
  m.forward = 300;
  m.angles[1] = static_cast<float>(tick % 360);
  return m;
}

// Our sequel server (node 0) with a player, a watcher on node 1, the face on node 5, viewers from
// node 6 up.
struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport server_port, player_port, watcher_port, face_port;
  std::vector<std::unique_ptr<gn::Transport>> viewer_ports;
  cn::SequelNetServer server;
  cn::SequelClientEnd player, watcher;
  std::vector<std::unique_ptr<cn::SequelClientEnd>> viewers;
  cn::SequelTvFace face;
  size_t taken = 0;  // of the watcher's recording, the bytes handed to the face
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::SequelTvConfig& tv = {}) {
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
  r->net = gn::make_sim_network(11, link);
  const gn::Address server_at = gn::sim_address(0, cn::kPortSequel);
  gn::open_sim_transport(r->server_port, r->net, server_at);
  CHECK_MSG(cn::sequel_net_start(r->server, r->rules, r->server_port, {}, error), error);
  gn::open_sim_transport(r->player_port, r->net, gn::sim_address(2, 27901));
  cn::SequelClientConfig pc;
  pc.name = "runner";
  pc.qport = 7;
  pc.brain = walk;
  cn::sequel_client_start(r->player, r->player_port, server_at, pc, 0);
  gn::open_sim_transport(r->watcher_port, r->net, gn::sim_address(1, 27901));
  cn::SequelClientConfig wc;
  wc.name = "tv";
  wc.spectator = true;
  wc.whole_every = 2;
  wc.qport = 8;
  cn::sequel_client_start(r->watcher, r->watcher_port, server_at, wc, 0);
  gn::open_sim_transport(r->face_port, r->net, gn::sim_address(5, 27910));
  cn::sequel_tv_start(r->face, r->face_port, tv);
  return r;
}

cn::SequelClientEnd& watch(Rig& r, const std::string& password = "") {
  const auto n = static_cast<uint32_t>(r.viewers.size());
  r.viewer_ports.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.viewer_ports.back(), r.net, gn::sim_address(6 + n, 27901));
  r.viewers.push_back(std::make_unique<cn::SequelClientEnd>());
  cn::SequelClientConfig cfg;
  cfg.name = "viewer" + std::to_string(n);
  cfg.qport = static_cast<uint16_t>(200 + n);
  cfg.password = password;
  cn::sequel_client_start(*r.viewers.back(), *r.viewer_ports.back(), gn::sim_address(5, 27910), cfg,
                          static_cast<double>(r.now) / 1e6);
  return *r.viewers.back();
}

// The watcher's new blocks to the face.
void relay(Rig& r) {
  const std::vector<uint8_t>& b = r.watcher.demo.bytes;
  while (r.taken + 4 <= b.size()) {
    const uint32_t len =
        static_cast<uint32_t>(b[r.taken]) | static_cast<uint32_t>(b[r.taken + 1]) << 8 |
        static_cast<uint32_t>(b[r.taken + 2]) << 16 | static_cast<uint32_t>(b[r.taken + 3]) << 24;
    if (len == 0xffffffffu || r.taken + 4 + len > b.size()) return;
    cn::sequel_tv_block(r.face, std::span<const uint8_t>(b.data() + r.taken + 4, len));
    r.taken += 4 + len;
  }
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    cn::sequel_client_tick(r.player, r.now);
    cn::sequel_client_tick(r.watcher, r.now);
    for (auto& v : r.viewers) cn::sequel_client_tick(*v, r.now);
    cn::sequel_net_frame(r.server, r.now);
    relay(r);
    cn::sequel_tv_frame(r.face, r.now);
  }
}

}  // namespace

TEST(viewers_watch_the_match_through_the_face_from_the_start_and_late) {
  auto r = rig();
  cn::SequelClientEnd& first = watch(*r);
  run(*r, 240);  // 3 s
  CHECK_MSG(r->watcher.stage == cn::ClientStage::active, r->watcher.refused);
  CHECK(r->face.map == "hall");
  CHECK(r->face.whole_frames >= 1);
  CHECK_MSG(first.stage == cn::ClientStage::active, first.refused + " " + first.error);
  cn::SequelClientEnd& late = watch(*r);
  run(*r, 400);  // 5 s more
  CHECK_MSG(r->face.error.empty(), r->face.error);
  CHECK(r->face.whole_frames >= 3);  // one every 2 s
  for (const cn::SequelClientEnd* v : {&first, &late}) {
    CHECK_MSG(v->stage == cn::ClientStage::active, v->refused + " " + v->error);
    CHECK_MSG(v->error.empty(), v->error);
    CHECK(v->demo.state.map == "hall");
    CHECK(v->demo.last_frame > 0);  // the newest frame read whole: every delta held
  }
  CHECK_MSG(first.frames > 50, std::to_string(first.frames) + " frames");
  CHECK_MSG(late.frames > 30, std::to_string(late.frames) + " frames");
  CHECK(cn::sequel_tv_viewers(r->face) == 2);
  cn::sequel_tv_stop(r->face);
  run(*r, 10);
  CHECK(cn::sequel_tv_viewers(r->face) == 0);
}

TEST(a_viewer_over_a_lossy_line_still_reads_every_frame) {
  auto r = rig();
  gn::LinkConditions bad;
  CHECK(gn::conditions_preset("bad", bad));
  cn::SequelClientEnd& v = watch(*r);
  gn::sim_set_path(r->net, 5, 6, bad);
  gn::sim_set_path(r->net, 6, 5, bad);
  run(*r, 1500);
  CHECK_MSG(v.stage == cn::ClientStage::active, v.refused + " " + v.error);
  CHECK_MSG(v.error.empty(), v.error);
  CHECK(v.demo.last_frame > 0);
  // Blocks that queued behind a resend go out joined, a packet carrying several frames.
  CHECK_MSG(v.frames > 50, std::to_string(v.frames) + " frames");
  CHECK(r->face.viewers[0].queue.size() < 10);  // not falling behind
}

TEST(a_wrong_password_is_refused_in_a_players_words) {
  cn::SequelTvConfig tv;
  tv.password = "secret";
  auto r = rig(tv);
  cn::SequelClientEnd& v = watch(*r, "guess");
  run(*r, 200);
  CHECK(v.stage != cn::ClientStage::active);
  CHECK_MSG(v.refused.find("Password") != std::string::npos, v.refused);
  CHECK(v.refused.find("next:") == std::string::npos);
  CHECK(r->face.refused >= 1);
}

TEST(the_relay_carries_the_match_through_a_tv_node_to_a_viewer_behind_by_the_delay) {
  auto r = rig();
  // The relay's own watcher replaces the rig's: it dials the server itself.
  gn::Transport watcher_port, face_port, viewer_port;
  gn::open_sim_transport(watcher_port, r->net, gn::sim_address(3, 27901));
  gn::open_sim_transport(face_port, r->net, gn::sim_address(4, 27911));
  gn::open_sim_transport(viewer_port, r->net, gn::sim_address(9, 27901));
  auto relay = std::make_unique<cn::SequelTvRelay>();
  cn::SequelTvRelayConfig rc;
  rc.delay_s = 1;
  cn::sequel_tv_relay_open(*relay, watcher_port, face_port, gn::sim_address(0, cn::kPortSequel), rc,
                           r->now);
  cn::SequelClientEnd viewer;
  cn::SequelClientConfig vc;
  vc.name = "viewer";
  vc.qport = 300;
  cn::sequel_client_start(viewer, viewer_port, gn::sim_address(4, 27911), vc, 0);
  for (int i = 0; i < 800; i++) {  // about 10 s
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    cn::sequel_client_tick(r->player, r->now);
    cn::sequel_client_tick(viewer, r->now);
    cn::sequel_net_frame(r->server, r->now);
    cn::sequel_tv_relay_update(*relay, r->now);
  }
  CHECK_MSG(relay->error.empty(), relay->error);
  CHECK(relay->watcher.stage == cn::ClientStage::active);
  CHECK(relay->target > 0);  // the camera took the player
  CHECK_MSG(viewer.stage == cn::ClientStage::active, viewer.refused + " " + viewer.error);
  CHECK_MSG(viewer.error.empty(), viewer.error);
  CHECK(viewer.demo.state.map == "hall");
  CHECK(viewer.demo.last_frame > 0);
  // Behind the watcher by about the delay (and the broadcast's one-second chunks).
  const int32_t behind = relay->watcher.demo.last_frame - viewer.demo.last_frame;
  CHECK_MSG(behind >= 8 && behind <= 30, std::to_string(behind) + " frames behind");
  cn::sequel_tv_relay_close(*relay);
}

TEST(a_recording_plays_to_viewers_at_the_games_pace_and_loops) {
  auto r = rig();
  run(*r, 400);  // the watcher's recording: about 5 s of the match
  CHECK_MSG(r->watcher.stage == cn::ClientStage::active, r->watcher.refused);
  std::vector<uint8_t> recording = r->watcher.demo.bytes;
  const int64_t recorded = r->watcher.frames;
  CHECK(recorded > 30);
  // A second face on node 9 plays the recording; one viewer watches it.
  gn::Transport face_port, viewer_port;
  gn::open_sim_transport(face_port, r->net, gn::sim_address(9, 27910));
  gn::open_sim_transport(viewer_port, r->net, gn::sim_address(10, 27901));
  cn::SequelTvFace face;
  cn::sequel_tv_start(face, face_port, {});
  cn::SequelTvDemo demo;
  std::string error;
  CHECK(!cn::sequel_tv_demo_open(demo, std::vector<uint8_t>(16, 0), true, error));
  CHECK_MSG(!error.empty(), "a refusal says why");
  CHECK_MSG(cn::sequel_tv_demo_open(demo, recording, true, error), error);
  CHECK(demo.from_protocol == 0);
  // The same recording in the older protocol plays as 34: converted when it is opened.
  {
    cn::SequelTvDemo old;
    const std::vector<uint8_t> as26 = shooters_edge::client::sequel_as_protocol_26(recording);
    CHECK(!as26.empty() && as26 != recording);
    CHECK_MSG(cn::sequel_tv_demo_open(old, as26, true, error), error);
    CHECK(old.from_protocol == 26);
    CHECK(old.bytes.size() <= recording.size());
    CHECK(std::equal(old.bytes.begin(), old.bytes.end(), recording.begin()));
  }
  cn::SequelClientEnd v;
  cn::SequelClientConfig cfg;
  cfg.name = "couch";
  cfg.qport = 300;
  cn::sequel_client_start(v, viewer_port, gn::sim_address(9, 27910), cfg,
                          static_cast<double>(r->now) / 1e6);
  const int64_t start = r->now;
  int64_t frames_at_1s = -1;
  for (int i = 0; i < 1300; i++) {  // about 17 s: three times round
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    cn::sequel_client_tick(v, r->now);
    cn::sequel_tv_demo_update(demo, face, r->now);
    cn::sequel_tv_frame(face, r->now);
    if (frames_at_1s < 0 && r->now - start >= 1000000) frames_at_1s = demo.frames;
  }
  CHECK_MSG(face.error.empty(), face.error);
  CHECK(!demo.ended);
  // The game's pace: ten frames a second, not the whole file at once.
  CHECK_MSG(frames_at_1s >= 9 && frames_at_1s <= 12, std::to_string(frames_at_1s) + " frames");
  CHECK_MSG(demo.rounds >= 2, std::to_string(demo.rounds) + " rounds");
  CHECK(face.levels >= 2);  // each round a level of its own, its count raised
  CHECK_MSG(v.stage == cn::ClientStage::active, v.refused + " " + v.error);
  CHECK_MSG(v.error.empty(), v.error);
  CHECK(v.levels >= 3);
  CHECK(v.demo.state.map == "hall");
  CHECK_MSG(v.frames > 100, std::to_string(v.frames) + " frames");
  // A file the viewer asks for is refused at once, and the viewer goes on watching.
  const int64_t before = v.frames;
  cn::sequel_client_command(v, "download models/none/tris.md2");
  for (int i = 0; i < 100; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    cn::sequel_client_tick(v, r->now);
    cn::sequel_tv_demo_update(demo, face, r->now);
    cn::sequel_tv_frame(face, r->now);
  }
  CHECK_MSG(v.error.empty(), v.error);
  CHECK(face.downloads_refused == 1);
  CHECK(v.stage == cn::ClientStage::active);
  CHECK(v.frames > before);
}

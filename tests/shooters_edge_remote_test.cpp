// Tests: the window joined to a server (games/shooters_edge/net/classic_remote.hpp) over
// engine/net's simulated network: the classic server on the test arena, the remote client driven by
// "hands" instead of a script. It signs on, its recording read back names the map and puts the eye
// on the player, the server's turn (setangle) is taken by the view, walking forward moves the eye
// the way the view faces, an impulse is sent once, and leaving frees the server's slot. With the
// level's solids handed over, the player's own moves are predicted: over a 50 ms link each
// prediction lands where the server later says the player was, and `cl_nopred` draws the server's
// place instead. Beside a scripted player, the other player is run ahead every window frame from
// his newest record, so he moves between the server's frames and sits nearer the server's place
// than the frame put him; missiles are drawn between the last two frames.
//
// ge:summary Tests: the remote client signs on, draws the server's state, walks, and leaves
// ge:keywords test remote-client connect disconnect classic-server live-demo setangle walk
// ge:keywords prediction cl_nopred latency predict_other cl_predict_players cl_nolerp lerp
// ge:skill net shooter testing
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

namespace {

// The test arena served on a LAN of the simulated network, and one remote client on it.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;  // over `qc`
  gn::SimNetwork net;
  gn::Transport st, ct, bt;
  cn::ClassicClientEnd other;  // a scripted player beside the window (`with_other`)
  bool with_other = false;
  cn::ClassicServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0;

  double latency_ms = 0.25;  // one way, each direction
  bool start() {
    std::string error;
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
    lan.latency_ms = latency_ms;
    lan.jitter_ms = 0;
    net = gn::make_sim_network(1, lan);
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    gn::open_sim_transport(ct, net, gn::sim_address(1, cn::kPortClient));
    rules = sv::qc_runner_over(*qc);
    if (!cn::classic_start(server, rules, st, cn::ServerConfig{}, error)) return false;
    cn::ClientConfig cc;
    cc.name = "window";
    cn::remote_start(remote, ct, gn::sim_address(0, cn::kPortServer), cc);
    if (with_other) {
      gn::open_sim_transport(bt, net, gn::sim_address(2, cn::kPortClient));
      cn::ClientConfig oc;
      oc.name = "runner";
      cn::client_start(other, bt, gn::sim_address(0, cn::kPortServer), oc, 0);
    }
    return true;
  }
  // One server frame and one window frame of the same length; the last step's result.
  cn::RemoteStep frame() {
    now += cn::kTickUs;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    if (with_other) cn::client_tick(other, now);
    return cn::remote_step(remote, cn::kTickUs * 1e-6, said);
  }
  // Frames until the client plays and has drawn a level; false if it never does.
  bool join() {
    bool level = false;
    for (int f = 0; f < 800 && !(level && remote.end.stage == cn::ClientStage::active); f++)
      if (frame() == cn::RemoteStep::new_level) level = true;
    return level && remote.end.stage == cn::ClientStage::active;
  }
};

bool said_has(const std::vector<std::string>& said, const std::string& what) {
  for (const std::string& s : said)
    if (s.find(what) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(the_remote_client_signs_on_and_draws_the_server_s_player_as_a_demo) {
  Rig rig;
  CHECK(rig.start());
  CHECK_MSG(rig.join(),
            "the client's stage: " + std::string(cn::client_stage_name(rig.remote.end.stage)) +
                " " + rig.remote.end.refused);
  CHECK(said_has(rig.said, "playing on "));
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  CHECK_MSG(ds.map == "arena", "the map read: '" + ds.map + "'");
  CHECK(ds.player == rig.remote.end.playernum);
  // The eye rides the player's own origin as the server sent it.
  for (int f = 0; f < 20; f++) rig.frame();
  const int slot = rig.remote.end.playernum;
  CHECK(slot >= 0);
  const sh::Vec3f at = rig.remote.end.players[slot].origin;
  CHECK_MSG(std::fabs(ds.eye.x - at.x) < 1 && std::fabs(ds.eye.y - at.y) < 1,
            "the eye is not over the player");
  CHECK(cn::remote_status(rig.remote) == "playing on sim:0:27500");
}

TEST(the_view_takes_the_server_s_turn_and_walking_moves_the_eye_the_way_it_faces) {
  Rig rig;
  CHECK(rig.start());
  CHECK(rig.join());
  // The server turns the player (as a teleport does): the view takes its angles.
  cn::SvcMsg turn;
  turn.id = cn::svc::setangle;
  turn.ints = {-1};
  turn.floats = {0, 135, 0};
  cn::reliable_svc(rig.server, rig.remote.end.playernum, turn);
  for (int f = 0; f < 40; f++) rig.frame();
  CHECK_MSG(rig.remote.setangles > 0 && std::fabs(rig.remote.view.y - 135) < 1,
            "the server's turn was not taken: yaw " + std::to_string(rig.remote.view.y));
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  const sh::Vec3f from = ds.eye;
  // Facing +x (yaw 0, level), forward for a second.
  rig.remote.view = {0, 0, 0};
  rig.remote.hands.forward = 400;
  for (int f = 0; f < 77; f++) rig.frame();
  rig.remote.hands.forward = 0;
  for (int f = 0; f < 20; f++) rig.frame();
  const float dx = ds.eye.x - from.x, dy = ds.eye.y - from.y;
  CHECK_MSG(dx > 100 && std::fabs(dy) < dx * 0.25f,
            "walked by " + std::to_string(dx) + ", " + std::to_string(dy));
  // The drawn view is the hands' own, whatever the recording's last command said.
  rig.remote.view = {10, 90, 0};
  rig.frame();
  CHECK(ds.angles.x == 10 && ds.angles.y == 90);
}

TEST(an_impulse_is_sent_once_and_leaving_frees_the_server_s_slot) {
  Rig rig;
  CHECK(rig.start());
  CHECK(rig.join());
  CHECK(cn::classic_count(rig.server, cn::ConnState::spawned) == 1);
  rig.remote.hands.impulse = 7;
  rig.frame();
  CHECK(rig.remote.hands.impulse == 0);
  cn::remote_disconnect(rig.remote);
  CHECK(!rig.remote.active);
  for (int f = 0; f < 20; f++) rig.frame();
  CHECK(cn::classic_count(rig.server, cn::ConnState::connected) == 0);
  // Not connected: a step draws nothing.
  CHECK(rig.frame() == cn::RemoteStep::waiting);
  CHECK(cn::remote_status(rig.remote) == "not connected");
}

TEST(the_player_s_own_moves_are_predicted_where_the_server_later_puts_him) {
  Rig rig;
  rig.latency_ms = 50;
  CHECK(rig.start());
  CHECK(rig.join());
  const std::vector<sh::BrushModel> models = sv::arena_models();
  cn::remote_world(rig.remote, &models);
  const int slot = rig.remote.end.playernum;
  // Each prediction kept by the newest move it ran, then held to the server's answer to that move.
  std::vector<sh::Vec3f> predicted(1 << 16);
  std::vector<bool> kept(1 << 16, false);
  int compared = 0, ahead = 0;
  float worst = 0, lead = 0;
  rig.remote.view = {0, 30, 0};
  for (int f = 0; f < 3 * 77; f++) {
    rig.remote.hands.forward = f < 2 * 77 ? 400 : 0;
    rig.remote.hands.side = f % 77 < 30 ? 200 : 0;
    rig.remote.hands.buttons = f % 40 == 0 ? sh::kButtonJump : 0;
    rig.frame();
    if (!rig.remote.predicted) continue;
    const cn::ClassicClientEnd& c = rig.remote.end;
    const uint32_t newest = c.ch.outgoing_sequence - 1;
    predicted[newest & 0xffff] = rig.remote.prediction;
    kept[newest & 0xffff] = true;
    const uint32_t acked = c.ch.incoming_acknowledged;
    if (kept[acked & 0xffff]) {
      const sh::Vec3f d = predicted[acked & 0xffff] - c.players[slot].origin;
      worst = std::fmax(worst, sh::length(d));
      compared++;
    }
    if (rig.remote.replayed > 0) ahead++;
    lead = std::fmax(lead, sh::length(rig.remote.prediction - c.players[slot].origin));
  }
  CHECK_MSG(compared > 100, "predictions compared: " + std::to_string(compared));
  // The server's state travels on a 1/8 grid and in whole units/s: a prediction may differ by
  // what the grid dropped, grown over the moves replayed.
  CHECK_MSG(worst < 1, "a prediction missed the server's answer by " + std::to_string(worst));
  CHECK_MSG(ahead > 100, "steps a move or more ahead of the server: " + std::to_string(ahead));
  // A round trip of 100 ms at a run is many units: the prediction leads what the server sent.
  CHECK_MSG(lead > 20, "the prediction led the server's place by at most " + std::to_string(lead));
  // The drawn eye is the prediction (the correction aside, which a clean link never makes).
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  CHECK(std::fabs(ds.eye.x - rig.remote.prediction.x) < 1);
}

TEST(cl_nopred_draws_the_player_where_the_server_last_put_him) {
  Rig rig;
  rig.latency_ms = 50;
  CHECK(rig.start());
  CHECK(rig.join());
  const std::vector<sh::BrushModel> models = sv::arena_models();
  cn::remote_world(rig.remote, &models);
  rig.remote.nopred = true;
  rig.remote.view = {0, 0, 0};
  rig.remote.hands.forward = 400;
  for (int f = 0; f < 40; f++) rig.frame();
  CHECK(!rig.remote.predicted);
  const int slot = rig.remote.end.playernum;
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  CHECK(std::fabs(ds.eye.x - rig.remote.end.players[slot].origin.x) < 1);
}

namespace {

// The other player's slot on the window's side (-1: not seen yet).
int other_slot(const Rig& rig) {
  for (int i = 0; i < 32; i++)
    if (i != rig.remote.end.playernum && rig.remote.end.player_seen[i] &&
        rig.remote.end.players[i].has_command)
      return i;
  return -1;
}

struct OtherRun {
  int frames = 0, moved = 0, predicted = 0;
  double raw_err = 0, drawn_err = 0;  // mean distance to the server's place, units
  int most_msec = 0;
};

// The window drawn at 240 frames a second beside the server's 77 for `seconds`: how often the
// other player moved between window frames, and how far his drawn and his framed places sat from
// where the server had him at that moment.
OtherRun watch_other(Rig& rig, double seconds) {
  OtherRun out;
  const int slot = other_slot(rig);
  if (slot < 0) return out;
  const int64_t window_us = 1000000 / 240;
  int64_t wall = rig.now;
  sh::Vec3f last = rig.remote.demo.state.players[static_cast<size_t>(slot)].origin;
  const int64_t end = rig.now + static_cast<int64_t>(seconds * 1e6);
  while (wall < end) {
    wall += window_us;
    while (rig.now + cn::kTickUs <= wall) {
      rig.now += cn::kTickUs;
      gn::sim_advance(rig.net, rig.now);
      cn::classic_frame(rig.server, rig.now);
      cn::client_tick(rig.other, rig.now);
    }
    cn::remote_step(rig.remote, window_us * 1e-6, rig.said);
    const sh::Vec3f drawn = rig.remote.demo.state.players[static_cast<size_t>(slot)].origin;
    const sh::Vec3f framed = rig.remote.end.players[slot].origin;
    const sh::NetPlayer truth = sh::net_player_of(rig.qc->sv, slot);
    const sh::Vec3f at{static_cast<float>(truth.origin8[0]) / 8.0f,
                       static_cast<float>(truth.origin8[1]) / 8.0f,
                       static_cast<float>(truth.origin8[2]) / 8.0f};
    out.frames++;
    if (sh::length(drawn - last) > 0) out.moved++;
    if (rig.remote.others > 0) out.predicted++;
    out.most_msec = std::max(out.most_msec, rig.remote.others_msec);
    out.raw_err += sh::length(framed - at);
    out.drawn_err += sh::length(drawn - at);
    last = drawn;
  }
  if (out.frames > 0) {
    out.raw_err /= out.frames;
    out.drawn_err /= out.frames;
  }
  return out;
}

}  // namespace

TEST(the_other_players_run_ahead_between_the_server_s_frames) {
  Rig rig;
  rig.with_other = true;
  rig.latency_ms = 5;
  CHECK(rig.start());
  CHECK(rig.join());
  for (int f = 0; f < 77 && rig.other.stage != cn::ClientStage::active; f++) rig.frame();
  CHECK(rig.other.stage == cn::ClientStage::active);
  for (int f = 0; f < 40; f++) rig.frame();
  CHECK_MSG(other_slot(rig) >= 0, "the window never saw the other player's command");
  const std::vector<sh::BrushModel> models = sv::arena_models();
  cn::remote_world(rig.remote, &models);
  rig.remote.predict_players = false;
  const OtherRun off = watch_other(rig, 2);
  rig.remote.predict_players = true;
  const OtherRun on = watch_other(rig, 2);
  std::printf(
      "other player at 240 fps: off moved %d/%d err %.2f; on moved %d/%d err %.2f "
      "(framed %.2f) most %d ms\n",
      off.moved, off.frames, off.drawn_err, on.moved, on.frames, on.drawn_err, on.raw_err,
      on.most_msec);
  CHECK(off.predicted == 0);
  CHECK_MSG(on.predicted > on.frames * 9 / 10,
            "frames with the other player run ahead: " + std::to_string(on.predicted));
  // Unpredicted he moves only when a frame arrives (about one window frame in three); run ahead
  // he moves nearly every window frame.
  CHECK_MSG(off.moved < off.frames / 2, "unpredicted moves: " + std::to_string(off.moved));
  CHECK_MSG(on.moved > on.frames * 8 / 10, "predicted moves: " + std::to_string(on.moved));
  CHECK_MSG(on.drawn_err < on.raw_err, "drawn " + std::to_string(on.drawn_err) +
                                           " units from the server's place, framed " +
                                           std::to_string(on.raw_err));
  CHECK(on.most_msec > 0 && on.most_msec <= 255);
  CHECK(rig.remote.end.latency > 0 && rig.remote.end.latency < 0.1);
}

TEST(missiles_are_drawn_between_the_last_two_frames) {
  Rig rig;
  rig.with_other = true;
  rig.latency_ms = 5;
  CHECK(rig.start());
  CHECK(rig.join());
  for (int f = 0; f < 200; f++) rig.frame();
  // The window fires a missile now and then; the scripted player fires on its own.
  int lerped = 0, frames = 0;
  const int64_t window_us = 1000000 / 240;
  int64_t wall = rig.now;
  for (int i = 0; i < 4 * 240; i++) {
    rig.remote.hands.buttons = i % 120 < 4 ? sh::kButtonAttack : 0;
    wall += window_us;
    while (rig.now + cn::kTickUs <= wall) {
      rig.now += cn::kTickUs;
      gn::sim_advance(rig.net, rig.now);
      cn::classic_frame(rig.server, rig.now);
      cn::client_tick(rig.other, rig.now);
    }
    cn::remote_step(rig.remote, window_us * 1e-6, rig.said);
    frames++;
    if (rig.remote.lerped > 0) lerped++;
  }
  CHECK_MSG(lerped > 0, "no entity was drawn between frames in " + std::to_string(frames));
  rig.remote.nolerp = true;
  for (int i = 0; i < 40; i++) rig.frame();
  CHECK(rig.remote.lerped == 0);
}

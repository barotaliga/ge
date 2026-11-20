// Tests: the window joined to a server (games/shooters_edge/net/qw_remote.hpp) over
// engine/net's simulated network: the classic server on the test arena, the remote client driven by
// "hands" instead of a script. It signs on, its recording read back names the map and puts the eye
// on the player, the server's turn (setangle) is taken by the view, walking forward moves the eye
// the way the view faces, an impulse is sent once, and leaving frees the server's slot. With the
// level's solids handed over, the player's own moves are predicted: over a 50 ms link each
// prediction lands where the server later says the player was, and `cl_nopred` draws the server's
// place instead. Beside a scripted player, the other player is run ahead every window frame from
// his newest record, so he moves between the server's frames and sits nearer the server's place
// than the frame put him; missiles are drawn between the last two frames. A mod's named effects
// reach the window by name over the game's own set and are never sent to a client without it.
// On a server with voice the window hears the other player's talk, never its own, and while
// `+voip` is held the microphone's samples it is handed reach the other player.
// Not joined, rcon goes to rcon_address and the answers are said until it falls quiet;
// showpackets, showdrop and cl_solid_players are bound to the window. `record` begun late keeps the
// level from its server data, so the file plays from the signon; a match records itself from the
// status leaving standby to 4 s into the intermission, and one cut short waits for match_save.
//
// ge:summary Tests: the remote client signs on, draws the server's state, walks, and leaves
// ge:keywords test remote-client connect disconnect classic-server live-demo setangle walk
// ge:keywords prediction cl_nopred latency predict_other cl_predict_players cl_nolerp lerp
// ge:keywords item-back items_back
// ge:keywords named-effects ge-ext pointparticles trailparticles voice cl_voip push-to-talk
// ge:skill net shooter testing
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_world.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

namespace {

bool talk(int, int) { return true; }

// The test arena served on a LAN of the simulated network, and one remote client on it.
struct Rig {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;  // over `qc`
  gn::SimNetwork net;
  gn::Transport st, ct, bt;
  cn::QwClientEnd other;  // a scripted player beside the window (`with_other`)
  bool with_other = false;
  bool voice = false;  // the server relays voice to everyone; the window and the other ask for it
  std::string rcon;    // the server's rcon_password ("": none)
  cn::QwServer server;
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
    cn::ServerConfig sc;
    sc.voice = voice;
    sc.rcon_password = rcon;
    sc.voice_route = cn::VoiceRoute::all;
    if (!cn::qw_start(server, rules, st, sc, error)) return false;
    cn::ClientConfig cc;
    cc.name = "window";
    cc.voice = voice;
    cc.live_mic = true;  // as the window joins: its microphone, while +voip is held
    cn::remote_start(remote, ct, gn::sim_address(0, cn::kPortServer), cc);
    if (with_other) {
      gn::open_sim_transport(bt, net, gn::sim_address(2, cn::kPortClient));
      cn::ClientConfig oc;
      oc.name = "runner";
      oc.ask[cn::ExtFamily::ge] = 0;  // as a classic client: the game's own set unknown
      oc.voice = voice;
      oc.talk = talk;  // talks from his first tick when the server has voice
      cn::client_start(other, bt, gn::sim_address(0, cn::kPortServer), oc, 0);
    }
    return true;
  }
  // One server frame and one window frame of the same length; the last step's result.
  cn::RemoteStep frame() {
    now += cn::kTickUs;
    gn::sim_advance(net, now);
    cn::qw_frame(server, now);
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
  CHECK(cn::qw_count(rig.server, cn::ConnState::spawned) == 1);
  rig.remote.hands.impulse = 7;
  rig.frame();
  CHECK(rig.remote.hands.impulse == 0);
  cn::remote_disconnect(rig.remote);
  CHECK(!rig.remote.active);
  for (int f = 0; f < 20; f++) rig.frame();
  CHECK(cn::qw_count(rig.server, cn::ConnState::connected) == 0);
  // Not connected: a step draws nothing.
  CHECK(rig.frame() == cn::RemoteStep::waiting);
  CHECK(cn::remote_status(rig.remote) == "not connected");
}

TEST(a_server_that_drops_the_window_lets_it_go) {
  Rig rig;
  CHECK(rig.start());
  CHECK(rig.join());
  rig.remote.left = false;
  cn::qw_drop(rig.server, rig.remote.end.playernum, "kicked");
  bool dropped = false;
  for (int f = 0; f < 40 && !dropped; f++) dropped = rig.frame() == cn::RemoteStep::dropped;
  CHECK(dropped);
  // Let go, so the window goes back to its own title rather than the server's last frame.
  CHECK(!rig.remote.active && rig.remote.left);
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
    const cn::QwClientEnd& c = rig.remote.end;
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
      cn::qw_frame(rig.server, rig.now);
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
      cn::qw_frame(rig.server, rig.now);
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

TEST(a_mods_named_effects_reach_the_game_s_own_client_by_name_and_no_classic_client) {
  Rig rig;
  rig.with_other = true;
  CHECK(rig.start());
  // The other player is a client that does not know the game's own set (a classic client's ask).
  CHECK(rig.join());
  for (int f = 0; f < 200 && rig.other.stage != cn::ClientStage::active; f++) rig.frame();
  const int me = rig.remote.end.playernum;
  CHECK(me >= 0 && rig.server.clients[me].ext.has(cn::ExtFamily::ge, cn::kGeNamedEffects));
  CHECK(rig.remote.end.ext.has(cn::ExtFamily::ge, cn::kGeNamedEffects));
  sv::NetEvent point;
  point.kind = sv::EventKind::point_effect;
  point.to = ~0u;
  point.origin = {64, 32, 40};
  point.values[0] = 1;
  point.values[3] = 4;
  point.text = "spark_fountain";
  sv::NetEvent trail = point;
  trail.kind = sv::EventKind::trail_effect;
  trail.values[0] = 128, trail.values[1] = 32, trail.values[2] = 48;
  trail.text = "spark_trail";
  const int64_t dropped = rig.server.counts.effects_dropped;
  rig.qc->out.events.push_back(point);
  rig.qc->out.events.push_back(trail);
  std::vector<shooters_edge::client::DemoEffect> seen;
  for (int f = 0; f < 20 && seen.size() < 2; f++) {
    rig.frame();
    for (const auto& e : rig.remote.demo.state.effects)
      if (!e.name.empty()) seen.push_back(e);
  }
  CHECK_MSG(seen.size() == 2, std::to_string(seen.size()) + " named effects read");
  if (seen.size() == 2) {
    CHECK(seen[0].kind == shooters_edge::client::DemoEffectKind::named &&
          seen[0].name == "spark_fountain" && seen[0].count == 4 && seen[0].way.x == 1.0f &&
          seen[0].at.y == 32.0f);
    CHECK(seen[1].kind == shooters_edge::client::DemoEffectKind::named_trail &&
          seen[1].name == "spark_trail" && seen[1].end.x == 128.0f && seen[1].end.z == 48.0f);
  }
  // The other client (a classic one's ask) was sent neither: both counted as dropped for it.
  CHECK_MSG(rig.server.counts.effects_dropped == dropped + 2,
            std::to_string(rig.server.counts.effects_dropped - dropped) + " dropped");
}

// An item taken (hidden by the program, its think set for its return) is told to the game's own
// client with where it lies, its model and when it comes back, so the window draws it coming back
// ahead of time; a classic client beside it is never sent the message and plays on.
TEST(an_item_taken_reaches_the_game_s_own_client_with_when_it_comes_back) {
  Rig rig;
  rig.with_other = true;
  CHECK(rig.start());
  CHECK(rig.join());
  for (int f = 0; f < 200 && rig.other.stage != cn::ClientStage::active; f++) rig.frame();
  CHECK(rig.remote.end.ext.has(cn::ExtFamily::ge, cn::kGeItemBack));
  sv::QcServer& q = *rig.qc;
  const auto& f = q.q.layout->f;
  // A thing of the player's model, hidden as a program hides an item it took.
  const int item = sh::spawn_edict(q.sv);
  CHECK(item > sh::kMaxClients);
  if (item <= sh::kMaxClients) return;
  const auto put = [&](int field, float v) {
    *ge::progs::entity_field(q.q.vm, item, field) = std::bit_cast<uint32_t>(v);
  };
  const int me = rig.remote.end.playernum + 1;
  const int model =
      static_cast<int>(std::bit_cast<float>(*ge::progs::entity_field(q.q.vm, me, f.modelindex)));
  CHECK(model > 0);
  put(f.modelindex, static_cast<float>(model));
  put(f.flags, 256);
  put(f.nextthink, static_cast<float>(q.sv.time) + 2.0f);
  *ge::progs::entity_field(q.q.vm, item, f.model) = 0;     // its text emptied: hidden
  sh::Edict& ed = q.sv.edicts[static_cast<size_t>(item)];  // the loop's copy, as a frame keeps it
  ed.modelindex = model;
  ed.flags = 256;
  ed.nextthink = static_cast<float>(q.sv.time) + 2.0f;
  const shooters_edge::client::DemoItemBack* told = nullptr;
  for (int k = 0; k < 40 && told == nullptr; k++) {
    rig.frame();
    for (const auto& b : rig.remote.demo.state.items_back)
      if (b.entity == item) told = &b;
  }
  CHECK(told != nullptr);
  if (told != nullptr) {
    CHECK(told->model == model);
    const double left = told->at - rig.remote.demo.state.time;
    CHECK_MSG(left > 1.8 && left <= 2.0, std::to_string(left));
  }
  for (int k = 0; k < 20; k++) rig.frame();
  CHECK(rig.other.stage == cn::ClientStage::active);
}

TEST(the_window_hears_the_other_player_talk_and_its_microphone_is_heard_while_voip_is_held) {
  Rig rig;
  rig.with_other = true;
  rig.voice = true;
  ge::console::Console con;
  cn::add_remote_commands(con, rig.remote);
  CHECK(rig.start());
  CHECK(rig.join());
  for (int f = 0; f < 200 && rig.other.stage != cn::ClientStage::active; f++) rig.frame();
  const int me = rig.remote.end.playernum;
  const int other = rig.other.playernum;
  CHECK(me >= 0 && other >= 0 && rig.remote.end.ext.has(cn::ExtFamily::x2, cn::kX2Voice));
  // A tone of a microphone, handed in every frame (13 ms of it); silent until +voip is held.
  std::vector<float> tone(static_cast<size_t>(48000 * cn::kTickUs / 1000000));
  for (size_t i = 0; i < tone.size(); i++)
    tone[i] = (i / 24) % 2 == 0 ? 0.25f : -0.25f;  // a 1 kHz square
  int64_t heard_other = 0, heard_self = 0;
  const auto run = [&](int frames) {
    for (int f = 0; f < frames; f++) {
      cn::remote_mic(rig.remote, tone);
      rig.frame();
      for (const auto& v : rig.remote.demo.state.voice)
        (v.sender == other ? heard_other : heard_self)++;
    }
  };
  run(80);
  CHECK_MSG(heard_other > 0, "the window heard no voice from the other player");
  CHECK_MSG(rig.other.voice_heard[me] == 0, "the window talked without +voip held");
  CHECK(!cn::remote_talking(rig.remote));
  con.execute("+voip");
  CHECK(cn::remote_talking(rig.remote));
  run(80);
  CHECK_MSG(rig.other.voice_heard[me] > 0, "the other player heard nothing of the window's mic");
  CHECK_MSG(heard_self == 0, std::to_string(heard_self) + " of the window's own voice played back");
  con.execute("-voip");
  const int64_t sent = rig.remote.end.voice_sent;
  run(20);
  CHECK(rig.remote.end.voice_sent == sent);
}

TEST(the_move_rate_is_the_servers_lowered_by_cl_physfps) {
  auto c = std::make_unique<cn::QwClientEnd>();
  CHECK(cn::client_move_rate(*c) == 72);  // a server info naming no maxfps
  c->serverinfo = "\\maxfps\\77\\hostname\\x";
  CHECK(cn::client_move_rate(*c) == 77);
  c->cfg.physfps = 50;
  CHECK(cn::client_move_rate(*c) == 50);
  c->cfg.physfps = 120;  // never more than the server allows
  CHECK(cn::client_move_rate(*c) == 77);
  c->cfg.physfps = 4;  // never under 10
  CHECK(cn::client_move_rate(*c) == 10);
  c->cfg.spectator = true;  // watching: cl_physfps_spectator in its place
  c->cfg.physfps_spectator = 30;
  CHECK(cn::client_move_rate(*c) == 30);
  // At 77 a second the moves are 12 and 13 ms with the fraction carried: a second each 77.
  ge::shooter::CmdClock clock;
  clock.period = 1.0 / 77;
  int sum = 0, twelves = 0;
  for (int i = 0; i < 77; ++i) {
    const int ms = ge::shooter::next_msec(clock);
    sum += ms;
    twelves += ms == 12 ? 1 : 0;
  }
  CHECK_MSG(sum == 1000 && twelves == 1, std::to_string(sum) + " ms, " + std::to_string(twelves));
}

TEST(cl_physfps_sets_the_moves_sent_a_second_on_a_server_joined) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  CHECK(con.cvars().find("cl_physfps")->layer == "classic");
  con.cvars().set("cl_physfps", "40", ge::console::Source::local);
  CHECK(r.physfps == 40);
  CHECK(con.cvars().find("cl_physfps_spectator")->value == "77");
}

TEST(cl_c2spps_holds_moves_back_to_its_packet_rate_never_one_with_an_impulse) {
  auto c = std::make_unique<cn::QwClientEnd>();
  CHECK(!cn::client_hold_move(*c, 1.0 / 72));  // 0: one packet a move
  c->cfg.c2spps = 24;
  int sent = 0, run = 0, longest = 0;
  for (int i = 0; i < 720; i++) {  // ten seconds of 72 moves a second
    const bool held = cn::client_hold_move(*c, 1.0 / 72);
    sent += held ? 0 : 1;
    run = held ? run + 1 : 0;
    longest = std::max(longest, run);
  }
  CHECK_MSG(sent >= 235 && sent <= 245, std::to_string(sent) + " packets in ten seconds");
  CHECK(longest == cn::kMostHeld);
  c->cfg.c2spps = 1000;  // more is 72: every move of 72 a second sent
  c->send_balance = 0;
  sent = 0;
  for (int i = 0; i < 72; i++) sent += cn::client_hold_move(*c, 1.0 / 72) ? 0 : 1;
  CHECK_MSG(sent >= 71, std::to_string(sent) + " of 72");
  // A weapon key in a move the packet would carry: never held back (cl_c2sImpulseBackup 3).
  c->cfg.c2spps = 10;
  c->send_balance = -0.1;
  c->held = 0;
  c->cmds[0].impulse = 7;
  CHECK(!cn::client_hold_move(*c, 1.0 / 72));
  c->cfg.c2s_impulse_backup = 2;  // only the new move and the one before checked
  CHECK(cn::client_hold_move(*c, 1.0 / 72));
}

TEST(a_window_at_cl_c2spps_sends_fewer_packets_and_the_server_still_runs_every_move) {
  Rig rig;
  CHECK(rig.start());
  CHECK(rig.join());
  ge::console::Console con{false};
  cn::add_remote_commands(con, rig.remote);
  con.cvars().set("cl_c2spps", "20", ge::console::Source::local);
  CHECK(rig.remote.c2spps == 20 && con.cvars().find("cl_c2spps")->layer == "classic");
  CHECK(con.cvars().find("cl_c2sImpulseBackup")->value == "3");
  const shooters_edge::client::DemoState& ds = rig.remote.demo.state;
  rig.remote.view = {0, 0, 0};
  rig.frame();
  const sh::Vec3f from = ds.eye;
  const uint64_t packets = rig.remote.end.ch.sent;
  const int64_t held = rig.remote.end.moves_held;
  rig.remote.hands.forward = 400;
  for (int f = 0; f < 77; f++) rig.frame();
  rig.remote.hands.forward = 0;
  for (int f = 0; f < 20; f++) rig.frame();
  const uint64_t sent = rig.remote.end.ch.sent - packets;
  CHECK_MSG(sent < 60 && rig.remote.end.moves_held - held > 30,
            std::to_string(sent) + " packets for 98 moves");
  // The held moves rode in the next packets: the walk is as far as at a packet a move.
  const float dx = ds.eye.x - from.x;
  CHECK_MSG(dx > 100, "walked by " + std::to_string(dx));
}

TEST(rcon_goes_to_rcon_address_while_not_joined_and_its_answers_are_said) {
  Rig rig;
  rig.rcon = "pw";
  CHECK(rig.start());
  cn::remote_disconnect(rig.remote);  // not joined
  cn::Remote& r = rig.remote;
  std::string error;
  CHECK(!cn::remote_rcon(r, "", "rcon \"pw\" status", error));
  CHECK(error == "You must either be connected or set rcon_address to send rcon commands.");
  CHECK(!cn::remote_rcon(r, "sim:", "rcon \"pw\" status", error) && error == "Bad address: sim:");
  gn::open_sim_transport(rig.bt, rig.net, gn::sim_address(2, cn::kPortClient));
  r.away = std::make_unique<cn::RconAway>();
  r.away->link = &rig.bt;
  CHECK(cn::remote_rcon(r, "sim:0:27500", "rcon \"pw\" serverinfo", error));
  std::vector<std::string> lines;
  for (int f = 0; f < 40; f++) {
    rig.now += cn::kTickUs;
    gn::sim_advance(rig.net, rig.now);
    cn::qw_frame(rig.server, rig.now);
    cn::remote_rcon_step(r, static_cast<double>(rig.now) * 1e-6, lines);
  }
  CHECK_MSG(!lines.empty(), "no answer to rcon");
  std::string all;
  for (const std::string& l : lines) all += l + "\n";
  CHECK_MSG(all.find("Bad rcon_password") == std::string::npos, all);
  CHECK(r.away->until_s > 0);  // still listening
  cn::remote_rcon_step(r, static_cast<double>(rig.now) * 1e-6 + cn::kRconQuietS + 1, lines);
  CHECK(r.away->until_s < 0);  // quiet: closed
}

TEST(showpackets_showdrop_and_cl_solid_players_are_the_classic_layer_s) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  con.cvars().set("showpackets", "3", ge::console::Source::local);
  con.cvars().set("showdrop", "1", ge::console::Source::local);
  CHECK(r.trace.packets == 3 && r.trace.drop);
  CHECK(con.cvars().find("showpackets")->layer == "classic");
  CHECK(con.cvars().find("rcon_address")->value.empty());
  con.cvars().set("cl_solid_players", "0", ge::console::Source::local);
  CHECK(!r.solid_players);
  r.trace.print("a line");  // printed on the console
}

TEST(record_while_joined_keeps_the_level_from_its_start_and_a_match_records_itself) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "ge_remote_record_test";
  fs::remove_all(dir);
  Rig rig;
  CHECK(rig.start());
  ge::console::Console con{false};
  cn::Remote& r = rig.remote;
  cn::add_remote_commands(con, r);
  r.joined_record.folder = [dir] { return dir.string(); };
  CHECK(rig.join());
  for (int f = 0; f < 60; f++) rig.frame();
  std::string error;
  CHECK_MSG(cn::remote_record_open(r, dir / "late.qwd", error), error);
  for (int f = 0; f < 60; f++) rig.frame();
  const std::string line = cn::remote_record_close(r, con, 0);
  CHECK_MSG(line.rfind("Recorded ", 0) == 0, line);
  // Begun late, the file still starts at the level's server data: it plays from the signon.
  std::ifstream in(dir / "late.qwd", std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  CHECK(bytes.size() > 1000);
  shooters_edge::client::Demo d;
  CHECK_MSG(shooters_edge::client::open_demo(bytes, d, error), error);
  int frames = 0;
  while (shooters_edge::client::next_frame(d, error) == shooters_edge::client::DemoRead::frame)
    frames++;
  CHECK_MSG(frames > 60 && d.state.map == "arena", error + " map " + d.state.map);
  // The match on its own: begun when the status leaves standby, saved 4 s into the intermission.
  con.cvars().set("match_auto_record", "2", ge::console::Source::local);
  cn::info_set(r.end.serverinfo, "status", "Standby");
  cn::remote_match_frame(r, con, 1);
  CHECK(!r.match.running);
  cn::info_set(r.end.serverinfo, "status", "Countdown");
  cn::remote_match_frame(r, con, 2);
  CHECK(r.match.running && !r.file.path.empty());
  for (int f = 0; f < 30; f++) rig.frame();
  r.demo.state.intermission = true;
  cn::remote_match_frame(r, con, 3);
  CHECK(r.match.running);  // the scores still to come
  cn::remote_match_frame(r, con, 7.5);
  CHECK(!r.match.running && r.file.path.empty());
  CHECK(fs::exists(dir / "solo" / "window - [arena]_000.qwd"));
  // Cut short under the shortest length: not saved, kept for match_save.
  r.demo.state.intermission = false;
  cn::info_set(r.end.serverinfo, "status", "Standby");
  cn::remote_match_frame(r, con, 10);
  cn::info_set(r.end.serverinfo, "status", "10 min left");
  cn::remote_match_frame(r, con, 11);
  CHECK(r.match.running);
  cn::info_set(r.end.serverinfo, "status", "Standby");
  cn::remote_match_frame(r, con, 12);
  CHECK(!r.match.running && r.match.waiting);
  CHECK(!fs::exists(dir / "solo" / "window - [arena]_001.qwd"));
  con.run_now("match_save");
  CHECK(fs::exists(dir / "solo" / "window - [arena]_001.qwd"));
  fs::remove_all(dir);
}

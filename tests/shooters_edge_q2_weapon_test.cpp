// Quake II's held weapon predicted on the joined window (net/q2_weapon.hpp), what every game's
// prediction shares (net/weapon_shown.hpp): the hand a frame shows, the hand run over moves and
// server frames (a stock server's press kept until a shot, ours stepped by any move with attack
// down), and the window joined to our server firing ahead of the round trip.
//
// ge:summary Tests: Quake II's weapon predicted on the joined window, and the shared helpers
// ge:keywords test weapon-prediction q2 q2_hand_move shown-command sound-mask press-latch
// ge:skill net testing
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/q2_weapon.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/weapon_shown.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;

constexpr int kRocket = static_cast<int>(sv::SqWeapon::rocket_launcher);
constexpr int kMachinegun = static_cast<int>(sv::SqWeapon::machinegun);

TEST(the_shown_command_is_held_back_by_the_buffer_within_the_moves_in_flight) {
  CHECK(cn::weapon_shown_command(10, 20, 1) == 18);
  CHECK(cn::weapon_shown_command(10, 20, 0) == 19);
  CHECK(cn::weapon_shown_command(10, 20, 9) == 19);  // before the second after the answered
  CHECK(cn::weapon_shown_command(10, 20, 7) == 12);
}

TEST(the_sound_mask_names_the_weapons_left_to_the_server_by_their_keys) {
  CHECK(!cn::weapon_sound_plays(0, 1));
  CHECK(cn::weapon_sound_plays(1, 7));
  CHECK(!cn::weapon_sound_plays(2 | 128, 7));  // 128: key 7 (the rocket launcher) the server's
  CHECK(cn::weapon_sound_plays(2 | 128, 9));
  CHECK(!cn::weapon_sound_plays(1024, 10));  // Quake II's BFG10K on key 0, the tenth
}

TEST(a_shot_moved_by_a_correction_is_played_once) {
  float last[4] = {};
  CHECK(cn::weapon_shot_fresh(last, 4, 1, 2.0f, 0.09f));
  CHECK(!cn::weapon_shot_fresh(last, 4, 1, 2.05f, 0.09f));
  CHECK(cn::weapon_shot_fresh(last, 4, 2, 2.05f, 0.09f));  // another kind
  CHECK(cn::weapon_shot_fresh(last, 4, 1, 2.1f, 0.09f));   // the next shot, a frame on
}

TEST(a_frame_names_the_hand_s_state_by_the_weapon_s_frame_runs) {
  // The rocket launcher: activate 0..4, fire 5..12, idle 13..50, deactivate 51..54.
  CHECK(cn::q2_hand_of_frame(kRocket, 2, -1).state == sv::SqHandState::activating);
  CHECK(cn::q2_hand_of_frame(kRocket, 7, -1).state == sv::SqHandState::firing);
  CHECK(cn::q2_hand_of_frame(kRocket, 20, -1).state == sv::SqHandState::ready);
  CHECK(cn::q2_hand_of_frame(kRocket, 52, -1).state == sv::SqHandState::dropping);
  // The frame after the fire run: firing after a fire frame, ready after an idle one.
  CHECK(cn::q2_hand_of_frame(kRocket, 13, 12).state == sv::SqHandState::firing);
  CHECK(cn::q2_hand_of_frame(kRocket, 13, 50).state == sv::SqHandState::ready);
  CHECK(cn::q2_weapon_of_model("models/weapons/v_rocket/tris.md2") == kRocket);
  CHECK(cn::q2_weapon_of_model("models/weapons/v_mod/tris.md2") == -1);
}

cn::Q2HandRun ready_run(int weapon, int ammo, bool latch) {
  cn::Q2HandRun r;
  r.hand = cn::q2_hand_of_frame(weapon, sv::kSqWeapons[weapon].last_fire + 2, -1);
  r.body.ammo[static_cast<int>(sv::kSqWeapons[weapon].ammo)] = ammo;
  if (sv::kSqWeapons[weapon].item != nullptr)
    r.body.counts[sv::sq_item(sv::kSqWeapons[weapon].item)] = 1;
  r.latch = latch;
  r.frame_at = 10.0;
  return r;
}

// Moves of `msec` with attack as `attack(k)`; the level time of each shot.
std::vector<double> run_moves(cn::Q2HandRun& r, int moves, int msec, auto attack) {
  std::vector<double> at;
  for (int k = 0; k < moves; k++) {
    cn::Q2HandShot s[8];
    const int n = cn::q2_hand_move(r, 10.0 + k * msec * 0.001, attack(k), s, 8);
    for (int i = 0; i < n; i++) at.push_back(s[i].at);
  }
  return at;
}

TEST(a_press_fires_at_once_mid_frame_and_held_fire_steps_with_the_server_s_frames) {
  for (const bool latch : {true, false}) {
    cn::Q2HandRun r = ready_run(kRocket, 5, latch);
    // 16 ms moves; attack down from the fourth (48 ms into the frame) and held for 2 s.
    const auto at = run_moves(r, 125, 16, [](int k) { return k >= 3; });
    CHECK(!at.empty());
    CHECK_MSG(at[0] > 10.047 && at[0] < 10.049, std::to_string(at[0]));
    // The rocket's run: 8 frames from the shot to ready again, the next shot a frame later on the
    // frame's start (the first move of a frame on our server, the frame itself on a stock one).
    CHECK(at.size() >= 2);
    const double gap = at[1] - at[0];
    // Stock: the next frame's step was the press's, so the run's steps fall on 10.2, 10.3 ..., and
    // the second shot on 11.0; ours: on each frame's first move, 10.912.
    CHECK_MSG(gap > (latch ? 0.951 : 0.863) && gap < (latch ? 0.953 : 0.865), std::to_string(gap));
    CHECK(r.body.ammo[static_cast<int>(sv::SqAmmo::rockets)] == 5 - static_cast<int>(at.size()));
  }
}

TEST(a_tap_during_a_shot_is_lost_and_so_is_a_second_tap_in_one_frame) {
  for (const bool latch : {true, false}) {
    cn::Q2HandRun r = ready_run(kRocket, 5, latch);
    // A tap at once, then another 300 ms later while the first shot's run plays: lost either way.
    CHECK(run_moves(r, 100, 10, [](int k) { return k == 0 || k == 30; }).size() == 1);
  }
  // A stock server: a tap made after a move already stepped the hand this frame is dropped at the
  // frame's start, whose step it already had; the button up by then, nothing fires.
  cn::Q2HandRun r = ready_run(kMachinegun, 50, true);
  cn::Q2HandShot s[8];
  CHECK(cn::q2_hand_move(r, 10.02, false, s, 8) == 0);
  r.stepped = true;  // a move stepped it this frame (a weapon key's press, say)
  CHECK(cn::q2_hand_move(r, 10.03, true, s, 8) == 0);
  int later = 0;
  for (int k = 4; k < 14; k++) later += cn::q2_hand_move(r, 10.0 + k * 0.01, false, s, 8);
  CHECK(later == 0);
  CHECK(!r.pressed);
}

TEST(the_frames_offset_narrows_frame_by_frame_and_starts_again_when_a_frame_disagrees) {
  cn::Q2FramePhase p;
  // Packets every 48 ms; the server's frames start 30 ms after a packet sent at frame × 0.1 + 0.5.
  cn::q2_phase_frame(p, 10, 1.48, 1.528);
  CHECK(p.lo > 0.479 && p.hi < 0.529);
  cn::q2_phase_frame(p, 11, 1.576, 1.624);  // agrees: 0.476..0.524 narrows it to 0.48..0.524
  CHECK(p.lo > 0.479 && p.lo < 0.481 && p.hi > 0.523 && p.hi < 0.525);
  cn::q2_phase_frame(p, 12, 1.70, 1.748);  // the latency grew: its own window
  CHECK(p.lo > 0.499 && p.lo < 0.501);
  CHECK(cn::q2_phase_offset(p) > 0.5 && cn::q2_phase_offset(p) < 0.525);
}

TEST(the_machinegun_fires_a_bullet_every_frame_while_held_and_stops_short_of_ammo) {
  cn::Q2HandRun r = ready_run(kMachinegun, 4, true);
  const auto at = run_moves(r, 100, 10, [](int) { return true; });
  CHECK_MSG(at.size() == 4, std::to_string(at.size()) + " shots");
  // The press's step stands for its frame's, so the second bullet waits 0.2 s, the rest 0.1 s.
  CHECK(at[1] - at[0] > 0.199 && at[1] - at[0] < 0.201);
  for (size_t i = 2; i < at.size(); i++)
    CHECK(at[i] - at[i - 1] > 0.099 && at[i] - at[i - 1] < 0.101);
  // Out of bullets the hand is held: no weapon change guessed.
  CHECK(r.hand.weapon == kMachinegun);
  CHECK(r.hand.pending < 0);
}

// THE WINDOW JOINED TO OUR SERVER OF QUAKE II, on a link of 60 ms each way.
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
  br::Entity e;
  e.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}};
  w.entities.push_back(e);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Joined {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::Q2NetServer server;
  cn::Remote remote;
  std::vector<std::string> said;
  int64_t now = 0, wall = 0;
};

std::unique_ptr<Joined> join(bool predict) {
  auto j = std::make_unique<Joined>();
  j->rules = sv::q2_runner(sv::make_q2_server({}));
  j->rules.console("deathmatch 1");
  j->rules.console("sv_fps 10");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(j->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset("lan", link));
  link.latency_ms = 60;
  link.jitter_ms = 0;
  j->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(j->st, j->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(j->server, j->rules, j->st, {}, error), error);
  gn::open_sim_transport(j->ct, j->net, gn::sim_address(9, 27901));
  j->remote.q2_game = [] { return true; };
  j->remote.read_file = [](const std::string&, const std::string&) {
    return std::vector<uint8_t>{1};
  };
  j->remote.weapon.off = !predict;
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(j->remote, j->ct, gn::sim_address(0, cn::kPortQ2), cfg);
  return j;
}

// One window frame of 8 ms; true when the player's own blaster shot was heard in it.
bool frame(Joined& j) {
  j.wall += 8000;
  while (j.now + cn::kTickUs <= j.wall) {  // the server's host ticks
    j.now += cn::kTickUs;
    gn::sim_advance(j.net, j.now);
    cn::q2_net_frame(j.server, j.now);
  }
  cn::remote_step(j.remote, 0.008, j.said);
  const cl::DemoState& d = j.remote.demo.state;
  for (const cl::DemoSound& s : d.started)
    if (s.entity == d.player + 1 && s.name != nullptr &&
        std::string(s.name) == "weapons/blastf1a.wav")
      return true;
  return false;
}

// The milliseconds from a press to the shot heard, and the shots heard over 2 s of one tap.
std::pair<int, int> press(bool predict) {
  auto j = join(predict);
  for (int i = 0; i < 400; i++) frame(*j);  // in, and the blaster up
  CHECK_MSG(j->remote.q2 && j->remote.q2->stage == cn::ClientStage::active, j->remote.q2->refused);
  const int shots0 = j->remote.demo.state.own_shots;
  j->remote.hands.buttons = sh::kButtonAttack;
  int first = -1, heard = 0;
  for (int i = 0; i < 250; i++) {
    if (i == 6) j->remote.hands.buttons = 0;
    if (frame(*j)) {
      heard++;
      if (first < 0) first = i * 8;
    }
  }
  CHECK(j->remote.q2_weapon.on == predict);
  CHECK((j->remote.demo.state.own_shots > shots0) == predict);
  return {first, heard};
}

TEST(the_joined_window_hears_its_own_shot_with_the_press_not_a_round_trip_later) {
  const auto off = press(false);
  const auto on = press(true);
  CHECK_MSG(off.first >= 120, std::to_string(off.first) + " ms off");
  CHECK_MSG(on.first >= 0 && on.first <= 24, std::to_string(on.first) + " ms on");
  // One tap, one shot either way: the server's copy of the predicted one is not heard again.
  CHECK_MSG(off.second == 1 && on.second == 1,
            std::to_string(off.second) + " off, " + std::to_string(on.second) + " on");
}

}  // namespace

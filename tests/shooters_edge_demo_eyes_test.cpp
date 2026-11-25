// The classic view rebuilt on a recording's eye (games/shooters_edge/client/demo_eyes.hpp): a
// written multiview walk strafing to the left rolls the view left side down and bobs the eye by
// the velocity estimated from the frames read; a client recording's sent velocity is the one
// used; a blow tips the view toward where it came from and fades; dead the view lies on its side;
// a game joined moves by its prediction; Quake II's recordings and the intermission are drawn
// as read; a seek back starts the rig over.
//
// ge:summary Tests: the classic bob, roll and kick on a demo's eye, estimated and sent velocity
// ge:keywords test demo-eyes classic-view bob strafe-roll damage-kick dead-roll multiview
// ge:skill shooter camera testing
// ge:test tests/shooters_edge_demo_eyes_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_eyes.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/view.hpp"

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace cam = ge::camera;

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180;
const double kFov = cam::FirstPersonRow{}.fov_y;

cam::FirstPersonRow classic() { return cam::classic_first_person(ge::brush::kUnitM); }

// A frame state by hand: a client recording, the player in slot 0 at `origin` looking along +x.
cl::DemoState client_state(sh::Vec3f origin, double time, int64_t frame) {
  cl::DemoState s;
  s.format = cl::DemoFormat::client;
  s.protocol = 28;
  s.levels = 1;
  s.player = 0;
  s.time = time;
  s.frames = frame;
  s.players.resize(32);
  s.players[0].origin = origin;
  s.players[0].present = true;
  s.eye = origin;
  s.eye.z += s.view_height;
  s.stats[cl::kStatHealth] = 100;
  return s;
}

}  // namespace

TEST(a_multiview_strafe_rolls_and_bobs_by_the_estimated_velocity) {
  // Looking along +x, stepping +y (to the left) 4 units a frame at 72 frames a second: 288 u/s.
  cl::DemoScript sc;
  sc.frame_s = 0.014f;  // whole milliseconds: the multiview clock's
  for (int i = 0; i < 120; i++) {
    sc.origins.push_back({0, static_cast<float>(i) * 4, 24});
    sc.angles.push_back({0, 0, 0});
  }
  cl::Demo d;
  std::string error;
  CHECK(cl::open_demo(cl::write_multiview_demo(sc), d, error));
  cl::DemoEyes e;
  double lo = 1e9, hi = -1e9, roll = 0;
  int frames = 0;
  for (cl::DemoRead r = cl::next_frame(d, error); r != cl::DemoRead::end && frames < 110;
       r = cl::next_frame(d, error)) {
    if (r != cl::DemoRead::frame || d.state.map.empty() || !cl::demo_eyes_drawn(d.state)) continue;
    const ge::render::FrameView v = cl::demo_eyes_view(d.state, d.state, e, classic(), nullptr, {},
                                                       kFov, d.state.time, d.state.time);
    if (frames++ < 20) continue;  // the estimate and the roll settle
    lo = std::min(lo, v.eye[1]);
    hi = std::max(hi, v.eye[1]);
    roll = v.roll;
  }
  CHECK(frames > 60);
  // Moving left: the roll puts the left side down (negative), at the classic 2 degrees' share of
  // 288 over 200 units a second (capped at 2).
  CHECK(roll < 0);
  CHECK(std::fabs(std::fabs(roll) - 2 * kDeg) < 0.05 * kDeg);
  // The bob: 0.02 of 288 u/s, from 0.3 - 0.7 to 0.3 + 0.7 of it clamped to -7..+4 units.
  const double span_units = (hi - lo) / ge::brush::kUnitM;
  CHECK(span_units > 6 && span_units < 11.5);
}

TEST(a_client_recordings_sent_velocity_is_the_one_used) {
  cl::DemoEyes e;
  ge::render::FrameView v;
  for (int i = 0; i < 40; i++) {
    cl::DemoState s = client_state({0, 0, 24}, i / 72.0, i + 1);  // standing still...
    s.velocity = {0, -320, 0};                                    // ...but sent moving right
    s.velocity_read = true;
    v = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, s.time, s.time);
  }
  CHECK(v.roll > 0);  // moving right: right side down
  CHECK(std::fabs(v.roll - 2 * kDeg) < 1e-9);
}

TEST(a_blow_tips_the_view_toward_it_and_fades) {
  cl::DemoEyes e;
  cl::DemoState s = client_state({0, 0, 24}, 0, 1);
  ge::render::FrameView v = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  CHECK(std::fabs(v.roll) < 1e-12 && std::fabs(v.pitch) < 1e-12);
  // A rocket from the player's right (-y) for 20 armour and 40 blood: count 30.
  s = client_state({0, 0, 24}, 1.0 / 72, 2);
  cl::take_blow(s, {20, 40, {0, -200, 24}});
  v = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, s.time, s.time);
  const double first = v.roll;
  CHECK(std::fabs(first) > 1 * kDeg);
  // The same frame drawn again: the blow is read once.
  v = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, s.time + 0.005, s.time);
  CHECK(std::fabs(v.roll) <= std::fabs(first) + 1e-12);
  // Faded after the kick time (0.5 s).
  for (int i = 3; i < 80; i++) {
    s = client_state({0, 0, 24}, i / 72.0, i);
    v = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, s.time, s.time);
  }
  CHECK(std::fabs(v.roll) < 1e-9 && std::fabs(v.pitch) < 1e-9);
}

TEST(dead_the_view_lies_on_its_side_low) {
  cl::DemoEyes e;
  cl::DemoState s = client_state({0, 0, 24}, 0, 1);
  s.players[0].dead = true;
  const ge::render::FrameView v = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  CHECK(std::fabs(v.roll - 80 * kDeg) < 1e-9);
  // The eye 16 units under the origin.
  CHECK(std::fabs(v.eye[1] - 8 * ge::brush::kUnitM) < 1e-6);
}

TEST(a_game_joined_moves_by_its_prediction) {
  cl::DemoEyes e;
  cl::DemoBody body;
  body.known = true;
  body.velocity = {0, 320, 0};
  ge::render::FrameView v;
  for (int i = 0; i < 40; i++) {
    cl::DemoState s = client_state({0, 0, 24}, i / 72.0, i + 1);
    s.velocity = {0, -320, 0};  // the server's, older: the prediction wins
    s.velocity_read = true;
    v = cl::demo_eyes_view(s, s, e, classic(), nullptr, body, kFov, s.time, s.time);
  }
  CHECK(v.roll < 0);
}

TEST(q2_and_the_intermission_are_drawn_as_read) {
  cl::DemoEyes e;
  cl::DemoState s = client_state({0, 0, 24}, 0, 1);
  s.format = cl::DemoFormat::q2;
  s.kick = {0, 0, 3};
  CHECK(!cl::demo_eyes_drawn(s));
  ge::render::FrameView a = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  ge::render::FrameView b = cl::demo_view(s, kFov, 0);
  CHECK(a.roll == b.roll && a.eye[1] == b.eye[1]);
  s = client_state({0, 0, 24}, 0, 1);
  s.intermission = true;
  CHECK(!cl::demo_eyes_drawn(s));
}

TEST(a_seek_back_starts_the_rig_over) {
  cl::DemoEyes e;
  for (int i = 0; i < 30; i++) {
    cl::DemoState s = client_state({0, 0, 24}, 1 + i / 72.0, i + 1);
    s.velocity = {0, -320, 0};
    s.velocity_read = true;
    cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, s.time, s.time);
  }
  CHECK(e.eyes.clock > 0);
  cl::DemoState s = client_state({0, 0, 24}, 0.5, 31);
  cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0.5, 0.5);
  CHECK(e.eyes.clock == 0);
}

TEST(a_servers_kick_ramps_out_and_back_and_is_drawn_only_with_v_gunkick) {
  cl::DemoEyes e;
  cl::DemoState s;
  cl::demo_eyes_kick(s, e, 0);
  s.last_kick = 2;  // a small kick: -2 in 0.1 s, back in 0.1 s
  s.kicks_taken = 1;
  cl::demo_eyes_kick(s, e, 0.05);
  CHECK(std::fabs(e.kick + 1) < 1e-6);
  cl::demo_eyes_kick(s, e, 0.05);
  CHECK(std::fabs(e.kick + 2) < 1e-6 && e.kick_to == 0);
  cl::demo_eyes_kick(s, e, 0.1);
  CHECK(e.kick == 0);
  s.last_kick = 4;  // a big kick: -4 in 0.1 s, back in 0.2 s
  s.kicks_taken = 2;
  cl::demo_eyes_kick(s, e, 0.1);
  CHECK(std::fabs(e.kick + 4) < 1e-6);
  cl::demo_eyes_kick(s, e, 0.1);
  CHECK(std::fabs(e.kick + 2) < 1e-5);
  cl::demo_eyes_kick(s, e, 0.1);
  CHECK(e.kick == 0);

  // On a view: off by default, the kick tracked but not drawn; on, the eye back and pitched up.
  for (const bool on : {false, true}) {
    cl::DemoEyes v_e;
    v_e.gun_kick = &on;
    cl::DemoState f = client_state({0, 0, 24}, 0, 1);
    const ge::render::FrameView still =
        cl::demo_eyes_view(f, f, v_e, classic(), nullptr, {}, kFov, 0, 0);
    f = client_state({0, 0, 24}, 0.05, 2);
    f.last_kick = 4;
    f.kicks_taken = 1;
    const ge::render::FrameView kicked =
        cl::demo_eyes_view(f, f, v_e, classic(), nullptr, {}, kFov, 0.05, 0.05);
    CHECK(std::fabs(v_e.kick + 2) < 1e-5);
    const double up = kicked.pitch - still.pitch;
    CHECK_MSG(on ? std::fabs(up - 1 * kDeg) < 1e-6 : std::fabs(up) < 1e-12,
              "pitched by " + std::to_string(up / kDeg) + " degrees");
  }
}

TEST(cam_thirdperson_sits_behind_the_watched_player_and_draws_him) {
  // Looking along +x: 100 units back is 100 units down x; looking 45 degrees down, back and up.
  cl::DemoState s = client_state({0, 0, 24}, 0, 1);
  s.players[0].model = 1;
  s.models = {"", "progs/player.mdl"};
  s.format = cl::DemoFormat::q2;  // drawn as read, so the eye is the recorded one
  ge::render::FrameView own = cl::demo_view(s, kFov, 0);
  cl::DemoEyes e;
  e.behind = 100;
  ge::render::FrameView back = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  {  // the rig's own start (a cut) keeps the distance
    cl::DemoState c = client_state({0, 0, 24}, 0, 1);
    cl::DemoEyes r;
    r.behind = 100;
    ge::render::FrameView near = cl::demo_eyes_view(c, c, r, classic(), nullptr, {}, kFov, 0, 0);
    r.behind = 0;
    r.clock = -1;
    ge::render::FrameView eyes = cl::demo_eyes_view(c, c, r, classic(), nullptr, {}, kFov, 0, 0);
    CHECK(std::abs(near.eye[0] - eyes.eye[0]) + std::abs(near.eye[2] - eyes.eye[2]) > 2);
  }
  const cam::Vec3 d0 = cl::engine_point({-100, 0, 0}), o = cl::engine_point({0, 0, 0});
  CHECK(std::abs((back.eye[0] - own.eye[0]) - (d0.x - o.x)) < 1e-4);
  CHECK(std::abs((back.eye[2] - own.eye[2]) - (d0.z - o.z)) < 1e-4);
  CHECK(std::abs(back.eye[1] - own.eye[1]) < 1e-6);
  {  // cam_side (Quake's chase_right): looking along +x, his left is +y, whatever the pitch
    cl::DemoEyes l = e;
    l.side = 30;
    const ge::render::FrameView left =
        cl::demo_eyes_view(s, s, l, classic(), nullptr, {}, kFov, 0, 0);
    const cam::Vec3 d1 = cl::engine_point({-100, 30, 0});
    CHECK(std::abs((left.eye[0] - own.eye[0]) - (d1.x - o.x)) < 1e-4);
    CHECK(std::abs((left.eye[2] - own.eye[2]) - (d1.z - o.z)) < 1e-4);
  }
  {  // cam_up (Quake's chase_up): the camera that much above his eye, then back
    cl::DemoEyes h = e;
    h.up = 16;
    const ge::render::FrameView high =
        cl::demo_eyes_view(s, s, h, classic(), nullptr, {}, kFov, 0, 0);
    const cam::Vec3 d2 = cl::engine_point({-100, 0, 16});
    CHECK(std::abs((high.eye[0] - own.eye[0]) - (d2.x - o.x)) < 1e-4);
    CHECK(std::abs((high.eye[1] - own.eye[1]) - (d2.y - o.y)) < 1e-4);
  }
  s.angles.x = 45;  // looking down: the camera rises behind him
  back = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  CHECK(back.eye[1] > own.eye[1] + 0.5);
  // Off, or no one ridden (the intermission): his own eyes.
  e.behind = 0;
  back = cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  CHECK(back.eye[0] == own.eye[0] && back.eye[1] == own.eye[1]);
  // Only while watching.
  cl::DemoPlayback p;
  CHECK(cl::third_person_units(p, true) == 0);
  p.third_person = 1;
  CHECK(cl::third_person_units(p, true) == 100 && cl::third_person_units(p, false) == 0);
}

TEST(cam_lockpos_stays_put_and_turns_to_the_player_and_the_keys_zoom) {
  cl::DemoState s = client_state({0, 0, 24}, 0, 1);
  s.format = cl::DemoFormat::q2;
  cl::DemoEyes e;
  e.behind = 100;
  e.lock_pos = true;
  const ge::render::FrameView first =
      cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0, 0);
  s.eye = {200, 0, 24};  // he runs on along +x
  const ge::render::FrameView then =
      cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0.1, 0.1);
  CHECK(then.eye[0] == first.eye[0] && then.eye[1] == first.eye[1] && then.eye[2] == first.eye[2]);
  const cam::Vec3 f = cam::look_forward(then.heading, then.pitch);
  const cam::Vec3 him = cl::engine_point(s.eye);
  const double dx = him.x - then.eye[0], dz = him.z - then.eye[2];
  const double len = std::sqrt(dx * dx + dz * dz);
  const double flat = std::sqrt(f.x * f.x + f.z * f.z);  // the way it faces, seen from above
  CHECK_MSG(std::abs(f.x / flat - dx / len) < 1e-3 && std::abs(f.z / flat - dz / len) < 1e-3,
            std::to_string(f.x) + " " + std::to_string(f.z));
  e.lock_pos = false;  // let go: behind him again
  const ge::render::FrameView free =
      cl::demo_eyes_view(s, s, e, classic(), nullptr, {}, kFov, 0.2, 0.2);
  CHECK(!e.locked && std::abs(free.eye[0] - then.eye[0]) + std::abs(free.eye[2] - then.eye[2]) > 1);
  // The keys: in while +forward is held, gathering speed to cam_zoomspeed; never below 0.
  ge::console::Console con{false};
  cl::DemoPlayback p;
  cl::Timedemo bench;
  cl::add_playback_commands(con, p, bench);
  cl::zoom_third_person(p, 1, 0.1);
  CHECK(p.behind == 100);  // cam_thirdperson off: nothing
  p.third_person = 1;
  cl::zoom_third_person(p, 1, 0.1);  // 200 units/s after 0.1 s: 20 in
  CHECK_MSG(std::abs(p.behind - 80) < 1e-3, std::to_string(p.behind));
  cl::zoom_third_person(p, 1, 0.1);  // 300 (the top): 30 more
  CHECK_MSG(std::abs(p.behind - 50) < 1e-3, std::to_string(p.behind));
  cl::zoom_third_person(p, 0, 0.1);  // let go: the cvar says where it stopped
  CHECK(p.zoom_speed == 0 && con.cvars().find("cam_dist")->value == "50");
  for (int i = 0; i < 10; i++) cl::zoom_third_person(p, 1, 0.1);
  CHECK(p.behind == 0);
  cl::zoom_third_person(p, -1, 0.1);
  CHECK(p.behind > 0);
  CHECK(cl::zoom_of({.forward = true}) == 1 && cl::zoom_of({.back = true}) == -1 &&
        cl::zoom_of({.forward = true, .back = true}) == 0);
}

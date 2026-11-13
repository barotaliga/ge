// The classic view rebuilt on a recording's eye (games/shooters_edge/client/demo_eyes.hpp): a
// written multiview walk strafing to the left rolls the view left side down and bobs the eye by
// the velocity estimated from the frames read; a client recording's sent velocity is the one
// used; a blow tips the view toward where it came from and fades; dead the view lies on its side;
// a game joined moves by its prediction; the sequel's recordings and the intermission are drawn
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
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/timedemo.hpp"

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

TEST(the_sequel_and_the_intermission_are_drawn_as_read) {
  cl::DemoEyes e;
  cl::DemoState s = client_state({0, 0, 24}, 0, 1);
  s.format = cl::DemoFormat::sequel;
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

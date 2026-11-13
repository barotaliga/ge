// The first-person rig (engine/camera/first_person.hpp): the look is the player's untouched, the
// eyes stand at the view height and crouch, a stair is a ramp to the eye, the bob follows the
// classic stride and clamps, a strafe rolls the horizon toward the way it moves, a hit kicks the
// view away from the blow and falls off over the kick time, a shot punches the pitch up and it
// springs back, a landing dips the eyes, and comfort 0 removes every one of them. The classic row
// holds its bob in the air, kicks toward the blow and never dips.
//
// ge:summary Tests: the first-person rig's look, height, steps, bob, roll, kick, punch, dip
// ge:keywords test camera first-person bob roll kick punch dip stairs crouch comfort
// ge:skill camera testing
// ge:test tests/camera_first_person_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "camera/first_person.hpp"
#include "camera/scenarios.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

namespace {

struct Frame {
  cam::Subject s;
  cam::Pose p;
};

std::vector<Frame> run(const char* id, double hz, double motion = 1,
                       const cam::FirstPersonRow& row = {}) {
  const cam::Scenario* sc = cam::find_camera_scenario(id);
  cam::ScriptRun r = cam::start_script(*sc);
  cam::FirstPersonState st;
  std::vector<Frame> out;
  const int n = static_cast<int>(sc->seconds * hz);
  for (int i = 1; i <= n; ++i) {
    const cam::Subject s = cam::advance_to(r, i / hz);
    out.push_back({s, cam::frame_first_person(row, s, st, 1.0 / hz, motion)});
  }
  return out;
}

double eye_over(const Frame& f) { return f.p.eye.y - f.s.pos.y; }

}  // namespace

TEST(the_look_is_the_players_own) {
  for (const Frame& f : run("walk", 60, 1)) {
    CHECK(f.p.yaw == f.s.look_yaw);
  }
}

TEST(the_bob_follows_the_stride_and_clamps) {
  const cam::FirstPersonRow row;
  CHECK(cam::first_person_bob(row, 0.1, 0) == 0);
  double lo = 1, hi = -1;
  for (int i = 0; i < 600; ++i) {
    const double b = cam::first_person_bob(row, i * 0.001, 4);
    lo = b < lo ? b : lo;
    hi = b > hi ? b : hi;
  }
  // At 4 m/s: 0.08 m of bob, a third held as a lift: from -0.032 to 0.08.
  CHECK(std::fabs(hi - 0.08) < 1e-3 && std::fabs(lo + 0.032) < 1e-3);
  CHECK(cam::first_person_bob(row, 0.15, 100) == row.bob_hi_m);
}

TEST(a_strafe_rolls_the_horizon_and_caps_it) {
  const cam::FirstPersonRow row;
  CHECK(cam::strafe_roll(row, 2.5) > 0);
  CHECK(cam::strafe_roll(row, -2.5) == -cam::strafe_roll(row, 2.5));
  CHECK(cam::strafe_roll(row, 50) == row.roll_angle);
  double most = 0;
  for (const Frame& f : run("strafe", 120))
    most = std::fabs(f.p.roll) > most ? std::fabs(f.p.roll) : most;
  CHECK(std::fabs(most - cam::strafe_roll(row, 4)) < 1e-3);  // a 4 m/s strafe
}

TEST(a_stair_is_a_ramp_to_the_eye) {
  const cam::FirstPersonRow row;
  double most_jump = 0, most_lag = 0;
  const auto frames = run("stairs", 120);
  for (size_t i = 1; i < frames.size(); ++i) {
    const double base_now = frames[i].p.eye.y, base_was = frames[i - 1].p.eye.y;
    const double rise = base_now - base_was;
    most_jump = rise > most_jump ? rise : most_jump;
    const double lag = frames[i].s.pos.y + row.eye_stand_m - frames[i].p.eye.y;
    most_lag = lag > most_lag ? lag : most_lag;
  }
  // The body climbs 0.25 m at once each step; the eye never climbs more than its rate allows
  // (plus the bob's own movement).
  CHECK_MSG(most_jump < 0.05, "the eye jumped " + std::to_string(most_jump) + " m in a frame");
  CHECK(most_lag > 0.05 && most_lag < row.step_max_m + 0.15);
}

TEST(the_eyes_stand_at_the_view_height_and_crouch) {
  const auto frames = run("crouch", 120, 0);
  const cam::FirstPersonRow row;
  CHECK(std::fabs(eye_over(frames[100]) - row.eye_stand_m) < 1e-6);   // 0.83 s
  CHECK(std::fabs(eye_over(frames[360]) - row.eye_crouch_m) < 1e-3);  // 3 s
}

TEST(a_hit_kicks_away_from_the_blow_and_a_shot_punches_up) {
  const auto frames = run("fire", 120);
  // Shots at 1, 1.2 and 1.4 s: the pitch rises over the look and comes back.
  double most = 0;
  for (size_t i = 120; i < 240; ++i) most = frames[i].p.pitch > most ? frames[i].p.pitch : most;
  CHECK(most > 0.01);
  CHECK(std::fabs(frames[290].p.pitch) < 0.01);
  // A blow travelling to the right (from the left) at 2.5 s rolls the view right side down; it is
  // gone by the kick time.
  CHECK(frames[301].p.roll > 0.005);
  CHECK(std::fabs(frames[370].p.roll) < 1e-9);
  // A blow travelling backward (from the front) at 3.5 s pitches the view up.
  CHECK(frames[421].p.pitch - frames[421].s.look_pitch > 0.005);
}

TEST(a_landing_dips_the_eyes_and_they_come_back) {
  const auto frames = run("jump", 120, 1);
  double low = 10;
  size_t land = 0;
  for (size_t i = 1; i < frames.size(); ++i) {
    if (frames[i].s.grounded && !frames[i - 1].s.grounded && land == 0) land = i;
  }
  CHECK(land > 0);
  for (size_t i = land; i < land + 60; ++i)
    low = eye_over(frames[i]) < low ? eye_over(frames[i]) : low;
  const cam::FirstPersonRow row;
  CHECK_MSG(low < row.eye_stand_m - 0.03,
            "the landing dipped the eyes only to " + std::to_string(low));
}

TEST(comfort_zero_removes_the_bob_the_roll_and_the_kicks) {
  const cam::FirstPersonRow row;
  for (const char* id : {"walk", "strafe", "fire"}) {
    for (const Frame& f : run(id, 60, 0)) {
      CHECK(f.p.roll == 0);
      CHECK(f.p.pitch == f.s.look_pitch);
      CHECK(std::fabs(eye_over(f) - row.eye_stand_m) < 1e-9);
    }
  }
}

TEST(the_classic_row_holds_the_bob_in_the_air_and_never_dips) {
  const double u = 0.025;
  const cam::FirstPersonRow row = cam::classic_first_person(u);
  CHECK(std::fabs(row.bob_lo_m + 7 * u) < 1e-12);
  CHECK(std::fabs(row.bob_hi_m - 4 * u) < 1e-12);
  CHECK(std::fabs(row.roll_speed_mps - 200 * u) < 1e-12);
  const auto frames = run("jump", 120, 1, row);
  size_t off = 0, land = 0;
  for (size_t i = 1; i < frames.size(); ++i) {
    if (!frames[i].s.grounded && frames[i - 1].s.grounded && off == 0) off = i;
    if (off > 0 && frames[i].s.grounded && !frames[i - 1].s.grounded && land == 0) land = i;
  }
  CHECK(off > 0 && land > off);
  // In the air the eye stays at the height (and bob) it left the ground with.
  for (size_t i = off + 1; i < land; ++i)
    CHECK_MSG(std::fabs(eye_over(frames[i]) - eye_over(frames[off])) < 1e-9,
              "the eye moved in the air at frame " + std::to_string(i));
  // No dip: on the ground the eye is never under the view height less the bob's floor.
  for (size_t i = land; i < frames.size(); ++i)
    CHECK(eye_over(frames[i]) >= row.eye_stand_m + row.bob_lo_m - 1e-9);
}

TEST(the_classic_row_kicks_toward_the_blow) {
  cam::FirstPersonRow row = cam::classic_first_person(0.025);
  const auto frames = run("fire", 120, 1, row);
  // The blow travelling to the right (from the left) rolls the view left side down, toward it.
  CHECK(frames[301].p.roll < -0.005);
  // The blow travelling backward (from the front) pitches the view down, toward it.
  CHECK(frames[421].p.pitch - frames[421].s.look_pitch < -0.005);
}

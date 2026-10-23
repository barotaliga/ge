// The mounted rig (engine/camera/mounted.hpp): the eye rides the body's frame, the horizon takes
// its share of the body's roll, braking throws the head forward within the neck's travel, a hit
// throws it and it comes back, and comfort 0 bolts it rigidly.
//
// ge:summary Tests: the mounted rig's body frame, roll share, neck, hit and comfort
// ge:keywords test camera mounted helmet neck roll-share braking hit comfort
// ge:skill camera testing
// ge:test tests/camera_mounted_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "camera/mounted.hpp"
#include "camera/scenarios.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

namespace {
struct Frame {
  cam::Subject s;
  cam::Pose p;
};
std::vector<Frame> run(const char* id, double hz, double motion) {
  const cam::Scenario* sc = cam::find_camera_scenario(id);
  cam::ScriptRun r = cam::start_script(*sc);
  cam::MountedState st;
  const cam::MountedRow row;
  std::vector<Frame> out;
  for (int i = 1; i <= static_cast<int>(sc->seconds * hz); ++i) {
    const cam::Subject s = cam::advance_to(r, i / hz);
    out.push_back({s, cam::frame_mounted(row, s, st, 1.0 / hz, motion)});
  }
  return out;
}
// The eye's offset from the body, in the body's frame.
cam::Vec3 offset(const Frame& f) {
  return ge::core::unrotate(f.s.q,
                            {f.p.eye.x - f.s.pos.x, f.p.eye.y - f.s.pos.y, f.p.eye.z - f.s.pos.z});
}
}  // namespace

TEST(bolted_at_comfort_zero_the_eye_rides_the_body_frame) {
  const cam::MountedRow row;
  for (const Frame& f : run("carve", 60, 0)) {
    const cam::Vec3 o = offset(f);
    CHECK(std::fabs(o.x - row.eye_body.x) < 1e-9 && std::fabs(o.y - row.eye_body.y) < 1e-9 &&
          std::fabs(o.z - row.eye_body.z) < 1e-9);
    CHECK(f.p.roll == 0);
  }
}

TEST(the_horizon_takes_its_share_of_the_bank) {
  const cam::MountedRow row;
  const auto frames = run("carve", 60, 1);
  const Frame& f = frames[60];  // 1 s: banked
  const double body = ge::core::to_euler(f.s.q).roll;
  CHECK(std::fabs(body) > 0.1);
  CHECK(std::fabs(f.p.roll - body * row.roll_share) < 0.03);
}

TEST(braking_throws_the_head_forward_within_the_neck) {
  const cam::MountedRow row;
  double most = 0;
  for (const Frame& f : run("skid", 120, 1)) {
    const double fwd = offset(f).z - row.eye_body.z;
    most = fwd > most ? fwd : most;
  }
  CHECK(most > 0.01);
  CHECK(most <= row.neck_max_m + 1e-9);
}

TEST(a_hit_throws_the_head_and_it_comes_back) {
  const cam::MountedRow row;
  const auto frames = run("fire", 120, 1);
  CHECK(std::fabs(offset(frames[310]).x - row.eye_body.x) > 0.005);  // just after 2.5 s
  CHECK(std::fabs(offset(frames[410]).x - row.eye_body.x) < 0.002);
}

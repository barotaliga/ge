// The transitions' arithmetic (engine/camera/transition.hpp): the subject's frame round-trips, the
// quintic decay starts at its offset with its velocity and ends at rest at zero, never crosses
// zero, and a velocity closing fast shortens it; the inertialized pose lands on the new one.
//
// ge:summary Tests: the subject-local pose, the quintic decay, the inertialized pose
// ge:keywords test camera transition local quintic decay inertialize
// ge:skill camera testing
// ge:test tests/camera_transition_test.cpp
#include <cmath>
#include <string>

#include "camera/transition.hpp"
#include "check.hpp"

namespace cam = ge::camera;

TEST(a_pose_round_trips_through_the_subjects_frame) {
  cam::Subject s;
  s.pos = {10, 2, -4};
  s.heading = 0.7;
  cam::Pose p;
  p.eye = {7, 4, -9};
  p.yaw = 1.1;
  p.pitch = -0.2;
  p.roll = 0.05;
  p.fov_y = 1.2;
  const cam::Pose back = cam::from_local(cam::to_local(p, s), s);
  CHECK(std::fabs(back.eye.x - 7) < 1e-12 && std::fabs(back.eye.y - 4) < 1e-12 &&
        std::fabs(back.eye.z + 9) < 1e-12);
  CHECK(std::fabs(back.yaw - 1.1) < 1e-12);
}

TEST(the_decay_starts_with_its_offset_and_velocity_and_ends_at_rest) {
  const cam::Decay d = cam::make_decay(2.0, -1.0, 0.5);
  CHECK(std::fabs(cam::decay_at(d, 0) - 2.0) < 1e-12);
  const double h = 1e-6;
  CHECK(std::fabs((cam::decay_at(d, h) - cam::decay_at(d, 0)) / h + 1.0) < 1e-3);
  CHECK(std::fabs(cam::decay_at(d, d.t_end - h)) < 1e-9);
  CHECK(cam::decay_at(d, d.t_end + 1) == 0);
  for (int i = 0; i <= 100; ++i) CHECK(cam::decay_at(d, d.t_end * i / 100.0) >= -1e-12);
}

TEST(a_negative_offset_decays_from_below_and_a_fast_close_shortens_it) {
  const cam::Decay d = cam::make_decay(-1.0, 0.0, 0.4);
  CHECK(std::fabs(cam::decay_at(d, 0) + 1.0) < 1e-12);
  const cam::Decay fast = cam::make_decay(1.0, -50.0, 0.4);
  CHECK(fast.t_end < 0.4);
}

TEST(the_inertialized_pose_lands_on_the_new_one) {
  cam::Local from, to, still;
  from.off = {0, 3, -6};
  from.yaw = 0.3;
  from.fov = 1.2;
  to.off = {0, 0.7, 0.1};
  to.fov = 1.0;
  const cam::Inertial in = cam::make_inertial(from, still, to, still, 0.4);
  const cam::Local at0 = cam::apply_inertial(in, to, 0);
  CHECK(std::fabs(at0.off.z + 6) < 1e-9 && std::fabs(at0.yaw - 0.3) < 1e-9);
  const cam::Local end = cam::apply_inertial(in, to, 0.4);
  CHECK(end.off.z == to.off.z && end.yaw == to.yaw && end.fov == to.fov);
}

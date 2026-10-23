// The camera's pose (engine/camera/pose.hpp): the look direction's conventions, aiming at a
// point, the blend the short way round and by the lens's tangent, and the fov's conversion.
//
// ge:summary Tests: the camera pose's look, aim, blend and fov conversion
// ge:keywords test camera pose aim blend fov hor-plus
// ge:skill camera testing
// ge:test tests/camera_pose_test.cpp
#include <cmath>

#include "camera/pose.hpp"
#include "check.hpp"
#include "core/math.hpp"

namespace cam = ge::camera;

TEST(yaw_zero_looks_along_plus_z_and_a_quarter_turn_along_plus_x) {
  const cam::Vec3 a = cam::look_forward(0, 0);
  CHECK(std::fabs(a.z - 1) < 1e-12 && std::fabs(a.x) < 1e-12);
  const cam::Vec3 b = cam::look_forward(ge::core::kPi / 2, 0);
  CHECK(std::fabs(b.x - 1) < 1e-12);
  const cam::Vec3 c = cam::look_forward(0, 0.5);
  CHECK(c.y > 0);
}

TEST(aim_pose_looks_at_its_target) {
  const cam::Pose p = cam::aim_pose({0, 2, 0}, {3, 2, 3}, 0.1, 1.0);
  CHECK(std::fabs(p.yaw - ge::core::kPi / 4) < 1e-12);
  CHECK(std::fabs(p.pitch) < 1e-12);
  CHECK(p.roll == 0.1);
}

TEST(a_blend_turns_the_short_way_round) {
  cam::Pose a, b;
  a.yaw = 3.0;
  b.yaw = -3.0;  // 0.28 rad the short way, through pi
  const cam::Pose m = cam::blend_pose(a, b, 0.5);
  CHECK_MSG(std::fabs(ge::core::angle_diff(m.yaw, ge::core::kPi)) < 1e-9,
            "a blend from 3 to -3 rad went the long way round");
  CHECK(cam::blend_pose(a, b, 0).yaw == a.yaw);
}

TEST(a_blend_of_the_lens_moves_its_tangent_evenly) {
  cam::Pose a, b;
  a.fov_y = 0.4;
  b.fov_y = 1.4;
  const cam::Pose m = cam::blend_pose(a, b, 0.5);
  const double want = 0.5 * (std::tan(0.2) + std::tan(0.7));
  CHECK(std::fabs(std::tan(m.fov_y / 2) - want) < 1e-12);
}

TEST(ninety_degrees_across_at_four_by_three_is_about_seventy_four_up) {
  const double y = cam::fov_y_of(ge::core::kPi / 2);
  CHECK(std::fabs(y * 180 / ge::core::kPi - 73.74) < 0.01);
  CHECK(std::fabs(cam::fov_h_of(y) - ge::core::kPi / 2) < 1e-12);
}

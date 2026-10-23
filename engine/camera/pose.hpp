// THE POSE a camera hands the renderer each display frame: where the eye is, which way it looks,
// how the horizon is tipped and how wide the lens is. Every rig answers with one, a transition
// blends two, and `to_view.hpp` writes it into a frame's `FrameView`.
//
// CONVENTIONS (the world's, engine/core/math.hpp): y is up; a yaw is 0 along +z and turns
// clockwise seen from above, toward +x; a pitch is positive looking up; a roll is RIGHT SIDE DOWN
// positive. The field of view is VERTICAL here; a player's setting is horizontal at a 4:3
// reference aspect and is converted once (`fov_y_of`), so a wider screen sees more at the sides
// and the same height (Hor+).
//
// BLENDING: the eye linearly, the angles the short way round, the field of view by its tangent
// (the picture's scale moves evenly from a long lens to a wide one).
//
// ge:summary The camera's pose (eye, yaw, pitch, roll, fov) and blending, aiming, fov conversion
// ge:keywords camera pose eye yaw pitch roll fov field-of-view hor-plus blend_pose aim_pose
// ge:keywords look_forward fov_y_of fov_h_of near-plane
// ge:skill camera
// ge:test tests/camera_pose_test.cpp
#pragma once

#include "core/quat.hpp"

namespace ge::camera {

using core::Vec3;

struct Pose {
  Vec3 eye{0, 0, 0};    // world metres
  double yaw = 0;       // rad, 0 along +z, clockwise from above
  double pitch = 0;     // rad, positive up
  double roll = 0;      // rad, right side down positive
  double fov_y = 1.0;   // vertical field of view, rad
  double near_m = 0.1;  // the near plane, m
};

// The unit direction a yaw and pitch look along.
Vec3 look_forward(double yaw, double pitch);

// A pose at `eye` looking at `target`, with this roll and field of view.
Pose aim_pose(const Vec3& eye, const Vec3& target, double roll, double fov_y, double near_m = 0.1);

// `a` to `b` by `t` (0..1): the eye linearly, the angles the short way round, the fov by its
// tangent, the near plane linearly.
Pose blend_pose(const Pose& a, const Pose& b, double t);

// The vertical field of view a horizontal one at `aspect` (width over height) gives, rad; and
// back. A player's `cl_fov` is horizontal at 4:3 (`kFovAspect`).
inline constexpr double kFovAspect = 4.0 / 3.0;
double fov_y_of(double fov_h, double aspect = kFovAspect);
double fov_h_of(double fov_y, double aspect = kFovAspect);

}  // namespace ge::camera

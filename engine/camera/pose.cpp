// The pose's arithmetic: the look direction, aiming at a point, the blend and the fov conversion.
//
// ge:summary The pose's look direction, aim, blend and field-of-view conversion
// ge:keywords camera pose blend aim fov conversion
// ge:skill camera
// ge:test tests/camera_pose_test.cpp
#include "camera/pose.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

Vec3 look_forward(double yaw, double pitch) {
  const double c = libm::cos(pitch);
  return Vec3{libm::sin(yaw) * c, libm::sin(pitch), libm::cos(yaw) * c};
}

Pose aim_pose(const Vec3& eye, const Vec3& target, double roll, double fov_y, double near_m) {
  const double dx = target.x - eye.x, dy = target.y - eye.y, dz = target.z - eye.z;
  Pose p;
  p.eye = eye;
  p.yaw = libm::atan2(dx, dz);
  p.pitch = libm::atan2(dy, core::hypot(dx, dz));
  p.roll = roll;
  p.fov_y = fov_y;
  p.near_m = near_m;
  return p;
}

Pose blend_pose(const Pose& a, const Pose& b, double t) {
  Pose p;
  p.eye = Vec3{core::lerp(a.eye.x, b.eye.x, t), core::lerp(a.eye.y, b.eye.y, t),
               core::lerp(a.eye.z, b.eye.z, t)};
  p.yaw = a.yaw + core::angle_diff(a.yaw, b.yaw) * t;
  p.pitch = core::lerp(a.pitch, b.pitch, t);
  p.roll = a.roll + core::angle_diff(a.roll, b.roll) * t;
  const double ta = libm::tan(a.fov_y * 0.5), tb = libm::tan(b.fov_y * 0.5);
  p.fov_y = 2.0 * libm::atan(core::lerp(ta, tb, t));
  p.near_m = core::lerp(a.near_m, b.near_m, t);
  return p;
}

double fov_y_of(double fov_h, double aspect) {
  return 2.0 * libm::atan(libm::tan(fov_h * 0.5) / aspect);
}

double fov_h_of(double fov_y, double aspect) {
  return 2.0 * libm::atan(libm::tan(fov_y * 0.5) * aspect);
}

}  // namespace ge::camera

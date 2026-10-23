// The simple lenses' frames.
//
// ge:summary The orbit, planted and free lenses' frames
// ge:keywords camera orbit planted free frame
// ge:skill camera
// ge:test tests/camera_lenses_test.cpp
#include "camera/lenses.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

Pose frame_orbit(const OrbitRow& row, const Subject& s, const World& w, OrbitState& st, double dt) {
  st.angle += row.spin * dt;
  Vec3 eye{s.pos.x + libm::sin(st.angle) * row.radius_m, s.pos.y + row.height_m,
           s.pos.z + libm::cos(st.angle) * row.radius_m};
  const double floor = ground_at(w, eye.x, eye.z) + row.clearance_m;
  if (eye.y < floor) eye.y = floor;
  return aim_pose(eye, Vec3{s.pos.x, s.pos.y + row.aim_height_m, s.pos.z}, 0, row.fov_y);
}

Pose frame_planted(const PlantedRow& row, const Subject& s, PlantedState& st, double dt) {
  const double dx = s.pos.x - row.eye.x, dz = s.pos.z - row.eye.z;
  const double dy = s.pos.y + row.aim_height_m - row.eye.y;
  const double plan = core::hypot(dx, dz);
  const double yaw = libm::atan2(dx, dz), pitch = libm::atan2(dy, plan);
  const double range = core::hypot(plan, dy);
  double want_fov = row.fov_y;
  if (row.hold_m > 0 && range > 0) {
    want_fov = core::clamp(2.0 * libm::atan(row.hold_m * 0.5 / range), row.fov_min, row.fov_max);
  }
  bool snap = st.fresh;
  for (int i = 0; i < s.event_count; ++i) snap = snap || s.events[i].kind == Moment::teleport;
  st.fresh = false;
  Pose p;
  p.eye = row.eye;
  if (snap) {
    p.yaw = settle(st.yaw, yaw);
    p.pitch = settle(st.pitch, pitch);
    st.fov = want_fov;
  } else {
    p.yaw = follow_angle(st.yaw, row.aim, yaw, dt);
    p.pitch = follow(st.pitch, row.aim, pitch, dt);
    st.fov = damp(st.fov, want_fov, row.zoom_halflife_s, dt);
  }
  p.fov_y = st.fov;
  return p;
}

Pose frame_free(const FreeRow& row, const Subject& s) {
  Pose p;
  p.eye = s.pos;
  p.yaw = s.look_yaw;
  p.pitch = s.look_pitch;
  p.fov_y = row.fov_y;
  p.near_m = 0.05;
  return p;
}

}  // namespace ge::camera

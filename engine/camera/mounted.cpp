// The mounted rig's frame: the body's acceleration into the neck, the hit, the grain.
//
// ge:summary The mounted rig's frame: the neck read off the body's velocity, hits, grain
// ge:keywords camera mounted frame neck acceleration grain
// ge:skill camera
// ge:test tests/camera_mounted_test.cpp
#include "camera/mounted.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

namespace {
double bounded(double v, double most) { return most * libm::tanh(v / most); }
}  // namespace

Pose frame_mounted(const MountedRow& row, const Subject& s, MountedState& st, double dt,
                   double motion) {
  bool snap = st.fresh;
  for (int i = 0; i < s.event_count; ++i) snap = snap || s.events[i].kind == Moment::teleport;
  st.fresh = false;
  Vec3 a{0, 0, 0};
  if (!snap && dt > 0) {
    const Vec3 world{(s.vel.x - st.last_vel.x) / dt, (s.vel.y - st.last_vel.y) / dt,
                     (s.vel.z - st.last_vel.z) / dt};
    a = core::unrotate(s.q, world);
  }
  st.last_vel = s.vel;
  if (snap) {
    st.ax = st.ay = st.az = Spring{};
    st.hx = st.hy = st.hz = Spring{};
  }
  const double ax = spring_to(st.ax, a.x, row.neck_halflife_s, dt);
  const double ay = spring_to(st.ay, a.y, row.neck_halflife_s, dt);
  const double az = spring_to(st.az, a.z, row.neck_halflife_s, dt);
  for (int i = 0; i < s.event_count; ++i) {
    const Event& e = s.events[i];
    if (e.kind != Moment::hit) continue;
    const Vec3 d = core::unrotate(s.q, e.dir);
    st.hx.v += d.x * e.size * row.hit_mps;
    st.hy.v += d.y * e.size * row.hit_mps;
    st.hz.v += d.z * e.size * row.hit_mps;
  }
  const double hx = spring_to(st.hx, 0, row.neck_halflife_s, dt);
  const double hy = spring_to(st.hy, 0, row.neck_halflife_s, dt);
  const double hz = spring_to(st.hz, 0, row.neck_halflife_s, dt);

  // The head is thrown against the acceleration, within the neck's travel.
  const double m = row.neck_max_m;
  Vec3 off{row.eye_body.x + (bounded(-ax * row.neck_m_per_mps2, m) + hx) * motion,
           row.eye_body.y + (bounded(-ay * row.neck_m_per_mps2, m) + hy) * motion,
           row.eye_body.z + (bounded(-az * row.neck_m_per_mps2, m) + hz) * motion};
  st.clock += dt;
  const double speed = core::hypot3(s.vel.x, s.vel.y, s.vel.z);
  const double grain = row.grain_m * s.surface * core::num::min(1.0, speed / 10.0) * motion;
  if (grain > 0) {
    off.x += grain * wave(st.grain.axis[0], st.clock, row.grain_hz);
    off.y += grain * wave(st.grain.axis[1], st.clock, row.grain_hz);
  }
  const Vec3 eo = core::rotate(s.q, off);
  const core::Euler e = core::to_euler(s.q);
  const double r = row.neck_max_rad;
  Pose p;
  p.eye = Vec3{s.pos.x + eo.x, s.pos.y + eo.y, s.pos.z + eo.z};
  p.yaw = e.heading;
  // Thrown back under acceleration (the look tips up), forward under braking.
  p.pitch = e.pitch - row.down + bounded(az * row.neck_rad_per_mps2, r) * motion;
  // Leaned out of a corner: an acceleration to the right tips the head's left side down.
  p.roll = e.roll * row.roll_share * motion - bounded(ax * row.neck_rad_per_mps2, r) * motion;
  p.fov_y = row.fov_y + row.fov_per_mps * speed;
  p.near_m = 0.05;
  return p;
}

}  // namespace ge::camera

// The transitions' arithmetic: the subject's frame, the ease, the quintic decay.
//
// ge:summary The transitions' arithmetic: the subject-local pose, smootherstep, the quintic decay
// ge:keywords camera transition local quintic decay inertialize smootherstep
// ge:skill camera
// ge:test tests/camera_transition_test.cpp
#include "camera/transition.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

Local to_local(const Pose& p, const Subject& s) {
  const double sh = libm::sin(s.heading), ch = libm::cos(s.heading);
  const double dx = p.eye.x - s.pos.x, dy = p.eye.y - s.pos.y, dz = p.eye.z - s.pos.z;
  Local l;
  l.off = Vec3{dx * ch - dz * sh, dy, dx * sh + dz * ch};
  l.yaw = core::angle_diff(s.heading, p.yaw);
  l.pitch = p.pitch;
  l.roll = p.roll;
  l.fov = p.fov_y;
  l.near = p.near_m;
  return l;
}

Pose from_local(const Local& l, const Subject& s) {
  const double sh = libm::sin(s.heading), ch = libm::cos(s.heading);
  Pose p;
  p.eye = Vec3{s.pos.x + l.off.x * ch + l.off.z * sh, s.pos.y + l.off.y,
               s.pos.z - l.off.x * sh + l.off.z * ch};
  p.yaw = s.heading + l.yaw;
  p.pitch = l.pitch;
  p.roll = l.roll;
  p.fov_y = l.fov;
  p.near_m = l.near;
  return p;
}

Local local_rate(const Local& a, const Local& b, double dt) {
  Local r;
  if (dt <= 0) return Local{{0, 0, 0}, 0, 0, 0, 0, 0};
  r.off = Vec3{(b.off.x - a.off.x) / dt, (b.off.y - a.off.y) / dt, (b.off.z - a.off.z) / dt};
  r.yaw = core::angle_diff(a.yaw, b.yaw) / dt;
  r.pitch = (b.pitch - a.pitch) / dt;
  r.roll = core::angle_diff(a.roll, b.roll) / dt;
  r.fov = (b.fov - a.fov) / dt;
  r.near = 0;
  return r;
}

double smootherstep(double t) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

Decay make_decay(double x0, double v0, double duration_s) {
  Decay d;
  if (duration_s <= 0 || (x0 == 0 && v0 == 0)) return d;
  // Work on a positive offset (or, with none, a velocity carrying it positive). The velocity is
  // KEPT whichever way it points, so the picture leaves the switch moving as it moved: one moving
  // the offset further away carries it out and back (the `camera-transitions` lab measured the
  // dropped velocity as a kick of up to 10 m/s); one closing it fast shortens the decay so it
  // never crosses zero.
  d.sign = x0 < 0 || (x0 == 0 && v0 < 0) ? -1.0 : 1.0;
  x0 *= d.sign;
  v0 *= d.sign;
  double t = duration_s;
  if (v0 < 0) {
    const double shortest = -5.0 * x0 / v0;
    if (shortest < t) t = shortest;
  }
  double a0 = (-8.0 * v0 * t - 20.0 * x0) / (t * t);
  if (a0 < 0) a0 = 0;  // never a sudden pull toward the new pose at the switch
  const double t2 = t * t, t3 = t2 * t, t4 = t3 * t, t5 = t4 * t;
  d.x0 = x0;
  d.v0 = v0;
  d.a0 = a0;
  d.a = -(a0 * t2 + 6.0 * v0 * t + 12.0 * x0) / (2.0 * t5);
  d.b = (3.0 * a0 * t2 + 16.0 * v0 * t + 30.0 * x0) / (2.0 * t4);
  d.c = -(3.0 * a0 * t2 + 12.0 * v0 * t + 20.0 * x0) / (2.0 * t3);
  d.t_end = t;
  return d;
}

double decay_at(const Decay& d, double t) {
  if (t >= d.t_end || d.t_end <= 0) return 0;
  if (t < 0) t = 0;
  const double v = (((d.a * t + d.b) * t + d.c) * t + 0.5 * d.a0) * t * t + d.v0 * t + d.x0;
  return d.sign * v;
}

Inertial make_inertial(const Local& from, const Local& from_rate, const Local& to,
                       const Local& to_rate, double duration_s) {
  Inertial in;
  // The eye's offset decays on each axis of the subject's frame, so all of its velocity, not
  // only the part along the offset, leaves the switch with it.
  in.x = make_decay(from.off.x - to.off.x, from_rate.off.x - to_rate.off.x, duration_s);
  in.y = make_decay(from.off.y - to.off.y, from_rate.off.y - to_rate.off.y, duration_s);
  in.z = make_decay(from.off.z - to.off.z, from_rate.off.z - to_rate.off.z, duration_s);
  in.yaw = make_decay(core::angle_diff(to.yaw, from.yaw), from_rate.yaw - to_rate.yaw, duration_s);
  in.pitch = make_decay(from.pitch - to.pitch, from_rate.pitch - to_rate.pitch, duration_s);
  in.roll =
      make_decay(core::angle_diff(to.roll, from.roll), from_rate.roll - to_rate.roll, duration_s);
  in.fov = make_decay(from.fov - to.fov, from_rate.fov - to_rate.fov, duration_s);
  return in;
}

Local apply_inertial(const Inertial& in, const Local& to, double t) {
  Local l = to;
  l.off = Vec3{to.off.x + decay_at(in.x, t), to.off.y + decay_at(in.y, t),
               to.off.z + decay_at(in.z, t)};
  l.yaw = to.yaw + decay_at(in.yaw, t);
  l.pitch = to.pitch + decay_at(in.pitch, t);
  l.roll = to.roll + decay_at(in.roll, t);
  l.fov = to.fov + decay_at(in.fov, t);
  return l;
}

}  // namespace ge::camera

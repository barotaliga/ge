// The first-person rig's frame: the eye height and the step, the bob, the roll, the kicks, the
// dip, the zoom and the sway.
//
// ge:summary The first-person rig's frame: eye height, steps, bob, roll, kicks, dip, zoom
// ge:keywords camera first-person frame bob roll kick dip zoom step
// ge:skill camera
// ge:test tests/camera_first_person_test.cpp
#include "camera/first_person.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

double first_person_bob(const FirstPersonRow& row, double t_s, double ground_speed) {
  if (row.bob_cycle_s <= 0 || row.bob == 0) return 0;
  double c = t_s - std::floor(t_s / row.bob_cycle_s) * row.bob_cycle_s;
  c /= row.bob_cycle_s;
  const double up = core::clamp(row.bob_up, 0.01, 0.99);
  c = c < up ? core::kPi * c / up : core::kPi + core::kPi * (c - up) / (1.0 - up);
  const double b = ground_speed * row.bob;
  return core::clamp(b * 0.3 + b * 0.7 * libm::sin(c), row.bob_lo_m, row.bob_hi_m);
}

double strafe_roll(const FirstPersonRow& row, double side_speed) {
  const double side = std::fabs(side_speed);
  const double a =
      side < row.roll_speed_mps ? side * row.roll_angle / row.roll_speed_mps : row.roll_angle;
  return side_speed < 0 ? -a : a;
}

Pose frame_first_person(const FirstPersonRow& row, const Subject& s, FirstPersonState& st,
                        double dt, double motion) {
  const double crouched = core::lerp(row.eye_stand_m, row.eye_crouch_m, s.crouch);
  bool snap = st.fresh;
  for (int i = 0; i < s.event_count; ++i) snap = snap || s.events[i].kind == Moment::teleport;
  if (snap) {
    st.eye_base = s.pos.y;
    st.eye_height = crouched;
    st.was_grounded = s.grounded;
    st.kick_left = 0;
    settle(st.punch, 0);
    settle(st.dip, 0);
    st.zoom = s.zoom;
    st.fresh = false;
  }
  st.clock += dt;

  // The step: up at a rate within a lag, down at once; in the air, with the body.
  if (s.grounded && st.was_grounded && s.pos.y > st.eye_base) {
    st.eye_base = core::approach(st.eye_base, s.pos.y, row.step_rise_mps * dt);
    if (s.pos.y - st.eye_base > row.step_max_m) st.eye_base = s.pos.y - row.step_max_m;
  } else {
    st.eye_base = s.pos.y;
  }
  st.eye_height = damp(st.eye_height, crouched, row.crouch_halflife_s, dt);

  // The body's axes in plan, from the look's yaw.
  const double sy = libm::sin(s.look_yaw), cy = libm::cos(s.look_yaw);
  const double side_speed = s.vel.x * cy - s.vel.z * sy;  // along the right (cos, 0, -sin)
  const double ground_speed = core::hypot(s.vel.x, s.vel.z);

  // The moments.
  for (int i = 0; i < s.event_count; ++i) {
    const Event& e = s.events[i];
    if (e.kind == Moment::landing && !snap) {
      // A spring kicked from rest by v peaks near v / (w e) critically damped, a little more
      // under-damped: kicked by 2.2 w a metre, it dips about `dip_per_mps` a m/s of fall.
      kick(st.dip, -row.dip_per_mps * e.size * 2.2 * core::kTau * row.dip_poles.f_hz);
    }
    if (e.kind == Moment::shot) kick(st.punch, row.punch * e.size);
    if (e.kind == Moment::hit) {
      // Kicked away from the blow: a blow travelling to the right rolls the view right side
      // down, one travelling backward (from the front) pitches it up.
      const double right = e.dir.x * cy - e.dir.z * sy;
      const double fwd = e.dir.x * sy + e.dir.z * cy;
      st.kick_roll = e.size * right * row.kick_roll;
      st.kick_pitch = -e.size * fwd * row.kick_pitch;
      st.kick_left = row.kick_time_s;
    }
  }
  st.was_grounded = s.grounded;
  const double dip = follow(st.dip, row.dip_poles, 0.0, dt);
  const double punch = follow(st.punch, row.punch_poles, 0.0, dt);
  double kick_share = 0;
  if (st.kick_left > 0 && row.kick_time_s > 0) {
    kick_share = st.kick_left / row.kick_time_s;
    st.kick_left -= dt;
  }
  st.zoom = damp(st.zoom, s.zoom, row.zoom_halflife_s, dt);

  const double bob = s.grounded ? first_person_bob(row, st.clock, ground_speed) : 0.0;
  const double dip_clipped = row.dip_max_m * libm::tanh(dip / row.dip_max_m);
  double idle_yaw = 0, idle_pitch = 0, idle_roll = 0;
  if (row.idle > 0) {
    idle_yaw = row.idle * libm::sin(st.clock * core::kTau / 2.0);
    idle_pitch = row.idle * libm::sin(st.clock * core::kTau / 1.0);
    idle_roll = row.idle * 0.3 * libm::sin(st.clock * core::kTau / 0.5);
  }

  Pose p;
  p.eye = Vec3{s.pos.x, st.eye_base + st.eye_height + (bob + dip_clipped) * motion, s.pos.z};
  p.yaw = s.look_yaw + idle_yaw * motion;
  p.pitch = s.look_pitch + (st.kick_pitch * kick_share + punch + idle_pitch) * motion;
  p.roll = (strafe_roll(row, side_speed) + st.kick_roll * kick_share + idle_roll) * motion;
  p.fov_y = core::lerp(row.fov_y, row.zoom_fov_y, st.zoom);
  p.near_m = 0.05;
  return p;
}

}  // namespace ge::camera

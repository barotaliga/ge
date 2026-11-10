// The held weapon's frame: the followers chased, the pieces summed into a turn and a shift, and
// the turn written as the placement's matrix.
//
// ge:summary The held weapon's frame: bob, sway, kick, dip, put-away and breath as a placement
// ge:keywords viewmodel frame_viewmodel classic_row modern_row place_point place_vector
// ge:keywords step_breath breath_shape exertion wander
// ge:skill render camera
// ge:test tests/viewmodel_place_test.cpp
#include "viewmodel/place.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/quat.hpp"
#include "viewmodel/light.hpp"

namespace ge::viewmodel {

namespace libm = core::libm;

PlaceRow classic_row() {
  PlaceRow r;
  r.breath_m = 0.0025;
  r.breath_pitch = 0.004;
  r.breath_wander_m = 0.0008;
  return r;
}

PlaceRow modern_row() {
  PlaceRow r;
  r.bob_forward = 0.25;
  r.bob_up = 0.35;
  r.bob_side = 0.006;
  r.sway_poles = camera::Poles{3.5, 0.55, 0.0};
  r.sway_max = 0.18;
  r.sway_shift = 0.08;
  r.breath_m = 0.004;
  r.breath_pitch = 0.007;
  r.breath_wander_m = 0.0015;
  return r;
}

namespace {

constexpr double kBreathIn = 0.4;     // the share of a breath spent breathing in
constexpr double kBreathEaseS = 0.8;  // s the breath's depth takes to ease 63% to a new one

// The breath's shape over one breath, 0 (emptied) to 1 (full): up over the first `kBreathIn`,
// down over the rest, smooth at both turns.
double breath_shape(double phase) {
  const double s = phase < kBreathIn ? 0.5 * phase / kBreathIn
                                     : 0.5 + 0.5 * (phase - kBreathIn) / (1.0 - kBreathIn);
  return 0.5 - 0.5 * libm::cos(core::kTau * s);
}

// One frame of the breath: the exertion followed, the phase advanced (a new breath drawing its
// depth and length), the wander's leg walked. Writes the lift (m), the pitch (rad) and the drift
// across and up (m), all before the comfort scale.
void step_breath(const PlaceRow& row, const PlaceInput& in, PlaceState& st, double dt, double& lift,
                 double& pitch, double& across, double& up) {
  // Exertion: towards the speed's share of all-out, quick to climb, slow to fall.
  const double work =
      row.exert_run_mps > 0 ? core::clamp(in.ground_speed / row.exert_run_mps, 0.0, 1.0) : 0.0;
  const double tau = work > st.exertion ? row.exert_rise_s : row.exert_fall_s;
  if (tau > 0) st.exertion += (work - st.exertion) * (1.0 - libm::exp(-dt / tau));
  if (in.landing > 0) st.exertion += row.exert_landing * in.landing;
  st.exertion = core::clamp(st.exertion, 0.0, 1.0);
  const double e = st.exertion;
  if (row.breath_m <= 0 && row.breath_wander_m <= 0) return;

  // The phase: faster with exertion, each breath its own length.
  const double hz = row.breath_hz * (1.0 + row.exert_rate * e) / st.breath_len;
  st.breath_phase += hz * dt;
  while (st.breath_phase >= 1.0) {
    st.breath_phase -= 1.0;
    st.breath_depth = core::range(st.breath_rng, 0.8, 1.2);
    st.breath_len = core::range(st.breath_rng, 0.85, 1.2);
    if (core::chance(st.breath_rng, 0.08 * (1.0 - e))) {  // a sigh, only when settled
      st.breath_depth *= 1.7;
      st.breath_len *= 1.5;
    }
  }
  const double moving = core::smoothstep(0.0, 2.0, in.ground_speed);
  const double keep = core::lerp(1.0, row.breath_moving, moving);
  // The depth eases to its new value (a new breath's, a stop's) so the gun never jumps.
  const double depth_to = st.breath_depth * (1.0 + row.exert_depth * e) * keep;
  if (st.breath_amp < 0) st.breath_amp = depth_to;
  st.breath_amp += (depth_to - st.breath_amp) * (1.0 - libm::exp(-dt / kBreathEaseS));
  const double depth = st.breath_amp;
  const double b = breath_shape(st.breath_phase) - 0.5;
  lift = row.breath_m * depth * b;
  pitch = row.breath_pitch * depth * b;

  // The wander: a smooth walk between random points, quicker and wider when out of breath.
  if (row.breath_wander_m > 0) {
    st.wander_t += dt * (1.0 + e);
    if (st.wander_t >= st.wander_s) {
      st.wander_t = 0;
      st.wander_s = core::range(st.breath_rng, 1.5, 3.0);
      for (int k = 0; k < 2; ++k) {
        st.wander_from[k] = st.wander_to[k];
        st.wander_to[k] = core::range(st.breath_rng, -1.0, 1.0);
      }
    }
    const double t = core::smoothstep(0.0, 1.0, st.wander_t / st.wander_s);
    const double w = row.breath_wander_m * (1.0 + e) * keep;
    across = w * core::lerp(st.wander_from[0], st.wander_to[0], t);
    up = 0.6 * w * core::lerp(st.wander_from[1], st.wander_to[1], t);
  }
}

}  // namespace

Placement frame_viewmodel(const PlaceRow& row, const PlaceInput& in, PlaceState& st, double dt,
                          double motion) {
  motion = core::clamp(motion, 0.0, 1.0);
  if (st.fresh) {
    camera::settle(st.yaw, in.look_yaw);
    camera::settle(st.pitch, in.look_pitch);
    camera::settle(st.back, 0);
    camera::settle(st.rise, 0);
    camera::settle(st.dip, 0);
    st.lowered = core::clamp(in.lowered, 0.0, 1.0);
    st.fresh = false;
  }
  st.clock += dt;

  // The moments: a shot kicks the grip back and the muzzle up; a landing kicks it down.
  if (in.shot > 0) {
    camera::kick(st.back, -row.kick_back * in.shot);
    camera::kick(st.rise, row.kick_pitch * in.shot);
  }
  if (in.landing > 0) camera::kick(st.dip, -row.dip_per_mps * in.landing * core::kTau);
  const double back = camera::follow(st.back, row.kick_poles, 0, dt);
  const double rise = camera::follow(st.rise, row.kick_poles, 0, dt);
  const double dip = camera::follow(st.dip, row.dip_poles, 0, dt);

  // The sway: the aim chases the look; the lag is what is drawn.
  const double gy = camera::follow_angle(st.yaw, row.sway_poles, in.look_yaw, dt);
  const double gp = camera::follow_angle(st.pitch, row.sway_poles, in.look_pitch, dt);
  const double lag_yaw =
      core::clamp(core::angle_diff(in.look_yaw, gy), -row.sway_max, row.sway_max);
  const double lag_pitch =
      core::clamp(core::angle_diff(in.look_pitch, gp), -row.sway_max, row.sway_max);
  if (std::fabs(core::angle_diff(in.look_yaw, gy)) > row.sway_max)
    camera::settle(st.yaw, in.look_yaw + lag_yaw);
  if (std::fabs(core::angle_diff(in.look_pitch, gp)) > row.sway_max)
    camera::settle(st.pitch, in.look_pitch + lag_pitch);

  // Put away and back, at a steady rate.
  const double rate = row.switch_s > 0 ? dt / row.switch_s : 1.0;
  st.lowered = core::approach(st.lowered, core::clamp(in.lowered, 0.0, 1.0), rate);
  const double away = core::smoothstep(0.0, 1.0, st.lowered);

  // The stride's swing across (a figure of eight over two strides) and the breath at rest.
  double side = 0, lift = 0;
  if (in.grounded && in.stride_cycle_s > 0 && row.bob_side != 0) {
    const double a = core::kPi * in.stride_s / in.stride_cycle_s;
    const double w = row.bob_side * in.ground_speed;
    side = w * libm::sin(a);
    lift = -0.5 * w * std::fabs(libm::sin(a));
  }
  double breath = 0, breath_pitch = 0, wander_x = 0, wander_y = 0;
  step_breath(row, in, st, dt, breath, breath_pitch, wander_x, wander_y);
  side += wander_x;
  lift += breath + wander_y;

  Placement p;
  p.fov_y = row.fov_y;
  p.shift_m[0] = motion * (side - lag_yaw * row.sway_shift);
  p.shift_m[1] = motion * (lift + row.bob_up * in.bob_m + dip + lag_pitch * row.sway_shift) -
                 away * row.lower_m;
  p.shift_m[2] = motion * (row.bob_forward * in.bob_m + back);
  // A gun left behind by a turn to the right points left of the look: its yaw is the lag.
  p.turn[0] = motion * lag_yaw;
  p.turn[1] = motion * (lag_pitch + rise + breath_pitch) - away * row.lower_pitch;
  p.turn[2] = 0;
  p.exertion = st.exertion;
  p.breath = motion * breath;

  const core::Quat q = core::from_euler(p.turn[0], p.turn[1], p.turn[2]);
  const core::Vec3 ax = core::rotate(q, core::Vec3{1, 0, 0});
  const core::Vec3 ay = core::rotate(q, core::Vec3{0, 1, 0});
  const core::Vec3 az = core::rotate(q, core::Vec3{0, 0, 1});
  const double cols[3][3] = {{ax.x, ax.y, ax.z}, {ay.x, ay.y, ay.z}, {az.x, az.y, az.z}};
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) p.m[r * 4 + c] = cols[c][r];
    p.m[r * 4 + 3] = row.offset_m[r] + p.shift_m[r];
  }
  return p;
}

void place_point(const Placement& p, const float in[3], float out[3]) {
  for (size_t r = 0; r < 3; ++r)
    out[r] = static_cast<float>(p.m[r * 4] * in[0] + p.m[r * 4 + 1] * in[1] +
                                p.m[r * 4 + 2] * in[2] + p.m[r * 4 + 3]);
}

void place_vector(const Placement& p, const float in[3], float out[3]) {
  for (size_t r = 0; r < 3; ++r)
    out[r] =
        static_cast<float>(p.m[r * 4] * in[0] + p.m[r * 4 + 1] * in[1] + p.m[r * 4 + 2] * in[2]);
}

void held_to_world(const Placement& p, double gun_fov_y, const camera::Pose& view,
                   const float held[3], double out[3]) {
  float v[3];
  place_point(p, held, v);
  if (gun_fov_y > 0 && view.fov_y > 0) {
    const double k = core::libm::tan(view.fov_y * 0.5) / core::libm::tan(gun_fov_y * 0.5);
    v[0] = static_cast<float>(v[0] * k);
    v[1] = static_cast<float>(v[1] * k);
  }
  view_to_world(view, v, out);
}

void held_dir_to_world(const Placement& p, double gun_fov_y, const camera::Pose& view,
                       const float held_at[3], const float held_dir[3], double out[3]) {
  constexpr float kStep = 0.1f;  // m along the direction: well inside the gun's own depth range
  const float ahead[3] = {held_at[0] + held_dir[0] * kStep, held_at[1] + held_dir[1] * kStep,
                          held_at[2] + held_dir[2] * kStep};
  double a[3], b[3];
  held_to_world(p, gun_fov_y, view, held_at, a);
  held_to_world(p, gun_fov_y, view, ahead, b);
  double len = 0;
  for (int k = 0; k < 3; ++k) {
    out[k] = b[k] - a[k];
    len += out[k] * out[k];
  }
  len = std::sqrt(len);
  for (int k = 0; k < 3; ++k) out[k] = len > 0 ? out[k] / len : 0;
}

void muzzle_to_world(const HeldModel& model, int a, int b, double t, const Placement& p,
                     double gun_fov_y, const camera::Pose& view, double at[3], double dir[3],
                     double up[3], int barrel) {
  float tip[3];
  muzzle_at(model, a, b, t, tip, barrel);
  held_to_world(p, gun_fov_y, view, tip, at);
  held_dir_to_world(p, gun_fov_y, view, tip, model.muzzle_dir, dir);
  constexpr float kUp[3] = {0, 1, 0};
  double u[3];
  held_dir_to_world(p, gun_fov_y, view, tip, kUp, u);
  const double along = u[0] * dir[0] + u[1] * dir[1] + u[2] * dir[2];
  double len = 0;
  for (int k = 0; k < 3; ++k) {
    up[k] = u[k] - along * dir[k];
    len += up[k] * up[k];
  }
  len = std::sqrt(len);
  for (int k = 0; k < 3; ++k) up[k] = len > 0 ? up[k] / len : (k == 1 ? 1 : 0);
}

}  // namespace ge::viewmodel

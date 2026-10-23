// The trauma shake's decay and its waves.
//
// ge:summary The trauma shake's decay and its smooth per-axis waves
// ge:keywords camera shake trauma wave sines phases
// ge:skill camera
// ge:test tests/camera_shake_test.cpp
#include "camera/shake.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"

namespace ge::camera {

namespace libm = core::libm;

namespace {
// The waves' frequencies as shares of the top one: incommensurate, so the sum never repeats in
// a way the eye picks up.
constexpr double kShare[3] = {1.0, 0.6823, 0.4171};
constexpr double kWeight[3] = {0.5, 0.3, 0.2};
}  // namespace

Waves make_waves(uint32_t seed) {
  core::Rng rng = core::make_rng(seed);
  Waves w;
  for (Wave& a : w.axis) {
    for (double& p : a.phase) p = core::next(rng) * core::kTau;
  }
  return w;
}

double wave(const Wave& w, double t_s, double freq_hz) {
  double v = 0;
  for (int i = 0; i < 3; ++i) {
    v += kWeight[i] * libm::sin(core::kTau * freq_hz * kShare[i] * t_s + w.phase[i]);
  }
  return v;
}

ShakeState make_shake(uint32_t seed) {
  ShakeState s;
  s.waves = make_waves(seed);
  return s;
}

void add_trauma(ShakeState& s, double amount) {
  s.trauma = core::clamp(s.trauma + amount, 0.0, 1.0);
}

ShakeOut shake_at(const ShakeRow& row, ShakeState& s, double dt, double scale) {
  s.t += dt;
  s.trauma = core::clamp(s.trauma - row.decay_per_s * dt, 0.0, 1.0);
  ShakeOut o;
  if (s.trauma <= 0 || scale <= 0) return o;
  const double k = libm::pow(s.trauma, row.exponent) * scale;
  const double f = row.freq_hz;
  o.yaw = row.yaw * k * wave(s.waves.axis[0], s.t, f);
  o.pitch = row.pitch * k * wave(s.waves.axis[1], s.t, f);
  o.roll = row.roll * k * wave(s.waves.axis[2], s.t, f);
  o.right = row.offset_m * k * wave(s.waves.axis[3], s.t, f);
  o.up = row.offset_m * k * wave(s.waves.axis[4], s.t, f);
  o.forward = row.offset_m * k * wave(s.waves.axis[5], s.t, f);
  return o;
}

}  // namespace ge::camera

// SHAKE BY TRAUMA, and the WAVE every buzz in the camera is made of.
//
// A source (an explosion, a landing on a boom, a hit) ADDS trauma, 0..1 and capped; trauma
// decays linearly (`decay_per_s`); what moves the picture is trauma raised to `exponent` (2: a
// small knock barely shows, a big one is violent) times a smooth wave per axis. The wave is three
// incommensurate sines per axis under `freq_hz` (their phases dealt once from a seed), never a
// fresh random offset a frame: white noise at a blow's amplitude is not a rougher shot but a
// broken one, and at 30 Hz it aliases into a slow lurch. Angles move the picture far more than
// offsets, so a row's angles carry most of a shake.
//
// `wave` is the same oscillator for any steady buzz (a boom's tremor, a mounted lens's grain).
//
// ge:summary Trauma shake (linear decay, trauma squared, smooth waves) and the camera's wave
// ge:keywords camera shake screen-shake trauma decay exponent wave oscillator noise tremor buzz
// ge:keywords rumble explosion add_trauma shake_at make_shake
// ge:skill camera
// ge:test tests/camera_shake_test.cpp
#pragma once

#include <cstdint>

namespace ge::camera {

struct ShakeRow {
  double decay_per_s = 1.2;  // trauma lost a second
  double exponent = 2.0;     // the shake is trauma^exponent
  double yaw = 0.035;        // rad at full shake
  double pitch = 0.035;
  double roll = 0.05;
  double offset_m = 0.04;  // m at full shake, each axis
  double freq_hz = 7.0;    // the fastest of the waves
};

// Three incommensurate sines on one axis, phases dealt from a seed.
struct Wave {
  double phase[3] = {0, 0, 0};
};
// The six axes a shake moves: yaw, pitch, roll, x, y, z.
struct Waves {
  Wave axis[6];
};
// The waves for `seed`.
Waves make_waves(uint32_t seed);
// One axis at time `t_s` with its top frequency `freq_hz`, in -1..1.
double wave(const Wave& w, double t_s, double freq_hz);

struct ShakeState {
  double trauma = 0;  // 0..1
  double t = 0;       // the shake's own clock, s
  Waves waves{};
};

// What a shake adds this frame: angles (rad) and an offset in the camera's own axes (m: right,
// up, forward).
struct ShakeOut {
  double yaw = 0, pitch = 0, roll = 0;
  double right = 0, up = 0, forward = 0;
};

ShakeState make_shake(uint32_t seed);
// Add trauma (capped at 1).
void add_trauma(ShakeState& s, double amount);
// Advance by `dt` and return the shake, scaled by `scale` (the player's comfort, 0..1).
ShakeOut shake_at(const ShakeRow& row, ShakeState& s, double dt, double scale);

}  // namespace ge::camera

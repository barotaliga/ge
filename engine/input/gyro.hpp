// THE GYRO'S BIAS: a gyro at rest reads a small turn that is not there (its bias, a few hundredths
// of a rad/s, drifting with temperature), which a gyro aim turns into a slow creep. The bias is
// taken while the device is STILL: each gyro's reading is followed by a running mean (time
// constant `mean_s`); a reading within `rate` of that mean on every axis, with the mean itself
// under `most` (a slow steady turn is a hand, not a bias), is still, and once a device has been
// still for `settle_s` the mean IS its bias, refreshed for as long as it stays still. The bias is
// taken off the slot's reading in place, before the actions read it, so every `gyro:` binding
// reads the turn the hand made.
//
// It is a pure function of the readings and the frame's time step: the same session settles the
// same bias on every machine. A slot that empties forgets its bias.
//
// ge:summary The gyro's bias taken while the device is still, and taken off its readings
// ge:keywords input gyro bias drift calibration still stillness settle gyro-aim unbias_gyro
// ge:keywords motion sensor
// ge:skill input
// ge:test tests/input_gyro_test.cpp
#pragma once

#include <array>

#include "input/state.hpp"

namespace ge::input {

// What counts as still, and how long it takes.
struct GyroStill {
  float rate = 0.05f;     // rad/s off the running mean on every axis
  float most = 0.12f;     // rad/s: a mean faster than this is a turn, never a bias
  float settle_s = 0.8f;  // still this long before the bias is taken
  float mean_s = 0.4f;    // the running mean's time constant, s
};

struct GyroBias {
  bool seen = false;
  bool settled = false;  // a bias has been taken
  double still_s = 0;    // how long the device has been still
  std::array<double, 3> mean{}, bias{};
};

struct GyroBiases {
  std::array<GyroBias, kMaxDevices> slot{};  // by the state's slot
};

// Follow every slot's gyro over a frame of `dt_s` and take its bias off its reading in place.
void unbias_gyro(InputState& s, GyroBiases& g, double dt_s, const GyroStill& k = {});

}  // namespace ge::input

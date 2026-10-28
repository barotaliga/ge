// The gyro's bias: the running mean, the still test, the bias taken off.
//
// ge:summary unbias_gyro: the running mean, the still test, the bias taken off each reading
// ge:keywords input gyro bias still settle unbias_gyro
// ge:skill input
// ge:test tests/input_gyro_test.cpp
#include "input/gyro.hpp"

namespace ge::input {

namespace {

double magnitude(double v) { return v < 0 ? -v : v; }

}  // namespace

void unbias_gyro(InputState& s, GyroBiases& g, double dt_s, const GyroStill& k) {
  const double follow = k.mean_s > 0 && dt_s < k.mean_s ? dt_s / k.mean_s : 1.0;
  for (size_t i = 0; i < s.slots.size(); i++) {
    Slot& sl = s.slots[i];
    GyroBias& b = g.slot[i];
    if (!sl.active) {
      b = {};
      continue;
    }
    if (!b.seen) {
      for (size_t a = 0; a < 3; a++) b.mean[a] = sl.gyro[a];
      b.seen = true;
    } else {
      for (size_t a = 0; a < 3; a++) b.mean[a] += (sl.gyro[a] - b.mean[a]) * follow;
    }
    bool still = true;
    for (size_t a = 0; a < 3; a++) {
      still = still && magnitude(sl.gyro[a] - b.mean[a]) < k.rate && magnitude(b.mean[a]) < k.most;
    }
    b.still_s = still ? b.still_s + dt_s : 0;
    if (b.still_s >= k.settle_s) {
      b.bias = b.mean;
      b.settled = true;
    }
    for (size_t a = 0; a < 3; a++) sl.gyro[a] = static_cast<float>(sl.gyro[a] - b.bias[a]);
  }
}

}  // namespace ge::input

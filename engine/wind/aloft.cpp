// The wind aloft (aloft.hpp).
//
// ge:summary The wind aloft's growth, veer and the evening's low-level jet, as arithmetic
// ge:keywords wind aloft wind_aloft veer_aloft dusk_of jet_at low-level-jet
// ge:skill water render
// ge:test tests/wind_aloft_test.cpp
#include "wind/aloft.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

double wind_aloft(double surface, double h, double least) {
  return std::min(45.0, std::max(least, surface * (1 + 1.2 * core::libm::log(1 + h / 600))));
}

double veer_aloft(double h, double latitude) {
  return (latitude < 0 ? -1.0 : 1.0) * 0.35 * core::libm::log(1 + h / 1000);
}

double dusk_of(double elevation) {
  constexpr double kSix = 6 * kPi / 180;
  return std::clamp((kSix - elevation) / (2 * kSix), 0.0, 1.0);
}

double jet_at(double h, double dusk) {
  const double z = (h - 400) / 450;
  return 1 + 0.7 * dusk * core::libm::exp(-z * z);
}

}  // namespace ge::wind

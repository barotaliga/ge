// The shapes' arithmetic: the zones, the curve on core::libm's pow, the ramp's ease.
//
// ge:summary The dead zones', curve's and ramp's arithmetic in doubles, deterministic
// ge:keywords input dead-zone curve ramp arithmetic pow rescale
// ge:skill input
// ge:test tests/input_shape_test.cpp
#include "input/shape.hpp"

#include <array>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::input {

namespace {

double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// |v| from `inner` .. `outer` onto 0..1, through the curve and the anti-dead zone.
double rescale(double m, double inner, double outer, const Curve& c, double anti) {
  if (m <= inner) return 0;
  const double span = outer - inner;
  double r = span > 0 ? clamp01((m - inner) / span) : 1;
  if (c.exponent != 1 && r > 0 && r < 1) r = core::libm::pow(r, static_cast<double>(c.exponent));
  if (anti > 0) r = anti + (1 - anti) * r;
  return r;
}

double sign(double v) { return v < 0 ? -1 : 1; }

}  // namespace

void shape_stick(float& x, float& y, const StickShape& s) {
  double dx = x, dy = y;
  const double m = std::sqrt(dx * dx + dy * dy);
  switch (s.zone) {
    case Zone::none:
      break;
    case Zone::axial:
      dx = sign(dx) * rescale(std::fabs(dx), s.inner, s.outer, s.curve, s.anti);
      dy = sign(dy) * rescale(std::fabs(dy), s.inner, s.outer, s.curve, s.anti);
      break;
    case Zone::radial:
      if (m < s.inner) dx = dy = 0;
      break;
    case Zone::scaled_radial:
    case Zone::sloped: {
      const double r = rescale(m, s.inner, s.outer, s.curve, s.anti);
      if (r == 0) {
        dx = dy = 0;
      } else {
        dx = dx / m * r;
        dy = dy / m * r;
      }
      if (s.zone == Zone::sloped && r > 0) {
        const double ax = std::fabs(dx), ay = std::fabs(dy);
        const double zx = s.inner * ay, zy = s.inner * ax;
        dx = sign(dx) * (ax <= zx ? 0 : (ax - zx) / (1 - zx));
        dy = sign(dy) * (ay <= zy ? 0 : (ay - zy) / (1 - zy));
        // The snap turns the push; its length stays the scaled one, so full is still full.
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len > 0) {
          dx = dx / len * r;
          dy = dy / len * r;
        }
      }
      break;
    }
  }
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len > 1) {
    dx /= len;
    dy /= len;
  }
  x = static_cast<float>(dx);
  y = static_cast<float>(dy);
}

float shape_axis(float v, const AxisShape& s) {
  const double a = std::fabs(static_cast<double>(v));
  return static_cast<float>(sign(v) * rescale(a, s.inner, s.outer, s.curve, s.anti));
}

bool digital(bool was_on, float v, const Threshold& t) {
  const float a = std::fabs(v);
  return was_on ? a > t.off : a >= t.on;
}

float ramp_toward(float value, float target, double dt_s, const Ramp& r) {
  const double rate = target == 0 ? r.release : r.attack;
  if (rate <= 0) return target;
  const double k = rate * dt_s < 1 ? rate * dt_s : 1;
  const double next = value + (static_cast<double>(target) - value) * k;
  if (target == 0 && std::fabs(next) < r.snap) return 0;
  return static_cast<float>(next);
}

namespace {
constexpr std::array<std::string_view, 5> kZoneNames = {"none", "axial", "radial", "scaled_radial",
                                                        "sloped"};
}

std::string_view zone_name(Zone z) { return kZoneNames[static_cast<size_t>(z)]; }

std::optional<Zone> zone_from_name(std::string_view name) {
  for (size_t i = 0; i < kZoneNames.size(); i++) {
    if (kZoneNames[i] == name) return static_cast<Zone>(i);
  }
  return std::nullopt;
}

}  // namespace ge::input

// The view's axes: each angle turned to radians (rounded to float), its sine and cosine in double
// through core's libm, rounded to float, then the axes multiplied out in float (vec.hpp).
//
// ge:summary The view's forward, right and up from its angles, sines in double through libm
// ge:keywords angle-vectors forward right up sin cos degrees view
// ge:skill shooter determinism
// ge:test tests/shooter_vec_test.cpp
#include "shooter/vec.hpp"

#include "core/libm/libm.hpp"

namespace ge::shooter {

namespace {

constexpr double kRadPerDeg = 3.14159265358979323846 * 2 / 360;

struct SinCos {
  float s, c;
};

// The angle in radians is rounded to float before its sine is taken: sin(180 deg) is then
// -8.74e-8 (the sine of pi rounded to float), not 1.22e-16, which is the sideways drift
// tests/golden/shooter_pmove.txt records for a player facing -x.
SinCos sin_cos(float deg) {
  const auto rad = static_cast<float>(static_cast<double>(deg) * kRadPerDeg);
  const auto a = static_cast<double>(rad);
  return {static_cast<float>(core::libm::sin(a)), static_cast<float>(core::libm::cos(a))};
}

}  // namespace

ViewAxes angle_vectors(Vec3f angles_deg) {
  const SinCos p = sin_cos(angles_deg.x);
  const SinCos y = sin_cos(angles_deg.y);
  const SinCos r = sin_cos(angles_deg.z);
  ViewAxes v;
  v.forward = {p.c * y.c, p.c * y.s, -p.s};
  v.right = {-r.s * p.s * y.c + r.c * y.s, -r.s * p.s * y.s - r.c * y.c, -r.s * p.c};
  v.up = {r.c * p.s * y.c + r.s * y.s, r.c * p.s * y.s - r.s * y.c, r.c * p.c};
  return v;
}

}  // namespace ge::shooter

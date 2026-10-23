// The followers' integration: the second-order follower solved exactly over a frame (its three
// cases by the damping ratio), the exact half-life spring and ease.
//
// ge:summary The followers' integration: the exact second-order step and the exact springs
// ge:keywords camera follower integration exact closed-form underdamped overdamped halflife
// ge:skill camera
// ge:test tests/camera_follow_test.cpp
#include "camera/follow.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::camera {

namespace libm = core::libm;

namespace {

// ln 2: a half-life h is a rate ln2 / h.
constexpr double kLn2 = 0.6931471805599453;

// The follower's equation is k2 y'' + k1 y' + y = u with u = x + k3 xd. Over a frame the target
// x is taken as moving in a line from where it was to where it is (slope c) and xd as held, so
// u = u0 + c t and the motion is a particular path y_p = u0 - k1 c + c t (exactly x when r = 2:
// a ramp tracked with no lag) plus e, with e'' + 2 zeta w e' + w^2 e = 0 solved in closed form.
// The path is the one a 240 Hz frame would have drawn whatever the frame's length, and no frame
// can make it ring. `x0` is the target at the frame's start, on the follower's own winding.
double step(Follower& s, const Poles& p, double x0, double x, double xd, double dt) {
  s.x = x;
  if (dt <= 0 || p.f_hz <= 0) return s.y;
  const double w = core::kTau * p.f_hz;
  const double z = p.zeta < 0 ? 0.0 : p.zeta;
  const double k1 = 2.0 * z / w, k3 = p.r * z / w;
  const double c = (x - x0) / dt;
  const double base = x0 + k3 * xd - k1 * c;  // y_p at the frame's start
  const double e0 = s.y - base, v0 = s.v - c;
  double e, v;
  if (z > 1.0 + 1e-6) {
    const double q = w * std::sqrt(z * z - 1.0);
    const double r1 = -z * w + q, r2 = -z * w - q;
    const double c1 = (v0 - r2 * e0) / (r1 - r2), c2 = e0 - c1;
    const double x1 = libm::exp(r1 * dt), x2 = libm::exp(r2 * dt);
    e = c1 * x1 + c2 * x2;
    v = r1 * c1 * x1 + r2 * c2 * x2;
  } else if (z < 1.0 - 1e-6) {
    const double wd = w * std::sqrt(1.0 - z * z);
    const double a = e0, b = (v0 + z * w * e0) / wd;
    const double k = libm::exp(-z * w * dt), cs = libm::cos(wd * dt), sn = libm::sin(wd * dt);
    e = k * (a * cs + b * sn);
    v = k * ((b * wd - z * w * a) * cs - (a * wd + z * w * b) * sn);
  } else {
    const double j = v0 + w * e0, k = libm::exp(-w * dt);
    e = k * (e0 + j * dt);
    v = k * (v0 - w * j * dt);
  }
  s.y = base + c * dt + e;
  s.v = c + v;
  return s.y;
}

}  // namespace

double settle(Follower& s, double at) {
  s.y = at;
  s.v = 0;
  s.x = at;
  return at;
}

double follow(Follower& s, const Poles& p, double target, double dt) {
  return step(s, p, s.x, target, dt <= 0 ? 0.0 : (target - s.x) / dt, dt);
}

double follow_trend(Follower& s, const Poles& p, double target, double target_rate, double dt) {
  return step(s, p, s.x, target, target_rate, dt);
}

double follow_angle(Follower& s, const Poles& p, double target, double dt) {
  const double near = s.y + core::angle_diff(s.y, target);
  const double moved = core::angle_diff(s.x, target);
  const double xd = dt <= 0 ? 0.0 : moved / dt;
  // The remembered target is kept on the follower's own winding, so the next difference is short.
  return step(s, p, near - moved, near, xd, dt);
}

double spring_to(Spring& s, double target, double halflife_s, double dt) {
  if (halflife_s <= 0) {
    s.x = target;
    s.v = 0;
    return s.x;
  }
  if (dt <= 0) return s.x;
  // Critical damping: x(t) = e^(-y t) (j0 + j1 t) + target, y = 2 ln2 / h.
  const double y = 2.0 * kLn2 / halflife_s;
  const double j0 = s.x - target;
  const double j1 = s.v + j0 * y;
  const double e = libm::exp(-y * dt);
  s.x = e * (j0 + j1 * dt) + target;
  s.v = e * (s.v - j1 * y * dt);
  return s.x;
}

double damp(double x, double target, double halflife_s, double dt) {
  if (halflife_s <= 0) return target;
  if (dt <= 0) return x;
  return target + (x - target) * libm::exp(-kLn2 * dt / halflife_s);
}

double dead_band(double err, double band, double soft) {
  const double e = std::fabs(err);
  if (soft <= 0) return e > band ? 1.0 : 0.0;
  return core::smoothstep(band, band + soft, e);
}

}  // namespace ge::camera

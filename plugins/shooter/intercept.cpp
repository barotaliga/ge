// Where a shot meets a moving player (intercept.hpp).
//
// ge:summary Where a shot meets a moving player: intercept by flight time, a target's own arc,
// ge:summary the grenade's lob
// ge:keywords intercept mover_at lob_pitch
// ge:skill shooter bots
// ge:test tests/shooter_intercept_test.cpp
#include "shooter/intercept.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::shooter {

namespace {

constexpr double kDeg = core::kPi / 180;
constexpr int kHalvings = 24;
constexpr double kLowest = -80, kHighest = 70;  // the pitches a throw is looked for in, up positive

// A throw at `up_deg` (up positive): its height at the distance `d` across, and its flight there.
// False when the throw goes nowhere across.
bool arc_at(double up_deg, double d, const Shot& s, double& z, double& t) {
  const double c = core::libm::cos(up_deg * kDeg), n = core::libm::sin(up_deg * kDeg);
  const double vx = s.speed * c - s.up * n, vz = s.speed * n + s.up * c;
  if (vx <= 1) return false;
  t = d / vx;
  z = vz * t - 0.5 * s.gravity * t * t;
  return true;
}

}  // namespace

Vec3f mover_at(const MovingTarget& m, double t) {
  Vec3f p{m.at.x + static_cast<float>(m.velocity.x * t),
          m.at.y + static_cast<float>(m.velocity.y * t), m.at.z};
  if (m.turn != 0 && t > 0) {
    // On the arc: the velocity turned at `turn` for the time, held straight past `kArcMost`.
    const double w = static_cast<double>(m.turn) * kDeg;  // rad/s
    const double most = kArcMost * kDeg / (w < 0 ? -w : w);
    const double ta = t < most ? t : most;
    const double vx = m.velocity.x, vy = m.velocity.y;
    const double c = core::libm::cos(w * ta), s = core::libm::sin(w * ta);
    // The integral of the turned velocity over [0, ta], then on straight at the turned one.
    const double ix = (vx * s - vy * (1 - c)) / w, iy = (vy * s + vx * (1 - c)) / w;
    const double rest = t - ta;
    p.x = m.at.x + static_cast<float>(ix + (vx * c - vy * s) * rest);
    p.y = m.at.y + static_cast<float>(iy + (vx * s + vy * c) * rest);
  }
  if (m.slide != nullptr) {
    const Vec3f s = m.slide(m.slide_ctx, m.at, p);
    p.x = s.x;
    p.y = s.y;
  }
  if (!m.airborne) {
    p.z += static_cast<float>(m.velocity.z * t);
    return p;
  }
  const double z = m.at.z + m.velocity.z * t - 0.5 * m.gravity * t * t;
  p.z = static_cast<float>(z < m.floor_z ? m.floor_z : z);  // landed: on across at the floor
  return p;
}

bool lob_pitch(Vec3f from, Vec3f to, const Shot& s, float& pitch, double& flight_s) {
  const double dx = to.x - from.x, dy = to.y - from.y;
  const double d = std::sqrt(dx * dx + dy * dy), dz = to.z - from.z;
  // The longest throw's pitch: the arc's height at `d` is highest there (scanned, then halved).
  double best = kLowest, best_z = -1e18;
  for (int k = 0; k <= 30; k++) {
    const double a = kLowest + (kHighest - kLowest) * k / 30;
    double z = 0, t = 0;
    if (arc_at(a, d, s, z, t) && z > best_z) best_z = z, best = a;
  }
  double z = 0, t = 0;
  if (best_z < dz) {  // past the longest throw: the longest
    arc_at(best, d, s, z, t);
    pitch = static_cast<float>(-best);
    flight_s = t;
    return false;
  }
  // The low arc: between the lowest pitch and the longest throw's, the height rises with the pitch.
  double lo = kLowest, hi = best;
  for (int i = 0; i < kHalvings; i++) {
    const double mid = 0.5 * (lo + hi);
    if (arc_at(mid, d, s, z, t) && z < dz)
      lo = mid;
    else
      hi = mid;
  }
  arc_at(hi, d, s, z, t);
  pitch = static_cast<float>(-hi);
  flight_s = t;
  return true;
}

Intercept intercept(Vec3f from, const MovingTarget& m, double head_s, const Shot& s,
                    int iterations) {
  Intercept out;
  out.at = mover_at(m, head_s);
  for (int i = 0; i < (iterations < 1 ? 1 : iterations); i++) {
    if (s.lob) {
      out.lobbed = true;
      lob_pitch(from, out.at, s, out.pitch, out.flight_s);
    } else {
      const Vec3f d = out.at - from;
      out.flight_s = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y +
                               static_cast<double>(d.z) * d.z) /
                     (s.speed > 1 ? s.speed : 1);
    }
    out.at = mover_at(m, head_s + out.flight_s);
  }
  if (s.lob) lob_pitch(from, out.at, s, out.pitch, out.flight_s);
  return out;
}

}  // namespace ge::shooter

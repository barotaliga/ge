// A body through the surface: the crater, the ring and the spray (entry.hpp).
//
// ge:summary A body through the surface: its crater, ring and spray sized off its energy
// ge:keywords water entry crater ring spray jet energy entry_of
// ge:skill water
// ge:test tests/water_entry_test.cpp
#include "water/entry.hpp"

#include <algorithm>
#include <cmath>

#include "water/spectrum.hpp"

namespace ge::water {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

Entry entry_of(const Entrant& body, int water_body, double t, double rho) {
  Entry e;
  e.y = body.y;
  const double v = std::fabs(body.vy);
  const double footprint = std::sqrt(std::max(body.footprint, 1e-6) / kPi);
  e.ring.x = body.x;
  e.ring.z = body.z;
  e.ring.t0 = t;
  e.ring.body = water_body;
  if (body.leaving) {
    // Coming out: the water it lifts falls back off it; no crater.
    e.ring.volume = 0.25 * body.volume * std::min(1.0, v);
    e.ring.size = footprint;
    e.drops = static_cast<int>(std::clamp(body.volume * 400 * std::min(1.0, v), 4.0, 60.0));
    e.speed = 0.5 + 0.3 * v;
    return e;
  }
  // The crater: a share of the energy down through the surface lifts a hemisphere's water.
  const double down = 0.5 * body.mass * v * v;
  const double r4 = 4 * kCraterShare * down / (kPi * rho * kGravity);
  e.radius = std::max(std::sqrt(std::sqrt(r4)), footprint);
  const double density = body.mass / std::max(body.volume, 1e-9);
  const double dense = std::clamp(std::sqrt(density / rho), 0.5, 1.5);
  e.ring.crater = e.radius;
  e.ring.depth = 0.6 * e.radius * dense;
  e.ring.size = e.radius;
  e.jet_at = 2 * std::sqrt(e.radius / kGravity);
  // Moving across the surface: the water ahead pushed on, by the angle off the vertical, and the
  // cavity drawn out along the run.
  const double across = std::sqrt(body.vx * body.vx + body.vz * body.vz);
  const double total = std::sqrt(across * across + v * v);
  if (across > 0 && total > 0) {
    const double lean = kLeanMost * across / total;
    e.ring.lean_x = lean * body.vx / across;
    e.ring.lean_z = lean * body.vz / across;
    e.ring.reach = std::min(kReachShare * across * e.jet_at, kReachMost * e.radius);
  }
  const double along = e.radius + 0.5 * e.ring.reach;
  e.ring.volume = 2.0 / 3.0 * kPi * e.radius * along * e.ring.depth;
  e.energy = 0.5 * body.mass * total * total;
  e.drops = static_cast<int>(std::clamp(2 * std::sqrt(e.energy), 8.0, 160.0));
  e.speed = 0.8 * v;
  e.ahead = 0.6 * across;
  e.jet_speed = 0.5 * v;
  return e;
}

}  // namespace ge::water

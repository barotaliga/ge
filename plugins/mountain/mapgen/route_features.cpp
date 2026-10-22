// The definitions behind route_features.hpp.
//
// ge:summary R42 the felled trunks and the route jumps' search, stamp and gap
// ge:keywords route-jump route-gap fell-on-routes lay-route-jumps
// ge:skill mountain
// ge:test tests/mountain_ski_routes_test.cpp
#include "mountain/mapgen/route_features.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/ski_routes.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace K = resort_rules::route;
namespace J = resort_rules::route::wild::jump;
using ge::core::hypot;
using ge::core::kPi;
using ge::core::sample_field;
using ge::core::smoothstep;

// The heading change from a to b, rad, in −π..π.
double turn_of(double a, double b) { return libm::atan2(libm::sin(b - a), libm::cos(b - a)); }

// The station of `points` at arc `s` (the nearest laid).
const TrackPoint& station_at(std::span<const TrackPoint> points, double s) {
  const double i = num::round(s / K::station);
  return points[size_t(ge::core::clamp(i, 0.0, double(points.size() - 1)))];
}

// The stations of `points` with an arc in (from, to).
std::vector<TrackPoint> stretch(std::span<const TrackPoint> points, double from, double to) {
  std::vector<TrackPoint> out;
  for (const TrackPoint& q : points) {
    if (q.s > from && q.s < to) out.push_back(q);
  }
  return out;
}

// THE GAP past a route jump's lip: a trench `length` m down the line and `jump::dig` deep at its
// middle, as wide as the jump, its sides eased back into the snow; the lip's landing lowered
// into it.
void stamp_gap(ge::core::Heightfield& ground, double x, double z, double heading, double length) {
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  const double edge = rules::kickers::edge;
  const double reach = length + J::width / 2 + edge;
  const double cell = ground.cell;
  const int c0 = int(num::max(0.0, std::floor((x - reach) / cell)));
  const int c1 = int(num::min(double(ground.cols - 1), std::ceil((x + reach) / cell)));
  const int r0 = int(num::max(0.0, std::floor((z - reach) / cell)));
  const int r1 = int(num::min(double(ground.rows - 1), std::ceil((z + reach) / cell)));
  for (int r = r0; r <= r1; r++) {
    for (int c = c0; c <= c1; c++) {
      const double dx = c * cell - x;
      const double dz = r * cell - z;
      const double u = dx * fx + dz * fz;
      if (u <= 0.5 || u >= length + 0.5) continue;
      const double v = std::abs(dx * fz - dz * fx);
      const double across = 1 - smoothstep(J::width / 2, J::width / 2 + edge, v);
      if (across <= 0) continue;
      float& h = ground.data[size_t(r) * size_t(ground.cols) + size_t(c)];
      h = static_cast<float>(h - across * J::dig * libm::sin(kPi * (u - 0.5) / length));
    }
  }
}

}  // namespace

std::vector<TreeDef> fell_on_routes(std::span<const TreeDef> trees,
                                    std::span<const SkiRoute> routes,
                                    std::span<const Kicker> kickers) {
  constexpr double kCell = 8;
  std::map<std::pair<int, int>, std::vector<const TrackPoint*>> near;
  for (const SkiRoute& r : routes) {
    for (const TrackPoint& p : r.points) {
      near[{int(std::floor(p.x / kCell)), int(std::floor(p.z / kCell))}].push_back(&p);
    }
  }
  std::vector<TreeDef> out;
  out.reserve(trees.size());
  for (const TreeDef& t : trees) {
    const double reach = K::open + t.radius;
    const int i = int(std::floor(t.x / kCell));
    const int j = int(std::floor(t.z / kCell));
    bool felled = false;
    for (int dj = -1; dj <= 1 && !felled; dj++) {
      for (int di = -1; di <= 1 && !felled; di++) {
        const auto it = near.find({i + di, j + dj});
        if (it == near.end()) continue;
        for (const TrackPoint* p : it->second) {
          if (hypot(p->x - t.x, p->z - t.z) < reach) {
            felled = true;
            break;
          }
        }
      }
    }
    if (!felled && !on_kicker(kickers, t.x, t.z, 2)) out.push_back(t);
  }
  return out;
}

std::vector<Kicker> lay_route_jumps(ge::core::Heightfield& ground, std::vector<SkiRoute>& routes) {
  const double window = rules::track::colour_window;
  std::vector<Kicker> out;
  for (const SkiRoute& r : routes) {
    const std::vector<TrackPoint>& pts = r.points;
    struct Lip {
      TrackPoint p;
      double score;
      double roll;
    };
    std::vector<Lip> lips;
    for (const TrackPoint& p : pts) {
      if (p.s < num::max(J::ends, K::lead) || p.s > r.length - J::ends) continue;
      const bool by_drop = std::any_of(
          r.drops.begin(), r.drops.end(),
          [&](const SkiRoute::Drop& d) { return std::abs(d.s - p.s) < J::clear + kDropSpan; });
      if (by_drop) continue;
      const double fx = libm::sin(p.heading);
      const double fz = libm::cos(p.heading);
      const Fall above =
          fall_along(ground, p.x - fx * J::ramp, p.z - fz * J::ramp, p.heading, J::ramp);
      const Fall below = fall_along(ground, p.x, p.z, p.heading, J::landing);
      // A ramp that would climb is no lip, and the ground must roll away.
      if (above.gentlest < 0.2) continue;
      const double roll = below.steepest - above.gentlest;
      if (roll < J::roll) continue;
      // Neither the line's colour window nor its pitch pushed past a route's by the lip's fall.
      const std::vector<TrackPoint> near = stretch(pts, p.s - window - 1e-9, p.s + window + 1e-9);
      if (steepest_along(near, window) + J::height.max / window > K::steepest) continue;
      const std::vector<TrackPoint> local = stretch(pts, p.s - 30, p.s + J::landing + 20);
      if (steepest_along(local, K::pitch_window) + 2 * J::height.max / J::landing / 2 > K::cliff) {
        continue;
      }
      // How hard the line bends where he comes down, and whether it narrows there.
      const TrackPoint& land = station_at(pts, p.s + J::landing);
      const double bend =
          std::abs(turn_of(p.heading, station_at(pts, p.s + J::landing + 20).heading));
      const double narrow = land.width < K::width ? 1 : 0;
      lips.push_back({p, roll * 12 + bend * 3 + narrow, roll});
    }
    std::stable_sort(lips.begin(), lips.end(), [](const Lip& a, const Lip& c) {
      return a.score != c.score ? a.score > c.score : a.p.s < c.p.s;
    });
    std::vector<Lip> kept;
    for (const Lip& l : lips) {
      if (kept.size() >= size_t(J::count.max)) break;
      const bool close = std::any_of(kept.begin(), kept.end(), [&](const Lip& o) {
        return std::abs(o.p.s - l.p.s) < J::spacing;
      });
      if (!close) kept.push_back(l);
    }
    std::stable_sort(kept.begin(), kept.end(),
                     [](const Lip& a, const Lip& c) { return a.p.s < c.p.s; });
    for (const Lip& l : kept) {
      const TrackPoint& p = l.p;
      const double height =
          J::height.min + (J::height.max - J::height.min) * num::min(1.0, l.roll / 0.25);
      stamp_kicker(ground, p.x, p.z, p.heading, height, J::ramp, J::landing, J::width);
      const double share = (height - J::height.min) / (J::height.max - J::height.min);
      stamp_gap(ground, p.x, p.z, p.heading, J::gap.min + (J::gap.max - J::gap.min) * share);
      Kicker k{"O" + std::to_string(out.size() + 1),
               p.x,
               p.z,
               0,
               p.heading,
               height,
               J::ramp,
               J::landing,
               J::width,
               false,
               std::nullopt};
      k.route = r.id;
      out.push_back(std::move(k));
    }
  }
  for (Kicker& k : out) k.y = sample_field(ground, k.x, k.z);
  for (SkiRoute& r : routes) {
    for (TrackPoint& p : r.points) p.y = sample_field(ground, p.x, p.z);
    r.steepest = steepest_along(r.points, window);
  }
  return out;
}

}  // namespace ge::mountain::mapgen

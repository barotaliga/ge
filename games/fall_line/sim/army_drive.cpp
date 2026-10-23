// Reading the snow ahead for the army's machines (army_drive.hpp).
//
// ge:summary way_cost, choose_way and tunnel_within: a heading walked out and costed, a fan weighed
// ge:keywords army drive way_cost choose_way tunnel_within terrain-read fan
// ge:skill sim
// ge:test tests/fall_line_army_sled_test.cpp
#include "fall_line/sim/army_drive.hpp"

#include <cmath>

#include "ai/steer.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace fall_line::sim {

namespace {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;
namespace lm = ge::core::libm;
namespace num = ge::core::num;
using ge::core::angle_diff;

// How many trunks stand within `r` m (past their radius) of (x, z).
int32_t trunks_near(const mg::Level& level, const sk::TreeIndex& trees, double x, double z,
                    double r) {
  int32_t n = 0;
  sk::for_each_tree_near(trees, x, z, [&](int32_t i) {
    const mg::TreeDef& t = level.trees[static_cast<size_t>(i)];
    const double dx = t.x - x;
    const double dz = t.z - z;
    const double reach = r + t.radius;
    if (dx * dx + dz * dz < reach * reach) n++;
  });
  return n;
}

}  // namespace

bool tunnel_within(const mg::Level& level, double x, double z, double r) {
  if (!level.resort) return false;
  for (const mg::WindTunnel& t : level.resort->tunnels) {
    const double reach = t.width / 2 + r;
    for (size_t i = 0; i + 1 < t.points.size(); i++) {
      const mg::WindTunnel::Point& a = t.points[i];
      const mg::WindTunnel::Point& c = t.points[i + 1];
      const double dx = c.x - a.x;
      const double dz = c.z - a.z;
      const double l2 = dx * dx + dz * dz;
      const double u =
          l2 > 0 ? ge::core::clamp(((x - a.x) * dx + (z - a.z) * dz) / l2, 0.0, 1.0) : 0.0;
      const double ex = x - a.x - dx * u;
      const double ez = z - a.z - dz * u;
      if (ex * ex + ez * ez < reach * reach) return true;
    }
  }
  return false;
}

WayCost way_cost(const mg::Level& level, const sk::TreeIndex& trees, double x0, double z0,
                 double heading, double want, double keep, const DriveRead& read) {
  const double fx = lm::sin(heading);
  const double fz = lm::cos(heading);
  const auto n = num::to_int32(num::round(read.look / read.step));
  double g0 = level.ground_at(x0, z0);
  double reach = read.look;
  double trees_on = 0;
  int32_t near = 0;
  const double lo = read.edge;
  const double hi = level.size - read.edge;
  for (int32_t i = 1; i <= n; i++) {
    const double x = x0 + fx * read.step * i;
    const double z = z0 + fz * read.step * i;
    const double g = level.ground_at(x, z);
    const double rise = (g - g0) / read.step;
    g0 = g;
    if (x < lo || z < lo || x > hi || z > hi || rise > read.rise || rise < -read.drop ||
        tunnel_within(level, x, z, read.tunnel)) {
      reach = read.step * (i - 1);
      break;
    }
    const int32_t k = trunks_near(level, trees, x, z, read.trunk);
    trees_on += static_cast<double>(k) / i;
    if (i <= 3) near += k;
  }
  const double cost = read.turn_cost * std::fabs(angle_diff(heading, want)) +
                      read.blocked_cost * (read.look - reach) + read.tree_cost * trees_on +
                      read.keep_cost * std::fabs(angle_diff(heading, keep));
  return {cost, near, reach};
}

WayChoice choose_way(const mg::Level& level, const sk::TreeIndex& trees, double x, double z,
                     double held, double want, const DriveRead& read) {
  const auto best = ge::ai::pick_heading(
      held, want, ge::ai::Fan{read.fan, read.spread},
      [&](double h) { return way_cost(level, trees, x, z, h, want, held, read); });
  const double way = best.heading;
  return {lm::atan2(lm::sin(way), lm::cos(way)), best.costed.trees > 0, best.costed.cost,
          best.costed.reach};
}

}  // namespace fall_line::sim

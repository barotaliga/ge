// A lift station's footprint (clearing the stations off the runs is M4b3).
//
// ge:summary A station's footprint read as points across and along its line
// ge:keywords station-clear footprint bottom-footprint
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/station_clear.hpp"

#include "core/math.hpp"

namespace ge::mountain::mapgen {

namespace {

// The step a footprint is read at, m.
constexpr double kRead = 3;

// A station's footprint as points to read, its wheel at `at`, the line running `dir`. The
// steps along and across are `while` loops (a float loop counter is flagged by the linter), each
// value the running sum.
std::vector<PlanPoint> footprint(LiftKind kind, PlanPoint at, PlanPoint dir) {
  const auto& F = footprint_of(kind);
  std::vector<PlanPoint> out;
  double u = -F.back;
  while (u <= F.ahead + 1e-6) {
    double v = -F.half;
    while (v <= F.half + 1e-6) {
      out.push_back({at.x + dir.x * u + dir.z * v, at.z + dir.z * u - dir.x * v});
      v += kRead;
    }
    u += kRead;
  }
  return out;
}

}  // namespace

std::vector<PlanPoint> bottom_footprint(LiftKind kind, PlanPoint bottom, PlanPoint top) {
  const double h = ge::core::hypot(top.x - bottom.x, top.z - bottom.z);
  const double len = h != 0 ? h : 1;
  return footprint(kind, bottom, {(top.x - bottom.x) / len, (top.z - bottom.z) / len});
}

}  // namespace ge::mountain::mapgen

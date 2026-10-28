// What the village's streets carry for the traffic (furniture.hpp): the lanes off each street's
// centreline, the crossings at its ends, the bays along its parking strip and in the car park, the
// bus stop on the square's edge and the loop round the village.
//
// ge:summary The village streets' lanes, crossings, bays, bus stop and loop, built off the plan
// ge:keywords village furniture lanes bays crossings bus-stop loop furnish-streets
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#include "mountain/village/furniture.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/village/traffic_rules.hpp"

namespace ge::mountain::village {

namespace {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::hypot;
using mg::PlanPoint;

// The arc along `points` nearest (x, z).
double nearest_s(const std::vector<StreetPoint>& points, double x, double z) {
  double best = std::numeric_limits<double>::infinity();
  double s = 0;
  for (const StreetPoint& p : points) {
    const double d = hypot(p.x - x, p.z - z);
    if (d < best) {
      best = d;
      s = p.s;
    }
  }
  return s;
}

// Of the streets `ids`, the one passing nearest (x, z), and the arc on it.
struct Hit {
  int32_t street;
  double s;
};
std::optional<Hit> nearest_on(const Streets& plan, const std::vector<int32_t>& ids, double x,
                              double z) {
  std::optional<Hit> out;
  double best = std::numeric_limits<double>::infinity();
  for (const int32_t id : ids) {
    for (const StreetPoint& p : plan.streets[size_t(id)].points) {
      const double d = hypot(p.x - x, p.z - z);
      if (d < best) {
        best = d;
        out = Hit{id, p.s};
      }
    }
  }
  return out;
}

bool near_crossing(const std::vector<Crossing>& crossings, int32_t street, double s, double d) {
  return std::any_of(crossings.begin(), crossings.end(), [&](const Crossing& c) {
    return c.street == street && std::fabs(c.s - s) < d;
  });
}

}  // namespace

Trim trim_at(const Streets& plan, const Street& st, int end) {
  const Junction& j = plan.junctions[size_t(end == 0 ? st.from : st.to)];
  if (j.exit) return {0, 0};
  double walk = 0;
  double road = 0;
  for (const int32_t k : j.streets) {
    const Street& o = plan.streets[size_t(k)];
    if (&o == &st) continue;
    walk = num::max(walk, num::max(side_reach(o.section, 0), side_reach(o.section, 1)));
    road =
        num::max(road, o.section.lane + num::max(o.section.sides[0].park, o.section.sides[1].park));
  }
  return {walk + 0.5, road};
}

std::vector<StreetPoint> offset_line(const mg::Level& level, const Street& st, double lat,
                                     double s0, double s1, double step) {
  const int n = std::max(1, int(std::ceil(std::fabs(s1 - s0) / step)));
  std::vector<PlanPoint> pts;
  pts.reserve(size_t(n) + 1);
  for (int i = 0; i <= n; i++) {
    pts.push_back(beside_point(street_at(st, s0 + ((s1 - s0) * i) / n), lat));
  }
  return resample(level, pts, step);
}

Furniture furnish_streets(const mg::Level& level, const Streets& plan) {
  namespace F = furniture;
  Furniture out;
  const auto n_streets = int32_t(plan.streets.size());
  out.lanes.reserve(size_t(n_streets) * 2);
  for (int32_t k = 0; k < n_streets; k++) {
    const Street& st = plan.streets[size_t(k)];
    const Section& S = st.section;
    // THE LANES: one each way, in the middle of its half, on the right of its way as a driver sees
    // it: the left of the street's own way (`beside_point`'s negative side) for +1.
    for (const int32_t dir : {1, -1}) {
      std::vector<StreetPoint> pts = dir == 1 ? offset_line(level, st, -S.lane / 2, 0, st.length)
                                              : offset_line(level, st, S.lane / 2, st.length, 0);
      const double length = pts.back().s;
      out.lanes.push_back({st.id + (dir > 0 ? "+" : "-"), k, dir, dir > 0 ? st.from : st.to,
                           dir > 0 ? st.to : st.from, std::move(pts), length, S.lane});
    }
    // THE CROSSINGS at both ends of a street with a sidewalk, where it meets another street.
    const Trim t0 = trim_at(plan, st, 0);
    const Trim t1 = trim_at(plan, st, 1);
    if (S.sides[0].walk > 0 || S.sides[1].walk > 0) {
      for (const int end : {0, 1}) {
        const Junction& j = plan.junctions[size_t(end == 0 ? st.from : st.to)];
        if (j.exit || j.streets.size() < 2) continue;
        const double off = (end == 0 ? t0 : t1).road + F::kCrossingOff + F::kCrossingWidth / 2;
        if (off * 2 > st.length) continue;
        const double s = end == 0 ? off : st.length - off;
        const StreetPoint at = street_at(st, s);
        out.crossings.push_back({k, s, at.x, at.y, at.z, at.heading,
                                 2 * S.lane + S.sides[0].park + S.sides[1].park,
                                 F::kCrossingWidth});
      }
    }
    // THE KERBSIDE BAYS along a parking strip, clear of the crossings.
    for (const int side : {0, 1}) {
      const StreetSide& sd = S.sides[size_t(side)];
      if (sd.park <= 0) continue;
      const double sign = side == 0 ? -1 : 1;
      const double len = F::kKerbLength;
      const double first = t0.walk + len / 2 + 2;
      const double last = st.length - t1.walk - len / 2 - 2;
      for (int i = 0; first + len * i <= last; i++) {
        const double s = first + len * i;
        if (near_crossing(out.crossings, k, s, len / 2 + 2)) continue;
        const StreetPoint at = street_at(st, s);
        const PlanPoint q = beside_point(at, sign * (S.lane + sd.park / 2));
        // Parked with the traffic on its side of the street: side 0 carries the way the street
        // runs, side 1 the way back.
        out.bays.push_back({st.id + "k" + std::to_string(out.bays.size() + 1), BayKind::kerb, k,
                            q.x, level.ground_at(q.x, q.z), q.z,
                            side == 0 ? at.heading : at.heading + ge::core::kPi, len,
                            F::kKerbWidth});
      }
    }
  }
  // THE CAR PARK'S BAYS: a row either side of each aisle in from the street, square to it, from
  // past the main street's reach to the far aisle's edge.
  const int hub_side = plan.valley > 0 ? 1 : 0;
  const double main_reach = side_reach(section_of(StreetKind::main), hub_side);
  for (int32_t k = 0; k < n_streets; k++) {
    const Street& st = plan.streets[size_t(k)];
    if (st.id != "P1" && st.id != "P3") continue;
    const bool inward = st.id == "P1";
    const double lane = st.section.lane;
    const double near = main_reach + 1;
    const double far = st.length - lane - 0.5;
    const double w = areas::kBayWidth;
    for (int i = 0; near + w / 2 + w * i <= far - w / 2; i++) {
      const double d = near + w / 2 + w * i;
      const StreetPoint at = street_at(st, inward ? d : st.length - d);
      for (const double side : {-1.0, 1.0}) {
        const PlanPoint q = beside_point(at, side * (lane + areas::kBayLength / 2));
        // Nose in, away from the aisle.
        out.bays.push_back({st.id + "b" + std::to_string(out.bays.size() + 1), BayKind::lot, k, q.x,
                            level.ground_at(q.x, q.z), q.z, libm::atan2(q.x - at.x, q.z - at.z),
                            areas::kBayLength, areas::kBayWidth});
      }
    }
  }
  // A CROSSING over the main street at the square, and THE BUS STOP on the square's edge, the side
  // away from the car park.
  const auto sq = std::find_if(plan.areas.begin(), plan.areas.end(),
                               [](const VillageArea& a) { return a.kind == AreaKind::square; });
  if (sq != plan.areas.end()) {
    if (const std::optional<Hit> hit = nearest_on(plan, plan.main, plan.square, sq->z)) {
      const Street& st = plan.streets[size_t(hit->street)];
      const StreetPoint at = street_at(st, hit->s);
      out.crossings.push_back({hit->street, hit->s, at.x, at.y, at.z, at.heading,
                               2 * st.section.lane, F::kCrossingWidth});
    }
    const double away = plan.carpark_side == 0 ? 1 : -plan.carpark_side;
    const double bx = plan.square + away * (areas::kSquareAlong / 2 - F::kBusAlong / 2 - 1);
    if (const std::optional<Hit> b = nearest_on(plan, plan.main, bx, sq->z)) {
      const Street& st = plan.streets[size_t(b->street)];
      const StreetPoint at = street_at(st, b->s);
      const double sign = hub_side == 0 ? -1 : 1;
      const PlanPoint q =
          beside_point(at, sign * (side_reach(st.section, hub_side) + F::kBusDeep / 2 + 0.3));
      // The bus stands on the lane on the square's side.
      const int32_t lane = lane_of(b->street, sign < 0 ? 1 : -1);
      out.bus = BusStop{q.x,
                        level.ground_at(q.x, q.z),
                        q.z,
                        libm::atan2(at.x - q.x, at.z - q.z),
                        b->street,
                        lane,
                        nearest_s(out.lanes[size_t(lane)].points, at.x, at.z)};
    }
  }
  // THE LOOP: the main street east, the east end's cross street down, the back street west, the
  // west end's cross street up.
  for (const int32_t id : plan.main) out.loop.push_back(lane_of(id, 1));
  out.loop.push_back(lane_of(plan.end_cross[1], 1));
  for (auto it = plan.back.rbegin(); it != plan.back.rend(); ++it) {
    out.loop.push_back(lane_of(*it, -1));
  }
  out.loop.push_back(lane_of(plan.end_cross[0], -1));
  return out;
}

}  // namespace ge::mountain::village

// The village's traffic (traffic.hpp): the routes each kind of vehicle drives, the phases they
// are placed at so no two ever meet, the cars parked, and a vehicle read back at a moment.
//
// ge:summary The village's traffic planned (routes, phases, parked cars) and read at a moment
// ge:keywords traffic plan-traffic phases visitor-route bus-route through-route loop-route
// ge:keywords parked swept-bays vehicle-at swing-out
// ge:skill mountain
// ge:test tests/mountain_traffic_test.cpp
#include "mountain/village/traffic.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::village {

namespace {

namespace mg = ge::mountain::mapgen;
namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace T = traffic;
using ge::core::hypot;
using mg::PlanPoint;

constexpr double kInf = std::numeric_limits<double>::infinity();

// A small unit hash off the seed, a name and a number, on the traffic's salt.
double deal(uint32_t seed, const std::string& id, int32_t k) {
  return pick(seed, id.c_str(), k, kTrafficSalt);
}

// A plan in the making: the map, its streets, their furniture, the turns each lane may take.
struct Making {
  const mg::Level& level;
  const Streets& plan;
  const Furniture& f;
  std::vector<std::vector<int32_t>> next;
  int32_t S;
};

// The street named `id`, or -1.
int32_t street_named(const Streets& plan, const char* id) {
  for (size_t k = 0; k < plan.streets.size(); k++) {
    if (plan.streets[k].id == id) return int32_t(k);
  }
  return -1;
}

const Street& street_of(const Making& m, int32_t lane) {
  return m.plan.streets[size_t(m.f.lanes[size_t(lane)].street)];
}

// THE LANES' WAYS: from each lane, the lanes it may turn onto (another street at the junction it
// reaches, never back along its own).
std::vector<std::vector<int32_t>> next_lanes(const std::vector<Lane>& lanes) {
  std::vector<std::vector<int32_t>> out(lanes.size());
  for (size_t a = 0; a < lanes.size(); a++) {
    for (size_t b = 0; b < lanes.size(); b++) {
      if (lanes[b].from == lanes[a].to && lanes[b].street != lanes[a].street) {
        out[a].push_back(int32_t(b));
      }
    }
  }
  return out;
}

// THE SHORTEST WAY from lane `a` to lane `b`, both in, by the lanes' lengths: through the car
// park's aisles and down a road out only where one of them is `b`.
std::optional<std::vector<int32_t>> shortest(const Making& m, int32_t a, int32_t b) {
  const size_t n = m.f.lanes.size();
  std::vector<double> dist(n, kInf);
  std::vector<int32_t> prev(n, -1);
  std::vector<uint8_t> done(n, 0);
  dist[size_t(a)] = 0;
  for (;;) {
    int32_t u = -1;
    for (size_t i = 0; i < n; i++) {
      if (!done[i] && dist[i] < kInf && (u < 0 || dist[i] < dist[size_t(u)])) u = int32_t(i);
    }
    if (u < 0) return std::nullopt;
    if (u == b) break;
    done[size_t(u)] = 1;
    for (const int32_t w : m.next[size_t(u)]) {
      const StreetKind kind = street_of(m, w).kind;
      if (w != b && (kind == StreetKind::aisle || kind == StreetKind::road)) continue;
      const double d = dist[size_t(u)] + m.f.lanes[size_t(w)].length;
      if (d < dist[size_t(w)]) {
        dist[size_t(w)] = d;
        prev[size_t(w)] = u;
      }
    }
  }
  std::vector<int32_t> out;
  for (int32_t u = b; u >= 0; u = prev[size_t(u)]) out.push_back(u);
  std::reverse(out.begin(), out.end());
  if (out.front() != a) return std::nullopt;
  return out;
}

std::vector<Visit> visits_of(const Making& m, std::span<const int32_t> ids) {
  std::vector<Visit> out;
  out.reserve(ids.size());
  for (const int32_t i : ids) out.push_back(whole_visit(m.plan, m.f, i));
  return out;
}

// A line driven from `v0` to `v1` along `visits` (and on into `tail`, its first point left off).
Driven driven(const Making& m, std::span<const Visit> visits, const Drive& drive, double v0,
              double v1, std::span<const RawPoint> tail = {}) {
  std::vector<RawPoint> raw = raw_line(m.level, m.plan, m.f, visits, Keep::middle, drive, false);
  if (tail.size() > 1) raw.insert(raw.end(), tail.begin() + 1, tail.end());
  return drive_path(lay_path(raw, false), drive, v0, v1);
}

// A part of a route as it is put together: a line driven, a stand or a time off the map.
struct Part {
  LegKind kind;
  std::optional<Driven> d;
  double duration;
};
Part drive_part(Driven d) {
  const double t = d.duration;
  return {LegKind::drive, std::move(d), t};
}
Part wait_part(double s) { return {LegKind::wait, std::nullopt, s}; }

// A route of parts, the last hidden for what is left of `period`; nothing where they overrun it.
std::optional<Route> route_of(Role role, double period, std::vector<Part> parts) {
  Route r{{}, {}, period, 0, role};
  double start = 0;
  double odo = 0;
  for (Part& p : parts) {
    if (p.kind == LegKind::drive && p.d) {
      const double rolled = (p.d->reverse ? -1 : 1) * roll_of(*p.d);
      r.legs.push_back({LegKind::drive, start, p.duration, odo, int32_t(r.driven.size())});
      r.driven.push_back(std::move(*p.d));
      start += p.duration;
      odo += rolled;
    } else if (p.kind == LegKind::wait) {
      r.legs.push_back({LegKind::wait, start, p.duration, odo, int32_t(r.driven.size()) - 1});
      start += p.duration;
    } else {
      r.legs.push_back({LegKind::hide, start, p.duration, odo, -1});
      start += p.duration;
    }
  }
  if (start > period) return std::nullopt;
  if (start < period) r.legs.push_back({LegKind::hide, start, period - start, odo, -1});
  r.roll = odo;
  return r;
}

// The whole fraction of the period nearest `t` s, its laps a divisor of the samples so a phase on
// the sample grid stays on it.
double fit_period(int32_t S, double t) {
  const double want = num::max(1.0, num::round((S * T::kSample) / t));
  int32_t best = 1;
  for (int32_t m = 1; m <= S; m++) {
    if (S % m == 0 && std::fabs(m - want) < std::fabs(best - want)) best = m;
  }
  return (S * T::kSample) / best;
}

// A LOOP round the village, the way `ids` go, kept to `keep`.
Route loop_route(const Making& m, std::span<const int32_t> ids, Keep keep, const Drive& drive,
                 Role role) {
  const std::vector<Visit> visits = visits_of(m, ids);
  const Path path = lay_path(raw_line(m.level, m.plan, m.f, visits, keep, drive, true), true);
  const Driven free = drive_path(path, drive, 0, 0);
  const double period = fit_period(m.S, free.duration);
  Driven d = drive_path(path, drive, 0, 0, false, period / free.duration);
  const double roll = roll_of(d);
  Route r{{{LegKind::drive, 0, period, 0, 0}}, {}, period, roll, role};
  r.driven.push_back(std::move(d));
  return r;
}

// A road out: its lane in (off the map), its lane out and the junction it leaves the village at.
struct RoadOut {
  int32_t into;
  int32_t out;
  int32_t back;
};
std::vector<RoadOut> roads_of(const Making& m) {
  std::vector<RoadOut> out;
  for (size_t k = 0; k < m.plan.streets.size(); k++) {
    const Street& st = m.plan.streets[k];
    if (st.kind != StreetKind::road) continue;
    out.push_back({lane_of(int32_t(k), -1), lane_of(int32_t(k), 1), st.from});
  }
  return out;
}

// A car IN off the road to `bay`, parked `stay` s, backed out and away down a road.
std::optional<Route> visitor_route(const Making& m, VehicleKind kind, const Bay& bay, int32_t into,
                                   int32_t out, double stay) {
  const Drive drive = drive_of(Driver::car);
  const int32_t p1 = street_named(m.plan, "P1");
  const int32_t p2 = street_named(m.plan, "P2");
  const int32_t p3 = street_named(m.plan, "P3");
  if (p1 < 0 || p2 < 0 || p3 < 0) return std::nullopt;
  const int32_t aisle = lane_of(bay.street, 1);
  const Lane& lane = m.f.lanes[size_t(aisle)];
  const Street& st = m.plan.streets[size_t(bay.street)];
  const Visit whole = whole_visit(m.plan, m.f, aisle);
  const double sb = arc_nearest(st, bay.x, bay.z);
  const double sa = sb - T::kApproach;
  if (sa < whole.s0 + 1 || sb > whole.s1) return std::nullopt;
  // In: off the road to the car park's first aisle, on round to the bay's.
  const std::optional<std::vector<int32_t>> to_aisle = shortest(m, into, lane_of(p1, 1));
  if (!to_aisle) return std::nullopt;
  std::vector<int32_t> ids = *to_aisle;
  if (bay.street == p3) {
    ids.push_back(lane_of(p2, 1));
    ids.push_back(lane_of(p3, 1));
  }
  std::vector<Visit> visits = visits_of(m, ids);
  visits.back().s1 = sa;
  const StreetPoint p = street_at(st, sa);
  const PlanPoint a = beside_point(p, lane_offset(st, lane, Keep::middle));
  const PlanPoint ta{libm::sin(p.heading), libm::cos(p.heading)};
  const double back = axle_to_middle(kind);
  const PlanPoint fb{libm::sin(bay.heading), libm::cos(bay.heading)};
  const PlanPoint b{bay.x - fb.x * back, bay.z - fb.z * back};
  std::vector<RawPoint> turn = bay_turn(a, ta, b, fb, drive.aisle);
  Driven in_leg = driven(m, visits, drive, drive.road, 0, turn);
  // Backed out along the same turn, and away.
  std::reverse(turn.begin(), turn.end());
  Driven back_out = drive_path(lay_path(turn, false), drive, 0, 0, true);
  std::vector<int32_t> onward;
  if (bay.street == p1) onward = {lane_of(p2, 1), lane_of(p3, 1)};
  const int32_t last = onward.empty() ? aisle : onward.back();
  const std::optional<std::vector<int32_t>> away = shortest(m, last, out);
  if (!away) return std::nullopt;
  std::vector<Visit> out_visits{{aisle, sa, whole.s1}};
  for (const Visit& v : visits_of(m, onward)) out_visits.push_back(v);
  for (const Visit& v : visits_of(m, std::span(*away).subspan(1))) out_visits.push_back(v);
  Driven out_leg = driven(m, out_visits, drive, 0, drive.road);
  std::vector<Part> parts;
  parts.push_back(drive_part(std::move(in_leg)));
  parts.push_back(wait_part(stay));
  parts.push_back(drive_part(std::move(back_out)));
  parts.push_back(drive_part(std::move(out_leg)));
  return route_of(Role::visitor, m.S * T::kSample, std::move(parts));
}

// A car through: in off one road and out by another, or round the loop and back out the way it
// came where there is one road.
std::optional<Route> through_route(const Making& m, int32_t k,
                                   const std::array<std::vector<int32_t>, 2>& loop_ids) {
  const Drive drive = drive_of(Driver::car);
  const std::vector<RoadOut> roads = roads_of(m);
  if (roads.empty()) return std::nullopt;
  const uint32_t seed = num::to_uint32(m.level.seed);
  const size_t ri =
      size_t(std::floor(deal(seed, "through", k) * double(roads.size()))) % roads.size();
  const RoadOut& r = roads[ri];
  std::optional<std::vector<int32_t>> ids;
  if (roads.size() > 1) {
    const RoadOut& o = roads[ri == 0 ? 1 : 0];
    ids = shortest(m, r.into, o.out);
  } else {
    const std::vector<int32_t>& loop = loop_ids[size_t(k % 2)];
    const auto at = std::find_if(loop.begin(), loop.end(),
                                 [&](int32_t i) { return m.f.lanes[size_t(i)].from == r.back; });
    if (at != loop.end()) {
      std::vector<int32_t> v{r.into};
      v.insert(v.end(), at, loop.end());
      v.insert(v.end(), loop.begin(), at);
      v.push_back(r.out);
      ids = std::move(v);
    }
  }
  if (!ids) return std::nullopt;
  std::vector<Part> parts;
  parts.push_back(drive_part(driven(m, visits_of(m, *ids), drive, drive.road, drive.road)));
  return route_of(Role::through, m.S * T::kSample, std::move(parts));
}

// THE SKI BUS: in off a road to the stop, standing `stay` s, and away.
std::optional<Route> bus_route(const Making& m, double stay) {
  const std::vector<RoadOut> roads = roads_of(m);
  if (!m.f.bus || roads.empty()) return std::nullopt;
  const BusStop& bus = *m.f.bus;
  const Drive drive = drive_of(Driver::bus);
  const int32_t at = bus.lane;
  const Lane& lane = m.f.lanes[size_t(at)];
  const Street& st = m.plan.streets[size_t(lane.street)];
  // The stop's point on its lane.
  PlanPoint stop_pt{lane.points.back().x, lane.points.back().z};
  for (size_t i = 1; i < lane.points.size(); i++) {
    const StreetPoint& a = lane.points[i - 1];
    const StreetPoint& b = lane.points[i];
    if (b.s >= bus.s) {
      const double u = b.s > a.s ? num::min(1.0, (bus.s - a.s) / (b.s - a.s)) : 0;
      stop_pt = {a.x + (b.x - a.x) * u, a.z + (b.z - a.z) * u};
      break;
    }
  }
  // The stop's arc on the street, the bus's rear axle that far short of the shelter's middle so
  // the doors stand at it.
  const VehicleSize V = vehicle_size(VehicleKind::bus);
  const double sm = arc_nearest(st, stop_pt.x, stop_pt.z) - lane.dir * (V.length / 2 - V.front);
  const Visit whole = whole_visit(m.plan, m.f, at);
  const double lo = num::min(whole.s0, whole.s1);
  const double hi = num::max(whole.s0, whole.s1);
  const double stop = num::max(lo + 2, num::min(hi - 2, sm));
  const uint32_t seed = num::to_uint32(m.level.seed);
  const auto road = [&](int32_t k) {
    return roads[size_t(std::floor(deal(seed, "bus", k) * double(roads.size()))) % roads.size()];
  };
  const RoadOut r = road(0);
  const RoadOut o = road(1);
  const std::optional<std::vector<int32_t>> in_ids = shortest(m, r.into, at);
  const std::optional<std::vector<int32_t>> out_ids = shortest(m, at, o.out);
  if (!in_ids || !out_ids) return std::nullopt;
  std::vector<Visit> in_visits = visits_of(m, *in_ids);
  in_visits.back() = {whole.lane, whole.s0, stop};
  std::vector<Visit> out_visits{{whole.lane, stop, whole.s1}};
  for (const Visit& v : visits_of(m, std::span(*out_ids).subspan(1))) out_visits.push_back(v);
  std::vector<Part> parts;
  parts.push_back(drive_part(driven(m, in_visits, drive, drive.road, 0)));
  parts.push_back(wait_part(stay));
  parts.push_back(drive_part(driven(m, out_visits, drive, 0, drive.road)));
  return route_of(Role::bus, m.S * T::kSample, std::move(parts));
}

// A car's class, dealt by the shares (no van in the day car park).
VehicleKind car_kind(double u, bool lot) {
  std::array<double, 4> shares = T::kKinds;
  if (lot) shares[3] = 0;
  const double total = shares[0] + shares[1] + shares[2] + shares[3];
  double x = u * total;
  constexpr std::array<VehicleKind, 4> kinds{VehicleKind::hatch, VehicleKind::estate,
                                             VehicleKind::suv, VehicleKind::van};
  for (size_t i = 0; i < 4; i++) {
    x -= shares[i];
    if (x < 0) return kinds[i];
  }
  return VehicleKind::hatch;
}

// The SAMPLES of a route: its rear axle every `kSample` s round its period, for the plan's check.
struct Samples {
  int32_t n = 0;
  std::vector<RoutePose> at;
};
Samples samples_of(const Route& route) {
  Samples s;
  s.n = int32_t(num::round(route.period / T::kSample));
  s.at.reserve(size_t(s.n));
  for (int32_t i = 0; i < s.n; i++) s.at.push_back(route_at(route, i * T::kSample));
  return s;
}

// A vehicle's box: its middle, its way, its half length and half width.
struct Box {
  double x;
  double z;
  double fx;
  double fz;
  double hl;
  double hw;
};

// The box a vehicle keeps at a sample: its body, run on ahead by the gap it keeps.
Box room_of(VehicleKind kind, const RoutePose& p) {
  const VehicleSize V = vehicle_size(kind);
  const double fx = libm::sin(p.facing);
  const double fz = libm::cos(p.facing);
  const double mid = axle_to_middle(kind);
  const double gap =
      (kind == VehicleKind::bike ? 0.6 : T::kMarginGap) + T::kMarginHeadway * std::fabs(p.speed);
  const double sign = p.speed < 0 ? -1 : 1;
  return {p.x + fx * mid + sign * fx * (gap / 2),
          p.z + fz * mid + sign * fz * (gap / 2),
          fx,
          fz,
          V.length / 2 + gap / 2,
          V.width / 2 + T::kMarginSide};
}

// Whether two boxes overlap (the separating axes of both).
bool overlap(const Box& a, const Box& b) {
  const double dx = b.x - a.x;
  const double dz = b.z - a.z;
  const std::array<std::array<double, 2>, 4> axes{
      {{a.fx, a.fz}, {a.fz, -a.fx}, {b.fx, b.fz}, {b.fz, -b.fx}}};
  for (const auto& [ax, az] : axes) {
    const double ra =
        a.hl * std::fabs(a.fx * ax + a.fz * az) + a.hw * std::fabs(a.fz * ax - a.fx * az);
    const double rb =
        b.hl * std::fabs(b.fx * ax + b.fz * az) + b.hw * std::fabs(b.fz * ax - b.fx * az);
    if (std::fabs(dx * ax + dz * az) > ra + rb) return false;
  }
  return true;
}

// A vehicle placed on the streets, for the check.
struct Placed {
  VehicleKind kind;
  const Samples* s;
  int32_t shift;
};

int64_t cell_key(int64_t i, int64_t cx, int64_t cz) {
  return (i * 8192 + (cx + 4096)) * 8192 + (cz + 4096);
}

// The bays some vehicle's body passes over at some moment (a hand's breadth round it): no car is
// parked in those.
std::unordered_set<int32_t> swept_bays(const std::vector<Bay>& bays,
                                       const std::vector<Placed>& placed) {
  constexpr double kC = 8;
  std::unordered_map<int64_t, std::vector<int32_t>> cells;
  for (size_t k = 0; k < bays.size(); k++) {
    cells[cell_key(0, int64_t(std::floor(bays[k].x / kC)), int64_t(std::floor(bays[k].z / kC)))]
        .push_back(int32_t(k));
  }
  std::unordered_set<int32_t> out;
  for (const Placed& p : placed) {
    const VehicleSize V = vehicle_size(p.kind);
    const double mid = axle_to_middle(p.kind);
    for (const RoutePose& q : p.s->at) {
      if (!q.shown) continue;
      const double fx = libm::sin(q.facing);
      const double fz = libm::cos(q.facing);
      const Box a{q.x + fx * mid, q.z + fz * mid, fx, fz, V.length / 2 + 0.15, V.width / 2 + 0.15};
      const auto cx = int64_t(std::floor(a.x / kC));
      const auto cz = int64_t(std::floor(a.z / kC));
      for (int64_t ox = -1; ox <= 1; ox++) {
        for (int64_t oz = -1; oz <= 1; oz++) {
          const auto it = cells.find(cell_key(0, cx + ox, cz + oz));
          if (it == cells.end()) continue;
          for (const int32_t k : it->second) {
            if (out.count(k) != 0) continue;
            const Bay& bay = bays[size_t(k)];
            const Box b{bay.x,
                        bay.z,
                        libm::sin(bay.heading),
                        libm::cos(bay.heading),
                        bay.length / 2 - 0.4,
                        bay.width / 2 - 0.3};
            if (overlap(a, b)) out.insert(k);
          }
        }
      }
    }
  }
  return out;
}

// The cell of the plan's check a point is in, m.
constexpr double kCell = 24;

// Where a driven leg leaves its vehicle at `i` + `u` of a step.
void on_leg(const Driven& d, int32_t i, double u, RoutePose& out) {
  const Path& p = d.path;
  const int32_t n = p.n;
  const auto a = size_t(i);
  const auto j = size_t(p.closed ? (i + 1) % n : std::min(n - 1, i + 1));
  out.x = p.x[a] + (p.x[j] - p.x[a]) * u;
  out.z = p.z[a] + (p.z[j] - p.z[a]) * u;
  const double dh = libm::atan2(libm::sin(p.h[j] - p.h[a]), libm::cos(p.h[j] - p.h[a]));
  const double h = p.h[a] + dh * u;
  out.facing = d.reverse ? h + ge::core::kPi : h;
  out.bend = (p.k[a] + (p.k[j] - p.k[a]) * u) * (d.reverse ? -1 : 1);
  out.lane = u < 0.5 ? p.lane[a] : p.lane[j];
  out.turn = d.reverse ? 0 : p.turn[a];
}

// How far a car swings out round a cyclist on its lane at `t`, m.
double swing_out(const TrafficPlan& plan, const Vehicle& veh, const RoutePose& p, double t) {
  if (veh.kind == VehicleKind::bike || p.lane < 0) return 0;
  const double fx = libm::sin(p.facing);
  const double fz = libm::cos(p.facing);
  const double mid = axle_to_middle(veh.kind);
  // Beside him while any of the car's length is beside any of the bicycle's.
  const double half =
      vehicle_size(veh.kind).length / 2 + vehicle_size(VehicleKind::bike).length / 2;
  double most = 0;
  for (const Vehicle& b : plan.vehicles) {
    if (b.kind != VehicleKind::bike) continue;
    const RoutePose o = route_at(plan.routes[size_t(b.route)], t + b.phase);
    if (!o.shown || o.lane != p.lane) continue;
    const double along =
        (o.x - p.x) * fx + (o.z - p.z) * fz - mid + axle_to_middle(VehicleKind::bike);
    if (along > T::kPassAhead + half || along < -T::kPassBehind - half) continue;
    // Out full beside him, eased in over the way up to him and back.
    const double lead = num::max(0.0, along - half) / T::kPassAhead;
    const double tail = num::max(0.0, -along - half) / T::kPassBehind;
    const double w = 1 - num::min(1.0, num::max(lead, tail));
    const double swing = pass_swing(veh.kind, plan.furniture.lanes[size_t(p.lane)].width);
    most = num::max(most, swing * w * w * (3 - 2 * w));
  }
  return most;
}

}  // namespace

RoutePose route_at(const Route& route, double tau) {
  const double P = route.period;
  const double laps = std::floor(tau / P);
  const double u = tau - laps * P;
  const Leg* leg = &route.legs.back();
  for (const Leg& l : route.legs) {
    if (u < l.start + l.duration) {
      leg = &l;
      break;
    }
  }
  const double into = num::max(0.0, u - leg->start);
  RoutePose out;
  if (leg->kind == LegKind::hide) {
    out.odo = laps * route.roll + leg->odo;
    return out;
  }
  out.shown = true;
  const Driven& d = route.driven[size_t(leg->driven)];
  if (leg->kind == LegKind::wait) {
    on_leg(d, d.path.closed ? 0 : d.path.n - 1, 0, out);
    out.turn = 0;
    out.odo = laps * route.roll + leg->odo;
    return out;
  }
  // A driven leg: the point it is at, the speed there and how it changes.
  const std::vector<double>& t = d.t;
  const int32_t last_t = int32_t(t.size()) - 1;
  const int32_t n = d.path.n;
  int32_t i = 0;
  double f = 0;
  double speed = 0;
  if (into >= t[size_t(last_t)]) {
    i = d.path.closed ? last_t - 1 : n - 2;
    f = 1;
    speed = d.v[size_t(d.path.closed ? 0 : n - 1)];
  } else {
    int32_t lo = 0;
    int32_t hi = last_t;
    while (hi - lo > 1) {
      const int32_t mid = (lo + hi) / 2;
      if (t[size_t(mid)] <= into) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    const double va = d.v[size_t(lo)];
    const double vb = d.v[size_t((lo + 1) % n)];
    const double span = t[size_t(lo) + 1] - t[size_t(lo)];
    const double tt = into - t[size_t(lo)];
    const double a = span > 0 ? (vb - va) / span : 0;
    const double s = va * tt + 0.5 * a * tt * tt;
    const double step = span > 0 ? ((va + vb) / 2) * span : d.ds;
    i = lo;
    f = step > 1e-9 ? num::max(0.0, num::min(1.0, s / step)) : 0;
    speed = va + a * tt;
    out.accel = a;
  }
  on_leg(d, i, f, out);
  out.speed = d.reverse ? -speed : speed;
  const double rolled = (i + f) * d.ds;
  out.odo = laps * route.roll + leg->odo + (d.reverse ? -rolled : rolled);
  return out;
}

VehiclePose vehicle_at(const mg::Level& level, const TrafficPlan& plan, int32_t k, double t) {
  const Vehicle& veh = plan.vehicles[size_t(k)];
  const VehicleSize V = vehicle_size(veh.kind);
  const RoutePose rear = route_at(plan.routes[size_t(veh.route)], t + veh.phase);
  VehiclePose out;
  out.kind = veh.kind;
  out.shown = rear.shown;
  out.lane = rear.lane;
  out.roll = rear.odo;
  if (!rear.shown) return out;
  const double fx = libm::sin(rear.facing);
  const double fz = libm::cos(rear.facing);
  const double mid = axle_to_middle(veh.kind);
  const double swing = swing_out(plan, veh, rear, t);
  // The heading's right is (cos h, -sin h); a lane keeps its kerb on the other side, so out round
  // a cyclist is toward it.
  out.x = rear.x + fx * mid + fz * swing;
  out.z = rear.z + fz * mid - fx * swing;
  out.y = level.ground_at(out.x, out.z);
  out.heading = rear.facing;
  out.speed = rear.speed;
  out.steer = num::max(-0.6, num::min(0.6, libm::atan(V.wheelbase * rear.bend)));
  const double moving = std::fabs(rear.speed);
  const double way = rear.speed < 0 ? -1 : 1;
  out.brake = moving < 0.2 || rear.accel * way < -0.35;
  out.signal =
      rear.speed > 0.1 && std::fabs(rear.turn) > T::kSignalTurn ? (rear.turn > 0 ? 1 : -1) : 0;
  return out;
}

std::optional<TrafficPlan> plan_traffic(const mg::Level& level, const Streets& streets) {
  Furniture f = furnish_streets(level, streets);
  if (f.lanes.empty()) return std::nullopt;
  const auto S = int32_t(num::round(T::kPeriod / T::kSample));
  const Making m{level, streets, f, next_lanes(f.lanes), S};
  const uint32_t seed = num::to_uint32(level.seed);
  std::vector<Route> routes;
  struct Wanted {
    VehicleKind kind;
    int32_t route;
    Role role;
  };
  std::vector<Wanted> wanted;
  const auto add = [&](std::optional<Route> route, VehicleKind kind, Role role) {
    if (!route) return;
    routes.push_back(std::move(*route));
    wanted.push_back({kind, int32_t(routes.size()) - 1, role});
  };
  // The loop's lanes, and the same the other way round.
  const std::array<std::vector<int32_t>, 2> loops = [&] {
    std::vector<int32_t> b;
    b.reserve(f.loop.size());
    for (auto it = f.loop.rbegin(); it != f.loop.rend(); ++it) b.push_back(*it ^ 1);
    return std::array<std::vector<int32_t>, 2>{f.loop, b};
  }();
  // THE BUS first: its stand at the stop is what the rest keep clear of.
  const double bus_stay =
      T::kBusStayLeast + (T::kBusStayMost - T::kBusStayLeast) * deal(seed, "bus", 2);
  add(bus_route(m, bus_stay), VehicleKind::bus, Role::bus);
  // THE VISITORS, each to a bay of the car park of its own.
  const std::vector<RoadOut> roads = roads_of(m);
  std::vector<int32_t> lot;
  for (size_t k = 0; k < f.bays.size(); k++) {
    if (f.bays[k].kind == BayKind::lot) lot.push_back(int32_t(k));
  }
  const int n_visit =
      !lot.empty() && !roads.empty()
          ? T::kVisitorsLeast + int(std::floor(deal(seed, "visitors", 0) *
                                               (T::kVisitorsMost - T::kVisitorsLeast + 1)))
          : 0;
  std::unordered_set<int32_t> reserved;
  const auto any = [](double u, size_t n) { return size_t(std::floor(u * double(n))) % n; };
  for (int k = 0, tries = 0; k < n_visit && tries < n_visit * 6; tries++) {
    const int32_t bay = lot[any(deal(seed, "bay", tries), lot.size())];
    if (reserved.count(bay) != 0) continue;
    const VehicleKind kind = car_kind(deal(seed, "visitor-kind", tries), true);
    const RoadOut& r = roads[any(deal(seed, "visitor-in", tries), roads.size())];
    const RoadOut& o = roads[any(deal(seed, "visitor-out", tries), roads.size())];
    const double stay = T::kStayLeast + (T::kStayMost - T::kStayLeast) * deal(seed, "stay", tries);
    std::optional<Route> route = visitor_route(m, kind, f.bays[size_t(bay)], r.into, o.out, stay);
    if (!route) continue;
    reserved.insert(bay);
    add(std::move(route), kind, Role::visitor);
    k++;
  }
  // THE CARS THROUGH.
  for (int32_t k = 0; k < T::kThrough; k++) {
    add(through_route(m, k, loops), car_kind(deal(seed, "through-kind", k), false), Role::through);
  }
  // THE CARS ROUND THE LOOP, each way, and the cyclists.
  std::array<int32_t, 2> loop_route_at{-1, -1};
  std::array<int, 2> loop_cars{};
  for (size_t w = 0; w < 2; w++) {
    loop_cars[w] =
        T::kLoopLeast +
        int(std::floor(deal(seed, "loop", int32_t(w)) * (T::kLoopMost - T::kLoopLeast + 1)));
    if (loops[w].size() > 1) {
      routes.push_back(loop_route(m, loops[w], Keep::middle, drive_of(Driver::car), Role::loop));
      loop_route_at[w] = int32_t(routes.size()) - 1;
    }
  }
  for (int k = 0; k < std::max(loop_cars[0], loop_cars[1]); k++) {
    for (size_t w = 0; w < 2; w++) {
      if (loop_route_at[w] < 0 || k >= loop_cars[w]) continue;
      const std::string id = "loop-kind-" + std::to_string(w);
      wanted.push_back({car_kind(deal(seed, id, k), false), loop_route_at[w], Role::loop});
    }
  }
  const int n_bikes =
      T::kBikesLeast +
      int(std::floor(deal(seed, "bikes", 0) * (T::kBikesMost - T::kBikesLeast + 1)));
  std::array<int32_t, 2> bike_route_at{-1, -1};
  for (int k = 0; k < n_bikes; k++) {
    size_t w = any(deal(seed, "bike-way", k), 2);
    if (loops[w].size() <= 1) w = 0;
    if (loops[w].size() <= 1) continue;
    if (bike_route_at[w] < 0) {
      routes.push_back(loop_route(m, loops[w], Keep::edge, drive_of(Driver::bike), Role::bike));
      bike_route_at[w] = int32_t(routes.size()) - 1;
    }
    wanted.push_back({VehicleKind::bike, bike_route_at[w], Role::bike});
  }

  // THE PHASES: each vehicle at the first dealt phase whose period crosses no vehicle placed
  // before it.
  std::vector<Samples> samples;
  samples.reserve(routes.size());
  for (const Route& r : routes) samples.push_back(samples_of(r));
  std::unordered_map<int64_t, std::vector<int32_t>> grid;
  std::vector<Placed> placed;
  std::vector<Vehicle> vehicles;
  for (size_t k = 0; k < wanted.size(); k++) {
    const Wanted& w = wanted[k];
    const Samples& s = samples[size_t(w.route)];
    const int32_t n = s.n;
    // A cyclist, placed last round a loop already full of cars, is tried at more phases.
    const int32_t tries = w.kind == VehicleKind::bike ? T::kBikeTries : T::kTries;
    const auto offset = int32_t(std::floor(deal(seed, "phase", int32_t(k)) * tries));
    for (int32_t j = 0; j < tries; j++) {
      const int32_t shift = ((j + offset) % tries) * n / tries;
      bool clear = true;
      for (int32_t i = 0; i < S && clear; i++) {
        const RoutePose& q = s.at[size_t((i + shift) % n)];
        if (!q.shown) continue;
        const Box a = room_of(w.kind, q);
        const auto cx = int64_t(std::floor(a.x / kCell));
        const auto cz = int64_t(std::floor(a.z / kCell));
        for (int64_t ox = -1; ox <= 1 && clear; ox++) {
          for (int64_t oz = -1; oz <= 1 && clear; oz++) {
            const auto it = grid.find(cell_key(i, cx + ox, cz + oz));
            if (it == grid.end()) continue;
            for (const int32_t other : it->second) {
              const Placed& o = placed[size_t(other)];
              const RoutePose& oq = o.s->at[size_t((i + o.shift) % o.s->n)];
              // A car passing a cyclist on his own lane swings out round him: the one meeting
              // not held to.
              if ((o.kind == VehicleKind::bike) != (w.kind == VehicleKind::bike) &&
                  oq.lane == q.lane && q.lane >= 0) {
                continue;
              }
              if (overlap(a, room_of(o.kind, oq))) {
                clear = false;
                break;
              }
            }
          }
        }
      }
      // A cyclist is placed only where every car that will swing out round him (the cars are
      // all placed before the cyclists) has the room to: its room swung out clear of every
      // other vehicle.
      for (int32_t i = 0; i < S && clear && w.kind == VehicleKind::bike; i++) {
        const RoutePose& q = s.at[size_t((i + shift) % n)];
        if (!q.shown || q.lane < 0) continue;
        const auto cx = int64_t(std::floor(q.x / kCell));
        const auto cz = int64_t(std::floor(q.z / kCell));
        for (int64_t ox = -1; ox <= 1 && clear; ox++) {
          for (int64_t oz = -1; oz <= 1 && clear; oz++) {
            const auto it = grid.find(cell_key(i, cx + ox, cz + oz));
            if (it == grid.end()) continue;
            for (const int32_t car : it->second) {
              const Placed& c = placed[size_t(car)];
              const RoutePose& cq = c.s->at[size_t((i + c.shift) % c.s->n)];
              if (c.kind == VehicleKind::bike || cq.lane != q.lane) continue;
              const double fx = libm::sin(cq.facing);
              const double fz = libm::cos(cq.facing);
              const double half =
                  vehicle_size(c.kind).length / 2 + vehicle_size(VehicleKind::bike).length / 2;
              const double along = (q.x - cq.x) * fx + (q.z - cq.z) * fz - axle_to_middle(c.kind) +
                                   axle_to_middle(VehicleKind::bike);
              if (along > T::kPassAhead + half || along < -T::kPassBehind - half) continue;
              const double swing = pass_swing(c.kind, f.lanes[size_t(q.lane)].width);
              Box swung = room_of(c.kind, cq);
              swung.x += fz * swing;
              swung.z -= fx * swing;
              swung.hw = vehicle_size(c.kind).width / 2 + T::kPassSide;
              const auto sx = int64_t(std::floor(swung.x / kCell));
              const auto sz = int64_t(std::floor(swung.z / kCell));
              for (int64_t px = -1; px <= 1 && clear; px++) {
                for (int64_t pz = -1; pz <= 1 && clear; pz++) {
                  const auto jt = grid.find(cell_key(i, sx + px, sz + pz));
                  if (jt == grid.end()) continue;
                  for (const int32_t other : jt->second) {
                    const Placed& o = placed[size_t(other)];
                    if (other == car || o.kind == VehicleKind::bike) continue;
                    Box met = room_of(o.kind, o.s->at[size_t((i + o.shift) % o.s->n)]);
                    met.hw = vehicle_size(o.kind).width / 2 + T::kPassSide;
                    if (overlap(swung, met)) {
                      clear = false;
                      break;
                    }
                  }
                }
              }
              if (!clear) break;
            }
          }
        }
      }
      if (!clear) continue;
      const auto id = int32_t(placed.size());
      placed.push_back({w.kind, &s, shift});
      for (int32_t i = 0; i < S; i++) {
        const RoutePose& q = s.at[size_t((i + shift) % n)];
        if (!q.shown) continue;
        const Box a = room_of(w.kind, q);
        grid[cell_key(i, int64_t(std::floor(a.x / kCell)), int64_t(std::floor(a.z / kCell)))]
            .push_back(id);
      }
      vehicles.push_back({int32_t(vehicles.size()), w.kind, w.route, shift * T::kSample, w.role});
      break;
    }
  }

  // THE CARS PARKED: in the kerbs' bays and the car park's, those no visitor drives to and none a
  // moving vehicle's body passes through; the car park fuller in the lifts' hours.
  const double hour = level.sun.hour;
  const double lot_share =
      hour >= T::kDayFrom && hour < T::kDayTo ? T::kParkedDay : T::kParkedNight;
  const std::unordered_set<int32_t> blocked = swept_bays(f.bays, placed);
  std::vector<Parked> parked;
  for (size_t k = 0; k < f.bays.size(); k++) {
    const Bay& bay = f.bays[k];
    const auto id = int32_t(k);
    if (reserved.count(id) != 0 || blocked.count(id) != 0) continue;
    const double share = bay.kind == BayKind::kerb ? T::kParkedKerb : lot_share;
    const std::string name = "park-" + bay.id;
    if (deal(seed, name, 0) >= share) continue;
    parked.push_back({int32_t(parked.size()),
                      car_kind(deal(seed, name, 1), bay.kind == BayKind::lot), id, bay.x, bay.y,
                      bay.z, bay.heading});
  }

  double x0 = kInf;
  double z0 = kInf;
  double x1 = -kInf;
  double z1 = -kInf;
  for (const Street& st : streets.streets) {
    for (const StreetPoint& p : st.points) {
      x0 = num::min(x0, p.x);
      z0 = num::min(z0, p.z);
      x1 = num::max(x1, p.x);
      z1 = num::max(z1, p.z);
    }
  }
  return TrafficPlan{S * T::kSample,
                     std::move(f),
                     std::move(routes),
                     std::move(vehicles),
                     std::move(parked),
                     x0 - 15,
                     z0 - 15,
                     x1 + 15,
                     z1 + 15};
}

bool near_traffic(const TrafficPlan& plan, double x, double z, double pad) {
  return x >= plan.x0 - pad && x <= plan.x1 + pad && z >= plan.z0 - pad && z <= plan.z1 + pad;
}

uint64_t traffic_hash(const TrafficPlan& plan, uint64_t h) {
  const auto mix = [&h](double v) {
    uint64_t u = 0;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(plan.period);
  for (const Route& r : plan.routes) {
    mix(double(r.role));
    mix(r.period);
    mix(r.roll);
    for (const Leg& l : r.legs) {
      mix(double(l.kind));
      mix(l.start);
      mix(l.duration);
      mix(l.odo);
    }
    for (const Driven& d : r.driven) {
      for (int32_t i = 0; i < d.path.n; i++) {
        mix(d.path.x[size_t(i)]);
        mix(d.path.z[size_t(i)]);
        mix(d.v[size_t(i)]);
      }
    }
  }
  for (const Vehicle& v : plan.vehicles) {
    mix(double(v.kind));
    mix(v.route);
    mix(v.phase);
  }
  for (const Parked& p : plan.parked) {
    mix(double(p.kind));
    mix(p.bay);
    mix(p.x);
    mix(p.z);
    mix(p.heading);
  }
  return h;
}

}  // namespace ge::mountain::village

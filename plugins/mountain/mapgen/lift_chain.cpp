// The way off the gondola's top: the chair's bottom by an aim, the earthworks, pressed and groomed.
//
// ge:summary The chair's bottom placed by an aim, the way's earthworks, the way pressed and groomed
// ge:keywords lift-chain chain-bottom chain-way way-target way-levels rect-off earthworks press-way
// ge:keywords press-box approached
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/lift_chain.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/lift_line.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/station_clear.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace C = resort_rules::lift::chain;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

struct Ends {
  LiftKind kind;
  PlanPoint bottom;
  PlanPoint top;
};

// The lift that tops out at the mid-station and the one that leaves it.
constexpr const char* kUpper = "G1";
constexpr const char* kLower = "C1";

// How steeply a ramp's flank falls off its edge where it crosses the leg, m per m.
constexpr double kRampFlank = 0.5;

// The step the way is read at, m, and its ground for its earthworks, m.
constexpr double kRead = 1;
constexpr double kDigRead = 2;

const LiftPlan* find_lift(std::span<const LiftPlan> lifts, const char* id) {
  for (const LiftPlan& l : lifts) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

ChainWay chain_way(const Ends& upper, const Ends& lower) {
  const LetGo off = let_go_of(upper.kind, upper.bottom, upper.top);
  const LinePoint f = ring_frame(lower.kind);
  const double len =
      num::max(1.0, hypot(lower.top.x - lower.bottom.x, lower.top.z - lower.bottom.z));
  const double dx = (lower.top.x - lower.bottom.x) / len;
  const double dz = (lower.top.z - lower.bottom.z) / len;
  return {{off.x, off.z},
          {lower.bottom.x + dx * f.u + dz * f.v, lower.bottom.z + dz * f.u - dx * f.v},
          lower.bottom};
}

struct SegHit {
  double k;  // the share of the way along the segment nearest the point
  double d;  // the plan distance to it, m
};

SegHit seg_at(PlanPoint a, PlanPoint b, double x, double z) {
  const double ex = b.x - a.x;
  const double ez = b.z - a.z;
  const double e2 = ex * ex + ez * ez;
  const double k =
      num::max(0.0, num::min(1.0, ((x - a.x) * ex + (z - a.z) * ez) / (e2 != 0 ? e2 : 1)));
  return {k, hypot(x - (a.x + ex * k), z - (a.z + ez * k))};
}

double seg(PlanPoint a, PlanPoint b, double x, double z) { return seg_at(a, b, x, z).d; }

// How far (x, z) stands outside a bottom station's footprint, m.
double rect_off(const Ends& l, double x, double z) {
  const auto& F = footprint_of(l.kind);
  const double len = num::max(1.0, hypot(l.top.x - l.bottom.x, l.top.z - l.bottom.z));
  const double dx = (l.top.x - l.bottom.x) / len;
  const double dz = (l.top.z - l.bottom.z) / len;
  const double u = (x - l.bottom.x) * dx + (z - l.bottom.z) * dz;
  const double v = (x - l.bottom.x) * dz - (z - l.bottom.z) * dx;
  return hypot(num::max(0.0, -F.back - u, u - F.ahead), num::max(0.0, std::abs(v) - F.half));
}

struct Target {
  double y;
  double out;  // how far outside the way's core the point stands, m (0 inside it)
  bool cut;    // whether it is only ever cut to it
};

// What the way's ground is graded to at (x, z).
Target way_target(const ChainWay& way, const Ends& lower, double y0, double y_ring,
                  double y_station, double x, double z) {
  const SegHit first = seg_at(way.from, way.ring, x, z);
  const SegHit corral = seg_at(way.ring, way.station, x, z);
  const double station = rect_off(lower, x, z);
  const double leg = num::max(0.0, first.d - C::half);
  const double queue = num::max(0.0, corral.d - C::half);
  if (station <= num::min(leg, queue)) return {y_station, station, false};
  if (leg <= queue) return {y0 + (y_ring - y0) * first.k, leg, true};
  return {y_ring + (y_station - y_ring) * smoothstep(0, 1, corral.k), queue, false};
}

struct Levels {
  double y0, y_ring, y_station;
};

// The heights the way is graded to: where he is let go, the ring, and the station.
Levels way_levels(const ge::core::Heightfield& ground, const ChainWay& way, const Ends& lower) {
  const double y0 = sample_field(ground, way.from.x, way.from.z);
  const double reach = num::max(1.0, hypot(way.ring.x - way.from.x, way.ring.z - way.from.z));
  const double y_ring =
      num::min(sample_field(ground, way.ring.x, way.ring.z), y0 - C::grade * reach);
  const std::vector<PlanPoint> pts = bottom_footprint(lower.kind, lower.bottom, lower.top);
  double sum = 0;
  for (const PlanPoint& p : pts) sum += sample_field(ground, p.x, p.z);
  const double corral = hypot(way.station.x - way.ring.x, way.station.z - way.ring.z);
  const double y_station =
      num::max(y_ring, num::min(y_ring + C::climb * corral, sum / double(pts.size())));
  return {y0, y_ring, y_station};
}

// Points every `kDigRead` m along a segment.
void along_seg(PlanPoint a, PlanPoint b, std::vector<PlanPoint>& out) {
  const double n = num::max(1.0, std::ceil(hypot(b.x - a.x, b.z - a.z) / kDigRead));
  for (double i = 0; i <= n; i++) {  // NOLINT(clang-analyzer-security.FloatLoopCounter): whole
    out.push_back({a.x + ((b.x - a.x) * i) / n, a.z + ((b.z - a.z) * i) / n});
  }
}

struct Works {
  double fill, cut, dig;
};

// The earthworks the way asks: the most its core is filled or cut and the most the station's
// footprint is cut into the mountain, m.
Works earthworks(const ge::core::Heightfield& ground, const ChainWay& way, const Ends& lower) {
  const Levels lv = way_levels(ground, way, lower);
  Works out{0, 0, 0};
  const auto read = [&](PlanPoint p, bool footprint) {
    const Target t = way_target(way, lower, lv.y0, lv.y_ring, lv.y_station, p.x, p.z);
    const double d = sample_field(ground, p.x, p.z) - t.y;
    if (!t.cut) out.fill = num::max(out.fill, -d);
    if (footprint) {
      out.dig = num::max(out.dig, d);
    } else {
      out.cut = num::max(out.cut, d);
    }
  };
  std::vector<PlanPoint> pts;
  along_seg(way.from, way.ring, pts);
  along_seg(way.ring, way.station, pts);
  for (const PlanPoint& p : pts) read(p, false);
  for (const PlanPoint& p : bottom_footprint(lower.kind, lower.bottom, lower.top)) read(p, true);
  return out;
}

// Every grid cell within `far` m of the box round `corners`, its height handed to `to` and
// replaced by what it returns.
template <class Fn>
void press_box(ge::core::Heightfield& ground, std::span<const PlanPoint> corners, double far,
               Fn&& to) {
  double x0 = corners[0].x;
  double x1 = x0;
  double z0 = corners[0].z;
  double z1 = z0;
  for (const PlanPoint& p : corners) {
    x0 = num::min(x0, p.x);
    x1 = num::max(x1, p.x);
    z0 = num::min(z0, p.z);
    z1 = num::max(z1, p.z);
  }
  const double cell = ground.cell;
  const int c0 = static_cast<int>(num::max(0.0, std::floor((x0 - far - ground.origin_x) / cell)));
  const int c1 = static_cast<int>(
      num::min(double(ground.cols - 1), std::ceil((x1 + far - ground.origin_x) / cell)));
  const int r0 = static_cast<int>(num::max(0.0, std::floor((z0 - far - ground.origin_z) / cell)));
  const int r1 = static_cast<int>(
      num::min(double(ground.rows - 1), std::ceil((z1 + far - ground.origin_z) / cell)));
  for (int row = r0; row <= r1; row++) {
    for (int col = c0; col <= c1; col++) {
      const size_t i = static_cast<size_t>(row) * static_cast<size_t>(ground.cols) + size_t(col);
      ground.data[i] = static_cast<float>(
          to(ground.origin_x + col * cell, ground.origin_z + row * cell, double(ground.data[i])));
    }
  }
}

// Whether (x, z) is under the upper lift's way in, where its cabins come down onto the deck:
// ground there is only ever cut.
bool approached(const Ends& upper, double x, double z) {
  namespace A = resort_rules::lift::top::approach;
  const double len =
      num::max(1.0, hypot(upper.top.x - upper.bottom.x, upper.top.z - upper.bottom.z));
  const double ux = (upper.top.x - upper.bottom.x) / len;
  const double uz = (upper.top.z - upper.bottom.z) / len;
  const double back = (upper.top.x - x) * ux + (upper.top.z - z) * uz;
  const double v = std::abs((x - upper.top.x) * uz - (z - upper.top.z) * ux);
  return back > 0 && back < A::length && v < A::half.gondola + A::blend;
}

// R26 — THE WAY GRADED into the mountain, eased in over `blend` m.
void press_way(ge::core::Heightfield& ground, const ChainWay& way, const Ends& lower,
               const Ends& upper, double blend) {
  const Levels lv = way_levels(ground, way, lower);
  std::vector<PlanPoint> corners = bottom_footprint(lower.kind, lower.bottom, lower.top);
  corners.push_back(way.from);
  corners.push_back(way.ring);
  press_box(ground, corners, C::half + blend, [&](double x, double z, double y) {
    const Target t = way_target(way, lower, lv.y0, lv.y_ring, lv.y_station, x, z);
    if (t.out >= blend) return y;
    const double to = y + (t.y - y) * (1 - smoothstep(0, blend, t.out));
    return t.cut || approached(upper, x, z) ? num::min(y, to) : to;
  });
}

}  // namespace

PlanPoint chain_bottom(LiftKind upper_kind, PlanPoint upper_bottom, PlanPoint upper_top,
                       LiftKind kind, PlanPoint top, std::array<double, 2> aim) {
  const LetGo off = let_go_of(upper_kind, upper_bottom, upper_top);
  const double h = off.heading + aim[0];
  const double reach = aim[1];
  // The ring: ahead and to the right of the rider.
  const PlanPoint ring{off.x + libm::sin(h) * reach, off.z + libm::cos(h) * reach};
  const LinePoint f = ring_frame(kind);
  // The line's direction moves a little with the bottom, so read it again until it settles.
  PlanPoint b = ring;
  for (int k = 0; k < 4; k++) {
    const double len = num::max(1.0, hypot(top.x - b.x, top.z - b.z));
    const double dx = (top.x - b.x) / len;
    const double dz = (top.z - b.z) / len;
    b = {ring.x - dx * f.u - dz * f.v, ring.z - dz * f.u + dx * f.v};
  }
  return b;
}

bool way_falls(const ChainWay& way, const HeightAt& height) {
  const double len = hypot(way.ring.x - way.from.x, way.ring.z - way.from.z);
  const double n = num::max(1.0, std::ceil(len / kRead));
  const double y0 = height(way.from.x, way.from.z);
  double low = y0;
  for (double i = 1; i <= n; i++) {  // NOLINT(clang-analyzer-security.FloatLoopCounter): whole
    const double t = i / n;
    const double y = height(way.from.x + (way.ring.x - way.from.x) * t,
                            way.from.z + (way.ring.z - way.from.z) * t);
    if (y - low > C::rise) return false;
    low = num::min(low, y);
  }
  return y0 - height(way.ring.x, way.ring.z) >= C::fall * len;
}

double off_way(const ChainWay& way, double x, double z) {
  return num::min(seg(way.from, way.ring, x, z), seg(way.ring, way.station, x, z)) - C::half;
}

double off_queue(const ChainWay& way, double x, double z) {
  const double k = num::min(
      1.0, C::share / num::max(1.0, hypot(way.ring.x - way.from.x, way.ring.z - way.from.z)));
  const PlanPoint from{way.from.x + (way.ring.x - way.from.x) * k,
                       way.from.z + (way.ring.z - way.from.z) * k};
  return num::min(seg(from, way.ring, x, z) - C::half + resort_rules::lift::top::ramp::width / 2,
                  seg(way.ring, way.station, x, z) - C::half);
}

std::optional<LaidChain> lay_chain(ge::core::Heightfield& ground, std::span<const float> raw,
                                   std::vector<LiftPlan>& lifts, std::vector<StationPad> pads,
                                   const PadShape& shape) {
  const LiftPlan* up = find_lift(lifts, kUpper);
  const LiftPlan* lowp = find_lift(lifts, kLower);
  if (!up || !lowp) return std::nullopt;
  const Ends upper{up->kind, up->bottom, up->top};
  const LiftKind lower_kind = lowp->kind;
  const PlanPoint lower_top = lowp->top;
  struct Tried {
    PlanPoint bottom;
    Ends ends;
    ChainWay way;
    Works works;
  };
  // The way to the queue with the chair's bottom at `bottom`, or none where it stands on a pad
  // or asks more earthworks than `lift.chain` allows.
  const auto tried = [&](PlanPoint bottom) -> std::optional<Tried> {
    const Ends ends{lower_kind, bottom, lower_top};
    for (const PlanPoint& p : bottom_footprint(lower_kind, bottom, lower_top)) {
      for (const StationPad& q : pads) {
        if (!(hypot(p.x - q.x, p.z - q.z) > q.r)) return std::nullopt;
      }
    }
    const ChainWay way = chain_way(upper, ends);
    const Works works = earthworks(ground, way, ends);
    if (works.fill > C::fill || works.cut > C::dig || works.dig > C::dig) return std::nullopt;
    return Tried{bottom, ends, way, works};
  };
  std::optional<Tried> found;
  bool moved = false;
  for (size_t i = 0; i < C::aims.size() && !found; i++) {
    found = tried(i == 0 ? lowp->bottom
                         : chain_bottom(upper.kind, upper.bottom, upper.top, lower_kind, lower_top,
                                        C::aims[i]));
    moved = i > 0;
  }
  if (!found) return std::nullopt;
  if (moved) {
    // Pressed again with the chair where it now stands.
    for (LiftPlan& l : lifts) {
      if (l.id == kLower) l.bottom = found->bottom;
    }
    std::copy(raw.begin(), raw.end(), ground.data.begin());
    pads = press_pads(ground, lifts, shape);
  }
  press_way(ground, found->way, found->ends, upper, num::max(C::blend, C::bank * found->works.dig));
  return LaidChain{found->way, std::move(pads)};
}

bool keeps_chain(std::span<const LiftPlan> lifts, const HeightAt& height, bool snow) {
  const std::optional<ChainWay> way = chain_of(lifts);
  if (!way) return true;
  const LiftPlan& upper = *find_lift(lifts, kUpper);
  const LetGo off = let_go_of(upper.kind, upper.bottom, upper.top);
  const double dx = way->ring.x - way->from.x;
  const double dz = way->ring.z - way->from.z;
  const double reach = hypot(dx, dz);
  // To his right: the side the corral leaves its line on.
  const double bearing = ge::core::angle_diff(off.heading, libm::atan2(dx, dz));
  return within_band(bearing, C::bearing) && within_band(reach, C::reach) &&
         (!snow || way_falls(*way, height));
}

std::optional<ChainWay> chain_of(std::span<const LiftPlan> lifts) {
  const LiftPlan* upper = find_lift(lifts, kUpper);
  const LiftPlan* lower = find_lift(lifts, kLower);
  if (!upper || !lower) return std::nullopt;
  return chain_way({upper->kind, upper->bottom, upper->top},
                   {lower->kind, lower->bottom, lower->top});
}

void groom_way(const ChainWay& way, ge::core::Heightfield& packed, const OnRun& on_run) {
  const double half = C::half;
  const double cell = packed.cell;
  const double far = 2 * half;
  const double x0 = num::min(way.from.x, way.ring.x, way.station.x);
  const double x1 = num::max(way.from.x, way.ring.x, way.station.x);
  const double z0 = num::min(way.from.z, way.ring.z, way.station.z);
  const double z1 = num::max(way.from.z, way.ring.z, way.station.z);
  const int c0 = static_cast<int>(num::max(0.0, std::floor((x0 - far - packed.origin_x) / cell)));
  const int c1 = static_cast<int>(
      num::min(double(packed.cols - 1), std::ceil((x1 + far - packed.origin_x) / cell)));
  const int r0 = static_cast<int>(num::max(0.0, std::floor((z0 - far - packed.origin_z) / cell)));
  const int r1 = static_cast<int>(
      num::min(double(packed.rows - 1), std::ceil((z1 + far - packed.origin_z) / cell)));
  for (int row = r0; row <= r1; row++) {
    for (int col = c0; col <= c1; col++) {
      const double x = packed.origin_x + col * cell;
      const double z = packed.origin_z + row * cell;
      const double off = off_way(way, x, z);
      if (off >= half) continue;
      const double groom = off <= 0 ? 1 : 1 - off / half;
      const size_t i = static_cast<size_t>(row) * static_cast<size_t>(packed.cols) + size_t(col);
      if (groom > packed.data[i] && !on_run(x, z)) packed.data[i] = static_cast<float>(groom);
    }
  }
}

void regrade_leg(ge::core::Heightfield& ground, std::span<const LiftPlan> lifts,
                 std::span<const StationPad> pads, const RampsByLift& ramps) {
  const std::optional<ChainWay> way = chain_of(lifts);
  if (!way) return;
  const StationPad* pad = nullptr;
  for (const StationPad& p : pads) {
    if (p.lift == kUpper) pad = &p;
  }
  // Where the leg leaves a circle of radius `r` about the pad.
  const auto leaves = [&](double r) -> PlanPoint {
    const double ex = way->ring.x - way->from.x;
    const double ez = way->ring.z - way->from.z;
    const double fx = way->from.x - pad->x;
    const double fz = way->from.z - pad->z;
    const double e2 = ex * ex + ez * ez;
    const double a = e2 != 0 ? e2 : 1;
    const double b = fx * ex + fz * ez;
    const double c = fx * fx + fz * fz - r * r;
    if (c >= 0) return way->from;
    const double t = num::min(1.0, (-b + std::sqrt(b * b - a * c)) / a);
    return {way->from.x + ex * t, way->from.z + ez * t};
  };
  // Off the pad's own edge: its height read just inside it, its cells left as they were pressed.
  const double keep = pad ? pad->r : 0;
  const PlanPoint from = pad ? leaves(keep) : way->from;
  const PlanPoint edge = pad ? leaves(pad->r - ground.cell) : way->from;
  const double y0 = sample_field(ground, edge.x, edge.z);
  const double reach = hypot(way->ring.x - from.x, way->ring.z - from.z);
  const double y_ring = sample_field(ground, way->ring.x, way->ring.z);
  // A ramp off the deck crossing the leg: its own even fall and its flanks falling off it.
  const auto off = ramps.find(kUpper);
  const auto ramp_floor = [&](double x, double z) {
    double floor = -std::numeric_limits<double>::infinity();
    if (off == ramps.end()) return floor;
    for (const SummitRamp& r : off->second) {
      const SegHit at = seg_at({r.from.x, r.from.z}, {r.to.x, r.to.z}, x, z);
      const double side = at.d - r.width / 2;
      if (side >= resort_rules::lift::top::ramp::blend) continue;
      floor =
          num::max(floor, r.from.y + (r.to.y - r.from.y) * at.k - num::max(0.0, side) * kRampFlank);
    }
    return floor;
  };
  const std::array<PlanPoint, 2> corners{from, way->ring};
  press_box(ground, corners, C::half + C::blend, [&](double x, double z, double y) {
    if (pad && hypot(x - pad->x, z - pad->z) < keep) return y;
    const SegHit at = seg_at(from, way->ring, x, z);
    const double out = num::max(0.0, at.d - C::half);
    if (out >= C::blend) return y;
    const double w = 1 - smoothstep(0, C::blend, out);
    const double line = y0 + (y_ring - y0) * at.k;
    const double to = y + (line - y) * w;
    const double floor = ramp_floor(x, z);
    if (to < y) return num::max(to, num::min(y, floor));
    // Filled only off the pad's edge, fading over `lift.chain.lip` m, so the way leaves the pad
    // without a step down onto it, and never on a ramp, left falling as it was pressed.
    if (floor > -std::numeric_limits<double>::infinity()) return y;
    return y + (to - y) * (1 - smoothstep(0, C::lip, at.k * reach));
  });
}

std::vector<RampRoom> chain_rooms(const std::optional<ChainWay>& way) {
  if (!way) return {};
  const double half = C::half;
  return {{way->from, way->ring, half}, {way->ring, way->station, half}};
}

}  // namespace ge::mountain::mapgen

// A lift station's footprint, and the stations slid clear of the runs once they stand.
//
// ge:summary A station's footprint read as points; the stations slid clear, the drags re-laid
// ge:keywords station-clear footprint bottom-footprint clear-stations clear-bottom clear-drag
// ge:keywords candidates slides
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/station_clear.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/parallel.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
using ge::core::hypot;

// The step a footprint and a drag's line are read at, m.
constexpr double kRead = 3;
// How far off a run's surface a station keeps, m; how near a link lane's end its lift's bottom
// station stays, m.
constexpr double kMargin = 2;
constexpr double kLink = 18;
// The least a drag climbs, m, and how far inside its steepest pull a drag re-laid keeps.
constexpr double kRise = 30;
constexpr double kPitchMargin = 0.9;
// The slides tried across a chair's or a gondola's line at its bottom station, m, nearest first.
constexpr std::array<double, 19> kSlides{0,   6,  -6,  12, -12, 18, -18, 26, -26, 34,
                                         -34, 44, -44, 56, -56, 70, -70, 86, -86};
// A DRAG RE-LAID once its runs stand: its top moved this far round where it was, m, on as many
// bearings; its foot slid this far either way across, m, every `step`, from as far up its line.
constexpr std::array<double, 9> kDragTopReach{0, 15, 30, 45, 60, 75, 95, 120, 150};
constexpr int kDragTopBearings = 12;
constexpr int kDragFootReach = 180;
constexpr int kDragFootStep = 10;
constexpr std::array<double, 5> kDragFootUp{0, 0.25, 0.45, 0.6, 0.72};
// How many places a lift's end is read at side by side, at most.
constexpr size_t kBatch = 256;
// The nursery's drag, laid by the plan rather than to R29's bands.
constexpr const char* kNursery = "D1";

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

// Whether a bottom station at `bottom` on a line to `top` stands clear of the runs, on its own
// level ground (its level first: reads of the ground, where the runs are a search).
bool clear_bottom(const StationGround& g, LiftKind kind, PlanPoint bottom, PlanPoint top) {
  if (!stands_level(g.height, kind, bottom, top, g.level.value_or(kStationLevel))) return false;
  for (const PlanPoint& p : bottom_footprint(kind, bottom, top)) {
    if (g.on_run(p.x, p.z, kMargin)) return false;
  }
  return true;
}

// Whether a drag's line crosses no piste, climbs, keeps to a drag's length and pull where it is
// `banded` (R26), and its top's footprint stands clear of every run.
bool clear_drag(const StationGround& g, PlanPoint bottom, PlanPoint top, bool banded) {
  namespace D = resort_rules::lift::drag;
  const double h = hypot(top.x - bottom.x, top.z - bottom.z);
  const double len = h != 0 ? h : 1;
  if (banded && (len < D::length.min || len > D::length.max)) return false;
  // Never steeper than a rope pulls a skier on his skis: a few reads of the ground first.
  const auto at = [&](double u) {
    return g.height(bottom.x + ((top.x - bottom.x) * u) / len,
                    bottom.z + ((top.z - bottom.z) * u) / len);
  };
  if (at(len) - at(0) < kRise) return false;
  for (int k = 0; banded && k * kRead + D::pitch_window <= len; k++) {
    const double u = k * kRead;
    if ((at(u + D::pitch_window) - at(u)) / D::pitch_window > D::pitch * kPitchMargin) {
      return false;
    }
  }
  for (int k = 0; k * kRead <= len; k++) {
    const double t = (k * kRead) / len;
    if (g.on_piste(bottom.x + (top.x - bottom.x) * t, bottom.z + (top.z - bottom.z) * t, kMargin)) {
      return false;
    }
  }
  const PlanPoint down{(bottom.x - top.x) / len, (bottom.z - top.z) / len};
  for (const PlanPoint& p : footprint(LiftKind::drag, top, down)) {
    if (g.on_run(p.x, p.z, kMargin)) return false;
  }
  return true;
}

struct Place {
  PlanPoint bottom;
  PlanPoint top;
  double cost;
};

// Where a lift's ends may go, the nearest first: a chair's or a gondola's bottom station slid
// across its line; a drag re-laid, its top round where it was and its foot along or up its line.
std::vector<Place> candidates(const LiftPlan& l) {
  const double h = hypot(l.top.x - l.bottom.x, l.top.z - l.bottom.z);
  const double len = h != 0 ? h : 1;
  const PlanPoint right{(l.top.z - l.bottom.z) / len, -(l.top.x - l.bottom.x) / len};
  std::vector<Place> out;
  if (l.kind != LiftKind::drag) {
    out.reserve(kSlides.size());
    for (const double sb : kSlides) {
      out.push_back({{l.bottom.x + right.x * sb, l.bottom.z + right.z * sb}, l.top, std::abs(sb)});
    }
    return out;
  }
  for (const double r : kDragTopReach) {
    for (int k = 0; k < (r == 0 ? 1 : kDragTopBearings); k++) {
      const double a = ((double(k) / kDragTopBearings) * ge::core::kPi) * 2;
      const PlanPoint top{l.top.x + libm::sin(a) * r, l.top.z + libm::cos(a) * r};
      for (const double up : kDragFootUp) {
        // Its foot up its line: a shorter drag beside the run it serves.
        const double fx = l.bottom.x + (l.top.x - l.bottom.x) * up;
        const double fz = l.bottom.z + (l.top.z - l.bottom.z) * up;
        for (int sb = -kDragFootReach; sb <= kDragFootReach; sb += kDragFootStep) {
          const PlanPoint bottom{fx + right.x * sb, fz + right.z * sb};
          out.push_back({bottom, top, std::abs(double(sb)) + 2 * r + up * len});
        }
      }
    }
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Place& a, const Place& b) { return a.cost < b.cost; });
  return out;
}

}  // namespace

bool stands_level(const std::function<double(double x, double z)>& height, LiftKind kind,
                  PlanPoint bottom, PlanPoint top, double most) {
  double lo = std::numeric_limits<double>::infinity();
  double hi = -lo;
  for (const PlanPoint& p : bottom_footprint(kind, bottom, top)) {
    const double y = height(p.x, p.z);
    lo = num::min(lo, y);
    hi = num::max(hi, y);
  }
  const auto& F = footprint_of(kind);
  return (hi - lo) / num::max(F.back + F.ahead, 2 * F.half) <= most;
}

std::optional<std::string> clear_stations(
    const StationGround& g, std::vector<LiftPlan>& lifts,
    const std::function<bool(std::span<const LiftPlan> lifts)>& ok) {
  for (LiftPlan& l : lifts) {
    const std::vector<PlanPoint> ends = g.link_ends(l.id);
    const bool was_floor = g.floor(l.bottom);
    const PlanPoint was_bottom = l.bottom;
    const PlanPoint was_top = l.top;
    const bool drag = l.kind == LiftKind::drag;
    const auto fits = [&](PlanPoint bottom, PlanPoint top) {
      if (!drag && g.floor(bottom) != was_floor) return false;
      for (const PlanPoint& e : ends) {
        if (hypot(e.x - bottom.x, e.z - bottom.z) >
            num::max(kLink, hypot(e.x - was_bottom.x, e.z - was_bottom.z))) {
          return false;
        }
      }
      return clear_bottom(g, l.kind, bottom, top) &&
             (!drag || clear_drag(g, bottom, top, l.id != kNursery));
    };
    if (fits(was_bottom, was_top)) continue;
    bool placed = false;
    // Whether a place fits is a pure reading of the ground and the runs, so a batch of places is
    // read side by side; they are then tried in order, the first that fits and leaves `ok` true
    // taken, as if read one by one.
    const std::vector<Place> places = candidates(l);
    std::vector<uint8_t> fit(places.size(), 0);
    for (size_t b0 = 0; b0 < places.size() && !placed; b0 += kBatch) {
      const size_t b1 = std::min(places.size(), b0 + kBatch);
      ge::core::parallel_bands(b1 - b0, [&](size_t lo, size_t hi) {
        for (size_t q = b0 + lo; q < b0 + hi; q++) {
          fit[q] = fits(places[q].bottom, places[q].top) ? 1 : 0;
        }
      });
      for (size_t q = b0; q < b1; q++) {
        if (!fit[q]) continue;
        l.bottom = places[q].bottom;
        l.top = places[q].top;
        if (ok(lifts)) {
          placed = true;
          break;
        }
      }
    }
    if (!placed) {
      l.bottom = was_bottom;
      l.top = was_top;
      return l.id;
    }
  }
  return std::nullopt;
}

std::vector<PlanPoint> bottom_footprint(LiftKind kind, PlanPoint bottom, PlanPoint top) {
  const double h = ge::core::hypot(top.x - bottom.x, top.z - bottom.z);
  const double len = h != 0 ? h : 1;
  return footprint(kind, bottom, {(top.x - bottom.x) / len, (top.z - bottom.z) / len});
}

}  // namespace ge::mountain::mapgen

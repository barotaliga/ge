// Drag lifts: placing one beside a foot, whether its line fits, and keeping only those needed.
//
// ge:summary Drag lifts placed beside a foot, fitted to the ground, kept where access turns
// ge:keywords drags plan-drag fits pad-grade drag-back settle-drags point-at
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/drags.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace RR = resort_rules;
namespace D = RR::lift::drag;
using ge::core::hypot;

namespace {

// The step a drag lift's line is read along, and how far from either end it is read for the
// runs it would run up, m.
constexpr double kRead = 5;
constexpr double kEnds = 40;
// The least a drag lift climbs, m, and how far inside its steepest it is laid.
constexpr double kRise = 30;
constexpr double kPitchMargin = 0.9;
// How far up a run from its foot the next foot is tried, m, and how many; how many targets each
// is tried toward; how far apart along a lane its points are offered as targets, m.
constexpr double kFootStep = 180;
constexpr int kFeet = 5;
constexpr size_t kTargets = 4;
constexpr double kLaneStep = 120;
// How far out from the foot's run the bottom station is tried, as multiples of its first place
// off the run's edge; and how far either way of square across the line its top is tried, m.
constexpr double kOut[] = {1, 1.8, 2.8, 4};
constexpr double kAside[] = {0, 30, -30, 55, -55};

// Whether a drag lift's line from `a` up to `b` climbs, never steeper than a surface lift pulls,
// runs up no piste, and tops out on the level.
bool fits(const DragGround& g, PlanPoint a, PlanPoint b) {
  const double length = hypot(b.x - a.x, b.z - a.z);
  const int n = static_cast<int>(num::max(2.0, std::ceil(length / kRead)));
  std::vector<double> ys;
  for (int i = 0; i <= n; i++) {
    const double t = double(i) / n;
    const double x = a.x + (b.x - a.x) * t;
    const double z = a.z + (b.z - a.z) * t;
    ys.push_back(g.height(x, z));
    const double u = t * length;
    if (u < kEnds || u > length - kEnds) continue;
    const PisteNear p = g.piste(x, z);
    if (p.distance > p.width / 2 + D::room) continue;
    // A T-bar's track is ridden on the snow: it crosses no piste (R26).
    return false;
  }
  if (ys[size_t(n)] - ys[0] < kRise) return false;
  const int k = static_cast<int>(num::max(1.0, num::round(D::pitch_window / (length / n))));
  for (int i = 0; i + k <= n; i++) {
    if ((ys[size_t(i) + size_t(k)] - ys[size_t(i)]) / (k * (length / n)) > D::pitch * kPitchMargin)
      return false;
  }
  return pad_grade(g.height, b) <= D::pad_grade;
}

// The station of a line nearest an arc.
const TrackPoint& point_at(const std::vector<TrackPoint>& points, double s) {
  const TrackPoint* best = &points[0];
  for (const TrackPoint& q : points) {
    if (std::abs(q.s - s) < std::abs(best->s - s)) best = &q;
  }
  return *best;
}

}  // namespace

std::optional<LiftPlan> plan_drag(const DragGround& g, const Foot& foot, PlanPoint station,
                                  const std::string& id) {
  for (const double out : kOut) {
    for (const double sgn : {1.0, -1.0}) {
      // Beside the foot, off the run's edge.
      const double off = (foot.width / 2 + D::room + 6) * out * sgn;
      const PlanPoint bottom{foot.x + libm::cos(foot.heading) * off,
                             foot.z - libm::sin(foot.heading) * off};
      for (const double aside : kAside) {
        const double dx = station.x - bottom.x;
        const double dz = station.z - bottom.z;
        const double d = hypot(dx, dz);
        // Short of the station along the line, and aside of it across.
        const PlanPoint top{station.x - (dx / d) * D::beside + (dz / d) * aside,
                            station.z - (dz / d) * D::beside - (dx / d) * aside};
        const double reach = hypot(top.x - bottom.x, top.z - bottom.z);
        if (!within_band(reach, D::length)) continue;
        if (fits(g, bottom, top)) return LiftPlan{id, LiftKind::drag, bottom, top};
      }
    }
  }
  return std::nullopt;
}

double pad_grade(const std::function<double(double, double)>& height, PlanPoint p) {
  const double h = RR::lift::pad / 2;
  return num::max(std::abs(height(p.x + h, p.z) - height(p.x - h, p.z)),
                  std::abs(height(p.x, p.z + h) - height(p.x, p.z - h))) /
         (2 * h);
}

std::vector<PlanLift> plan_lifts(const DragGround& g, std::span<const LiftPlan> lifts) {
  std::vector<PlanLift> out;
  out.reserve(lifts.size());
  for (const LiftPlan& l : lifts) out.push_back({l.id, l.bottom, l.top, g.floor(l.bottom)});
  return out;
}

bool drag_back(const DragGround& g, std::span<const PlanRun> runs, std::vector<LiftPlan>& lifts,
               size_t i, DragWhy why) {
  const PlanRun& p = runs[i];
  // The runs a skier on it goes on down, no harder than it.
  const PlanRun* foot = &p;
  for (size_t k = 0; why == DragWhy::back && foot->into &&
                     runs[size_t(foot->into->run)].rank <= p.rank && k < runs.size();
       k++) {
    foot = &runs[size_t(foot->into->run)];
  }
  int own = -1;
  for (size_t l = 0; l < lifts.size(); l++) {
    if (lifts[l].id == p.from) {
      own = static_cast<int>(l);
      break;
    }
  }
  const auto drags = std::count_if(lifts.begin(), lifts.end(),
                                   [](const LiftPlan& l) { return l.kind == LiftKind::drag; });
  const std::string id = "D" + std::to_string(drags + 1);
  for (int f = 0; f < kFeet; f++) {
    // Off a run that cannot be skied on, a little up from where it begins closing on the harder
    // one; off one that ends, or has joined the run it merges into, its end — and on up it.
    const double at =
        (why == DragWhy::back && foot->into ? foot->merge_start - 20 : foot->length - 20) -
        f * kFootStep;
    if (at < kFootStep / 2 || (why == DragWhy::home && at < foot->merge_start - kFootStep)) break;
    const TrackPoint& q = point_at(*foot->points, at);
    const Foot base{q.x, q.z, q.heading, q.width};
    struct Target {
      PlanPoint at;
      bool first;
    };
    std::vector<Target> targets;
    for (size_t l = 0; l < lifts.size(); l++) {
      const LiftPlan& lift = lifts[l];
      if (static_cast<int>(l) == own) {
        targets.push_back({lift.top, true});
      } else if (lift.kind != LiftKind::drag) {
        targets.push_back({lift.top, false});
      }
      if (!g.floor(lift.bottom) && lift.kind != LiftKind::drag) {
        targets.push_back({lift.bottom, false});
      }
    }
    for (size_t k = 0; k < runs.size(); k++) {
      const PlanRun& r = runs[k];
      if (r.kind != RunKind::road || k == i) continue;
      double s = kLaneStep;
      while (s < r.length - kLaneStep / 2) {
        const TrackPoint& t = point_at(*r.points, s);
        targets.push_back({{t.x, t.z}, false});
        s += kLaneStep;
      }
    }
    struct Ranked {
      Target t;
      double d;
    };
    std::vector<Ranked> ranked;
    for (const Target& t : targets) {
      if (!(t.at.z < base.z - 60)) continue;
      const double d = hypot(t.at.x - base.x, t.at.z - base.z);
      if (d >= D::length.min && d <= D::length.max + D::beside) ranked.push_back({t, d});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& c) {
      return a.t.first == c.t.first ? a.d - c.d < 0 : a.t.first;
    });
    for (size_t k = 0; k < std::min(kTargets, ranked.size()); k++) {
      std::optional<LiftPlan> lift = plan_drag(g, base, ranked[k].t.at, id);
      if (!lift) continue;
      lifts.push_back(*lift);
      const std::vector<PlanLift> planned = plan_lifts(g, lifts);
      const PlanAccess v = reckon_access(runs, planned, g.slack);
      if (v.ok[i] && (why == DragWhy::back || v.home[i] <= RR::access::runout)) return true;
      lifts.pop_back();
    }
  }
  return false;
}

DragsSettled settle_drags(const DragGround& g, std::span<const PlanRun> runs,
                          std::vector<LiftPlan>& lifts,
                          const std::function<std::string(size_t)>& name,
                          const std::function<bool(const std::string&)>& pinned) {
  DragsSettled out{std::nullopt, {}, 0, 0};
  // A drag laid before the runs were graded that no longer fits the pressed mountain comes down.
  for (size_t k = lifts.size(); k-- > 0;) {
    const LiftPlan& l = lifts[k];
    if (l.kind == LiftKind::drag && l.id != "D1" && !pinned(l.id) && !fits(g, l.bottom, l.top)) {
      lifts.erase(lifts.begin() + std::ptrdiff_t(k));
    }
  }
  const auto read = [&]() {
    const std::vector<PlanLift> planned = plan_lifts(g, lifts);
    return reckon_access(runs, planned, g.slack);
  };
  PlanAccess v = read();
  for (size_t i = 0; i < runs.size(); i++) {
    if (!v.ok[i] && drag_back(g, runs, lifts, i)) {
      out.drags++;
      v = read();
    }
  }
  for (size_t i = 0; i < runs.size(); i++) {
    if (!v.ok[i]) {
      out.refused = "R29: piste " + name(i) + " — " + v.why[i];
      return out;
    }
  }
  if (!v.orphans.empty()) {
    std::string ids;
    for (const std::string& id : v.orphans) ids += (ids.empty() ? "" : ", ") + id;
    out.refused = "R29: no skier reaches " + ids;
    return out;
  }
  for (size_t i = 0; i < runs.size(); i++) {
    if (v.home[i] > RR::access::runout && drag_back(g, runs, lifts, i, DragWhy::home)) {
      out.home++;
      v = read();
    }
  }
  const PlanAccess want = v;
  for (size_t k = lifts.size(); k-- > 0;) {
    const LiftPlan l = lifts[k];
    if (l.kind != LiftKind::drag || l.id == "D1" || pinned(l.id)) continue;
    lifts.erase(lifts.begin() + std::ptrdiff_t(k));
    const PlanAccess without = read();
    bool needed = !without.orphans.empty();
    for (size_t i = 0; !needed && i < runs.size(); i++) {
      needed = !without.ok[i] ||
               (want.home[i] <= RR::access::runout && without.home[i] > RR::access::runout);
    }
    if (needed) lifts.insert(lifts.begin() + std::ptrdiff_t(k), l);
  }
  int n = 1;
  for (LiftPlan& l : lifts) {
    if (l.kind != LiftKind::drag || l.id == "D1") continue;
    const std::string renamed = "D" + std::to_string(++n);
    out.renamed[l.id] = renamed;
    l.id = renamed;
  }
  return out;
}

}  // namespace ge::mountain::mapgen

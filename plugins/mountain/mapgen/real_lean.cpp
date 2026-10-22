// The leaning behind real_lean.hpp: nearest searches over the hints, the pistes' bends bucketed
// by rows (the steering) and by cells (the widths) so a walk step reads only those near it.
//
// ge:summary R25 leaning on a real face's hints: stations, lifts, piste slots, widths, billing
// ge:keywords real-hints lean-station lean-lift piste-ahead piste-width
// ge:skill mountain
// ge:test tests/mountain_real_hints_test.cpp
#include "mountain/mapgen/real_lean.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>

#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace RL = resort_rules::massif::real;
using ge::core::hypot;

namespace {

double moved(double at, double to, double rise) {
  return at + num::max(-rise, num::min(rise, to - at));
}

// The rows a steering search reads the pistes' segments in, m.
constexpr double kRow = 100;
// The cells a width search reads the pistes' bends in, m (no less than `RL::width`).
constexpr double kCell = 50;

struct Segment {
  PlanPoint a;
  PlanPoint b;
  bool mine;
  int64_t lo;  // the first row it lies in
};

// The pistes' segments by the rows of the face they cross.
struct Rows {
  std::map<int64_t, std::vector<Segment>> rows;
  Band ahead;
  double aside;
  double same;
};

void add_piste(Rows& r, const HintPiste& p, bool mine) {
  for (size_t i = 1; i < p.points.size(); i++) {
    const PlanPoint a = p.points[i - 1];
    const PlanPoint b = p.points[i];
    const auto lo = int64_t(std::floor(num::min(a.z, b.z) / kRow));
    const auto hi = int64_t(std::floor(num::max(a.z, b.z) / kRow));
    for (int64_t k = lo; k <= hi; k++) r.rows[k].push_back({a, b, mine, lo});
  }
}

HintSteer steer_on(const std::shared_ptr<const Rows>& r) {
  return [r](double x, double z) -> std::optional<PlanPoint> {
    std::optional<PlanPoint> best;
    double best_d = INFINITY;
    const double z0 = z + r->ahead.min;
    const double z1 = z + r->ahead.max;
    const auto k0 = int64_t(std::floor(z0 / kRow));
    const auto k1 = int64_t(std::floor(z1 / kRow));
    for (int64_t k = k0; k <= k1; k++) {
      const auto row = r->rows.find(k);
      if (row == r->rows.end()) continue;
      for (const Segment& s : row->second) {
        // A segment crossing several rows is read in the first of them the search reaches.
        if (k > std::max(s.lo, k0)) continue;
        // The segment's stretch inside the rows ahead, its nearest point across to the run.
        const double dz = s.b.z - s.a.z;
        const double t0 = dz == 0 ? 0 : num::max(0.0, num::min(1.0, (z0 - s.a.z) / dz));
        const double t1 = dz == 0 ? 1 : num::max(0.0, num::min(1.0, (z1 - s.a.z) / dz));
        const double lo = num::min(t0, t1);
        const double hi = num::max(t0, t1);
        const double dx = s.b.x - s.a.x;
        const double t = dx == 0 ? lo : num::max(lo, num::min(hi, (x - s.a.x) / dx));
        const PlanPoint p{s.a.x + dx * t, s.a.z + dz * t};
        if (p.z < z0 - 1 || p.z > z1 + 1) continue;
        const double across = std::abs(p.x - x);
        const double d = across - (s.mine ? r->same : 0);
        if (across <= r->aside && d < best_d) {
          best = p;
          best_d = d;
        }
      }
    }
    return best;
  };
}

}  // namespace

std::optional<PisteGrade> piste_grade_of(HintGrade g) {
  switch (g) {
    case HintGrade::green:
      return PisteGrade::green;
    case HintGrade::blue:
      return PisteGrade::blue;
    case HintGrade::red:
      return PisteGrade::red;
    case HintGrade::black:
      return PisteGrade::black;
    case HintGrade::route:
      break;
  }
  return std::nullopt;
}

PlanPoint lean_station(const RealHints& hints, HintsUsed& used, PlanPoint at, bool top,
                       bool keep_z) {
  int best = -1;
  double best_d = RL::station::reach;
  for (size_t i = 0; i < hints.lifts.size(); i++) {
    if (used.lifts[i]) continue;
    const PlanPoint p = top ? hints.lifts[i].top : hints.lifts[i].bottom;
    const double d = hypot(p.x - at.x, p.z - at.z);
    if (d < best_d && std::abs(p.z - at.z) <= RL::station::rise * 2) {
      best = int(i);
      best_d = d;
    }
  }
  if (best < 0) return at;
  used.lifts[size_t(best)] = true;
  const HintLift& l = hints.lifts[size_t(best)];
  const PlanPoint p = top ? l.top : l.bottom;
  return {p.x, keep_z ? at.z : moved(at.z, p.z, RL::station::rise)};
}

std::optional<LeantLift> lean_lift(const RealHints& hints, HintsUsed& used, PlanPoint bottom,
                                   PlanPoint top, std::initializer_list<HintLiftKind> kinds,
                                   bool keep_bottom, bool keep_top) {
  // Where a real lift's line, carried on down, crosses the row z; nothing past its carry.
  const auto along = [](const HintLift& l, double z) -> std::optional<double> {
    const double dz = l.bottom.z - l.top.z;
    if (dz <= 0 || z - l.bottom.z > RL::lift::carry) return std::nullopt;
    return l.top.x + (l.bottom.x - l.top.x) * (z - l.top.z) / dz;
  };
  int best = -1;
  double best_x = 0;
  double best_d = INFINITY;
  for (size_t i = 0; i < hints.lifts.size(); i++) {
    const HintLift& l = hints.lifts[i];
    if (used.lifts[i] || std::find(kinds.begin(), kinds.end(), l.kind) == kinds.end()) continue;
    const std::optional<double> xb = keep_bottom ? along(l, bottom.z) : l.bottom.x;
    if (!xb) continue;
    const double db =
        keep_bottom ? std::abs(*xb - bottom.x) : hypot(*xb - bottom.x, l.bottom.z - bottom.z);
    const double dt = hypot(l.top.x - top.x, l.top.z - top.z);
    if (db < RL::lift::reach && dt < RL::lift::reach && db + dt < best_d) {
      best = int(i);
      best_x = *xb;
      best_d = db + dt;
    }
  }
  if (best < 0) return std::nullopt;
  used.lifts[size_t(best)] = true;
  const HintLift& l = hints.lifts[size_t(best)];
  return LeantLift{{best_x, keep_bottom ? bottom.z : moved(bottom.z, l.bottom.z, RL::lift::rise)},
                   {l.top.x, keep_top ? top.z : moved(top.z, l.top.z, RL::lift::rise)}};
}

const HintPiste* piste_for(const RealHints& hints, HintsUsed& used, PlanPoint start,
                           PlanPoint target) {
  int best = -1;
  double best_d = RL::along::reach;
  const double drop = target.z - start.z;
  for (size_t i = 0; i < hints.pistes.size(); i++) {
    const HintPiste& p = hints.pistes[i];
    if (used.pistes[i] || p.grade == HintGrade::route || p.points.size() < 3) continue;
    const PlanPoint top = p.points.front();
    if (p.points.back().z - top.z < drop * RL::along::fall) continue;
    const double d = hypot(top.x - start.x, top.z - start.z);
    if (d < best_d) {
      best = int(i);
      best_d = d;
    }
  }
  if (best < 0) return nullptr;
  used.pistes[size_t(best)] = true;
  return &hints.pistes[size_t(best)];
}

std::vector<PlanPoint> piste_via(const RealHints& hints, HintsUsed& used, PlanPoint start,
                                 PlanPoint target, PisteGrade grade) {
  int best = -1;
  double best_d = RL::via::reach;
  const double drop = target.z - start.z;
  for (size_t i = 0; i < hints.pistes.size(); i++) {
    const HintPiste& p = hints.pistes[i];
    if (used.pistes[i] || p.points.size() < 3) continue;
    const PlanPoint top = p.points.front();
    if (p.points.back().z - top.z < drop * RL::via::fall) continue;
    const bool mine = piste_grade_of(p.grade) == grade;
    const double d = hypot(top.x - start.x, top.z - start.z) - (mine ? RL::via::same : 0);
    if (d < best_d) {
      best = int(i);
      best_d = d;
    }
  }
  std::vector<PlanPoint> via;
  if (best < 0) return via;
  const HintPiste& p = hints.pistes[size_t(best)];
  for (size_t i = 1; i < p.points.size(); i++) {
    const PlanPoint q = p.points[i];
    if (q.z > start.z + RL::via::margin && q.z < target.z - RL::via::margin) via.push_back(q);
  }
  if (!via.empty()) used.pistes[size_t(best)] = true;
  return via;
}

HintSteer piste_ahead(const RealHints& hints, std::optional<PisteGrade> grade) {
  auto r = std::make_shared<Rows>();
  r->ahead = RL::follow::ahead;
  r->aside = RL::follow::aside;
  r->same = RL::follow::same;
  for (const HintPiste& p : hints.pistes) {
    add_piste(*r, p, grade && piste_grade_of(p.grade) == grade);
  }
  return steer_on(r);
}

HintSteer along_piste(const HintPiste& piste) {
  auto r = std::make_shared<Rows>();
  r->ahead = RL::along::ahead;
  r->aside = RL::along::aside;
  r->same = 0;
  add_piste(*r, piste, false);
  return steer_on(r);
}

HintWidth piste_width(const RealHints& hints) {
  struct Bend {
    double x, z, w;
  };
  auto cells = std::make_shared<std::map<std::pair<int64_t, int64_t>, std::vector<Bend>>>();
  for (const HintPiste& p : hints.pistes) {
    for (size_t i = 0; i < p.points.size() && i < p.widths.size(); i++) {
      if (p.widths[i] <= 0) continue;
      const PlanPoint q = p.points[i];
      (*cells)[{int64_t(std::floor(q.x / kCell)), int64_t(std::floor(q.z / kCell))}].push_back(
          {q.x, q.z, p.widths[i]});
    }
  }
  return [cells](double x, double z) {
    const auto ci = int64_t(std::floor(x / kCell));
    const auto cj = int64_t(std::floor(z / kCell));
    double best = 0;
    double best_d = RL::width;
    for (int64_t i = ci - 1; i <= ci + 1; i++) {
      for (int64_t j = cj - 1; j <= cj + 1; j++) {
        const auto cell = cells->find({i, j});
        if (cell == cells->end()) continue;
        for (const Bend& q : cell->second) {
          const double d = hypot(q.x - x, q.z - z);
          if (d < best_d) {
            best = q.w;
            best_d = d;
          }
        }
      }
    }
    return best;
  };
}

PisteGrade billed_colour(std::optional<PisteGrade> signed_as, PisteGrade measured) {
  if (!signed_as) return measured;
  const int off = std::abs(int(*signed_as) - int(measured));
  return off <= RL::least::billed ? *signed_as : measured;
}

}  // namespace ge::mountain::mapgen

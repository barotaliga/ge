// The course's checks behind resort.hpp: a ski area's course held as the piste a race is run
// down. It shares the start line, the day and the weather with every map (shared.hpp); what
// differs from the single piste is that a course is a chain of runs, so it may narrow to a
// lane's width, cross onto the next run's surface at a junction, and carries slalom gates set
// off its line.
//
// ge:summary The course's checks: the line, the gates, the start, the snow, the drifts, the colour
// ge:keywords analysis analyze-course
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "mountain/analysis/crossings.hpp"
#include "mountain/analysis/resort.hpp"
#include "mountain/analysis/shared.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/hub.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/spawn.hpp"
#include "mountain/mapgen/track.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace R = mapgen::rules;
namespace RR = mapgen::resort_rules;
using mapgen::Band;
using mapgen::Cliff;
using mapgen::Kicker;
using mapgen::TrackPoint;
using mapgen::within_band;

namespace {

// R5 — how far up the map a station may stand from the one before, m: a course crossing onto
// the next run eases sideways across the fall line.
constexpr double kBackSlack = 0.05;
// R5 — how far outside the hub the finish may stand, m.
constexpr double kHubSlack = 1;
// R8 — the most a course may climb over a grade window outside a kicker or a drop, m per m: a
// junction where two runs were graded apart may leave a lip this high.
constexpr double kJunctionClimb = 0.03;
// R10 — how far past a drift's fade the packed field may still read it, m: the grid reads a cell
// either side of a point, and a course's arcs are its runs' eased by a few metres.
constexpr double kDriftReach = 4;
// R11 — how far a gate may stand off where its offset puts it, m.
constexpr double kGateSlack = 0.75;

}  // namespace

LevelAnalysis analyze_course(const mapgen::Level& level) {
  LevelAnalysis out;
  const Add add = [&](const char* rule, Severity severity, std::string message) {
    out.findings.push_back({rule, severity, std::move(message)});
  };
  const auto& pts = level.track.points;
  const size_t n = pts.size();
  const double L = level.track.length;
  const double size = level.size;
  const auto inside = [&](double x, double z) {
    return x >= 0 && z >= 0 && x <= size && z <= size;
  };
  if (n < 2) {
    add("R5", Severity::error, "the course has no line");
    out.ok = false;
    return out;
  }

  // R1 — everything on the map.
  bool all = true;
  for (const TrackPoint& p : pts) all = all && inside(p.x, p.z);
  if (!all) add("R1", Severity::error, "the course leaves the map");
  all = inside(level.spawn.x, level.spawn.z);
  for (const mapgen::Spawn& g : level.grid) all = all && inside(g.x, g.z);
  if (!all) add("R1", Severity::error, "a start slot is off the map");

  // R5, R28 — one open descent, its stations even, out into the hub.
  if (!within_band(L, RR::course::length)) {
    add("R28", Severity::error,
        "the course is " + fmt(L, 0) + " m (band " + band_text(RR::course::length, " m") + ")");
  }
  double uneven = 0;
  for (size_t i = 1; i + 1 < n; i++) {
    uneven = num::max(uneven, std::abs(pts[i].s - pts[i - 1].s - R::track::step));
  }
  if (uneven > 0.01 || pts.back().s - pts[n - 2].s > R::track::step + 0.01) {
    add("R5", Severity::error,
        "stations stray " + fmt(uneven, 2) + " m off every " + str(R::track::step) + " m");
  }
  double back_at = -1;
  for (size_t i = 1; i < n && back_at < 0; i++) {
    if (pts[i].z < pts[i - 1].z - kBackSlack) back_at = pts[i].s;
  }
  if (back_at >= 0) {
    add("R5", Severity::error, "the course turns back up the map at s " + fmt(back_at, 0) + " m");
  }
  const int crossings = self_crossings(pts);
  if (crossings > 0) {
    add("R5", Severity::error, "the course crosses itself " + str(crossings) + " time(s)");
  }
  if (level.resort) {
    const double off = mapgen::outside_hub(level.resort->hub, pts[n - 1].x, pts[n - 1].z);
    if (off > kHubSlack) {
      add("R5", Severity::error, "the course ends " + fmt(off, 0) + " m outside the hub");
    }
  }

  // R7 — the width: a lane's at the narrowest, a piste's widest at the most.
  double width_min = INFINITY;
  double width_max = 0;
  for (const TrackPoint& p : pts) {
    width_min = num::min(width_min, p.width);
    width_max = num::max(width_max, p.width);
  }
  const Band widths{RR::road::width.min, mapgen::kWidest};
  if (!within_band(width_min, widths, 0.5) || !within_band(width_max, widths, 0.5)) {
    add("R7", Severity::error,
        "width runs " + fmt(width_min) + "–" + fmt(width_max) + " m (band " +
            band_text(widths, " m") + ")");
  }

  // R8 — no climb outside the kickers and the drops; R28 — the colour the course is billed.
  std::vector<const Kicker*> kickers;
  for (const Kicker& k : level.kickers) {
    if (k.on_track) kickers.push_back(&k);
  }
  std::vector<const Cliff*> drops;
  for (const Cliff& c : level.cliffs) {
    if (c.on_track) drops.push_back(&c);
  }
  std::vector<uint8_t> skip(n, 0);
  std::vector<double> ys(n);
  for (size_t i = 0; i < n; i++) {
    ys[i] = pts[i].y;
    const double s = pts[i].s;
    for (const Kicker* k : kickers) {
      const double u = s - k->s.value_or(0);
      if (u > -k->ramp - 4 && u < k->landing + 4) skip[i] = 1;
    }
    for (const Cliff* d : drops) {
      const double u = s - d->s.value_or(0);
      if (u > -4 && u < d->face + d->landing + 4) skip[i] = 1;
    }
  }
  const double step = L / double(n - 1);
  const double climb = num::max(0.0, -mapgen::window_grades(ys, step, skip).gentlest);
  if (climb > kJunctionClimb) {
    add("R8", Severity::error, "the course climbs at " + fmt(climb, 3) + " outside a kicker");
  }
  const double steepest = mapgen::steepest_span(level.track, level.kickers);
  const mapgen::PisteGrade colour = mapgen::piste_grade_of(steepest);
  if (level.grade && colour != *level.grade) {
    add("R28", Severity::error,
        std::string("a ") + mapgen::grade_name(*level.grade) + " course whose steepest " +
            str(R::track::colour_window) + " m falls at " + fmt(steepest * 100, 0) + " % — a " +
            mapgen::grade_name(colour) + "'s");
  }

  // R10 — packed on the line, outside the drifts (R17).
  const double F = R::drift::fade;
  const auto drifted = [&](double s) {
    for (const mapgen::Drift& d : level.drifts) {
      if (s > d.from - F - kDriftReach && s < d.to + F + kDriftReach) return true;
    }
    return false;
  };
  double packed_low = 1;
  double packed_at = 0;
  for (size_t i = 0; i < n; i += 5) {
    const double packed = level.packed_at(pts[i].x, pts[i].z);
    if (!drifted(pts[i].s) && packed < packed_low) {
      packed_low = packed;
      packed_at = pts[i].s;
    }
  }
  if (packed_low < 0.98) {
    add("R10", Severity::error,
        "the course is only " + fmt(packed_low, 2) + " packed at s " + fmt(packed_at, 0) + " m");
  }

  // R11, R28 — the gates: the start gate and the finish across the piste, the slalom gates
  // between them in order, spaced in the band, each inside the piste where it stands.
  const auto& cps = level.checkpoints;
  const double gate_arc = mapgen::start_gate_arc();
  if (cps.size() < 3) {
    add("R11", Severity::error, str(double(cps.size())) + " gates");
  } else {
    if (std::abs(cps[0].s - gate_arc) > 1e-6) {
      add("R11", Severity::error, "the start gate stands at s " + fmt(cps[0].s) + " m");
    }
    if (std::abs(cps.back().s - L) > 1e-6) {
      add("R11", Severity::error, "the finish is not at the course's end");
    }
    const Band gap = RR::course::gates::spacing;
    double spacing_min = INFINITY;
    double spacing_max = 0;
    for (size_t i = 0; i + 1 < cps.size(); i++) {
      const double d = cps[i + 1].s - cps[i].s;
      if (d <= 0) {
        add("R11", Severity::error,
            "gate " + str(double(i + 1)) + " stands up the course from gate " + str(double(i)));
      }
      spacing_max = num::max(spacing_max, d);
      // The last gap runs out to the finish, and may be short.
      if (i + 2 < cps.size()) spacing_min = num::min(spacing_min, d);
    }
    if (spacing_min < gap.min - 0.5 || spacing_max > gap.max + 0.5) {
      add("R11", Severity::error,
          "gates " + fmt(spacing_min, 0) + "–" + fmt(spacing_max, 0) + " m apart");
    }
    for (size_t i = 0; i < cps.size(); i++) {
      const mapgen::Checkpoint& c = cps[i];
      const double offset = c.offset.value_or(0);
      const mapgen::TrackHit hit = mapgen::nearest_track_point(level.track, c.x, c.z);
      if (std::abs(hit.distance - std::abs(offset)) > kGateSlack) {
        add("R11", Severity::error,
            "gate " + str(double(i)) + " stands " + fmt(hit.distance) + " m off the line");
      }
      if (c.offset && std::abs(offset) + c.width / 2 >
                          c.span.value_or(0) / 2 - RR::course::gates::margin + 0.5) {
        add("R11", Severity::error, "gate " + str(double(i)) + " stands outside the piste");
      }
    }
  }

  // R12 — the start gate clear of every kicker and drop down the course.
  for (const Kicker* k : kickers) {
    const double ds = k->s.value_or(0) - k->ramp - gate_arc;
    if (ds < 0) {
      add("R12", Severity::error, k->id + " stands " + fmt(ds, 0) + " m from the start gate");
    }
  }
  for (const Cliff* d : drops) {
    const double ds = d->s.value_or(0) - d->shelf - gate_arc;
    if (ds < 0) {
      add("R12", Severity::error, d->id + " stands " + fmt(ds, 0) + " m from the start gate");
    }
  }

  // R13 — the start line; R15, R19 — the day and the weather.
  check_start_line(level, add);
  check_day(level, add);
  check_park(level, add);

  // R16 — one run.
  if (level.laps != R::race::laps) add("R16", Severity::error, str(level.laps) + " runs");

  // R17 — the drifts: on the course, off its kickers, and loose.
  for (const mapgen::Drift& d : level.drifts) {
    if (d.from < 0 || d.to > L || d.to <= d.from) {
      add("R17", Severity::error, "a drift at s " + fmt(d.from, 0) + " m is not on the course");
      continue;
    }
    for (const Kicker* k : kickers) {
      const double s0 = k->s.value_or(0);
      if (d.from - F < s0 + k->landing && d.to + F > s0 - k->ramp) {
        add("R17", Severity::error, "a drift at s " + fmt(d.from, 0) + " m lies over " + k->id);
      }
    }
  }

  for (const Finding& f : out.findings) {
    if (f.severity == Severity::error) out.ok = false;
  }
  return out;
}

}  // namespace ge::mountain::analysis

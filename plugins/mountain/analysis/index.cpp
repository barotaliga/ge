// The single piste's checks behind index.hpp (no real face and no terrain park: every junction
// mask is empty); a ski area's map is handed to resort.hpp's checks.
//
// ge:summary The single piste's checks, R1-R19, with their fixed message text
// ge:keywords analysis
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/analysis/index.hpp"

#include <array>
#include <cmath>
#include <unordered_map>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/solar.hpp"
#include "mountain/analysis/cliffs.hpp"
#include "mountain/analysis/crossings.hpp"
#include "mountain/analysis/race.hpp"
#include "mountain/analysis/resort.hpp"
#include "mountain/analysis/shared.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/spawn.hpp"
#include "mountain/mapgen/sun.hpp"
#include "mountain/mapgen/track.hpp"
#include "mountain/mapgen/weather.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = mapgen::rules;
using ge::core::angle_diff;
using ge::core::hypot;
using ge::core::kPi;
using mapgen::Band;
using mapgen::Kicker;
using mapgen::TrackPoint;
using mapgen::within_band;

namespace {

// R9 — the least break in grade across a piste kicker's lip that still throws a skier.
constexpr double kKick = 0.15;
// R18 — the share of the lowest crest the two-metre grid is allowed to lose.
constexpr double kBermLow = 0.4;
// R18 — how much steeper than the rule a windrow's face may READ off the grid, m per m.
constexpr double kBermSteepSlack = 0.25;
// R10 — how packed the powder eight metres past the edge may read.
constexpr double kPowderSlack = 0.1;
// R2 — how far the ground at the published summit may stand off the summit's own height.
constexpr double kSummitSlack = 2;

}  // namespace

LevelAnalysis analyze_level(const mapgen::Level& level) {
  if (level.race) return analyze_race(level);
  if (level.resort) {
    LevelAnalysis out = analyze_course(level);
    LevelAnalysis area = analyze_resort(level);
    out.ok = out.ok && area.ok;
    out.findings.insert(out.findings.end(), area.findings.begin(), area.findings.end());
    return out;
  }
  LevelAnalysis out;
  const Add add = [&](const char* rule, Severity severity, std::string message) {
    out.findings.push_back({rule, severity, std::move(message)});
  };
  const auto& pts = level.track.points;
  const size_t n = pts.size();
  const double L = level.track.length;
  const double step = n > 1 ? L / double(n - 1) : R::track::step;
  const double size = level.size;
  const mapgen::Region& region = mapgen::region_row(level.region);
  const mapgen::GradeRow& G = mapgen::grade_row(level.grade);
  const auto inside = [&](double x, double z) {
    return x >= 0 && z >= 0 && x <= size && z <= size;
  };

  // R1 — everything on the map.
  bool all = true;
  for (const TrackPoint& p : pts) all = all && inside(p.x, p.z);
  if (!all) add("R1", Severity::error, "the piste leaves the map");
  all = true;
  for (const mapgen::Spawn& g : level.grid) all = all && inside(g.x, g.z);
  if (!all) add("R1", Severity::error, "a start slot is off the map");
  all = true;
  for (const mapgen::TreeDef& t : level.trees) all = all && inside(t.x, t.z);
  if (!all) add("R1", Severity::error, "a tree stands off the map");

  // R2 — the mountain.
  const mapgen::Mountain& M = level.mountain;
  {
    const Band band = mapgen::vertical_band(region, G);
    if (!within_band(M.vertical, band, 1e-6)) {
      add("R2", Severity::error,
          "a vertical of " + fmt(M.vertical, 0) + " m (band " + band_text(band, " m") + ")");
    }
    if (std::abs(M.summit.y - M.base.y - M.vertical) > 0.01) {
      add("R2", Severity::error, "the summit does not stand the vertical over the base");
    }
    if (!within_band(M.altitude, region.altitude.base) ||
        !within_band(M.tree_line, region.altitude.tree_line)) {
      add("R21", Severity::error,
          "a base at " + fmt(M.altitude, 0) + " m with the tree line at " + fmt(M.tree_line, 0) +
              " m");
    }
    if (std::abs(M.base.y - level.ground_at(M.base.x, M.base.z)) > 0.05) {
      add("R2", Severity::error, "the base is not on the ground");
    }
    const double slack = kSummitSlack * (R::mountain::crests * region.relief.crests +
                                         R::face::hills::amplitude.max * region.relief.hills);
    const double top = level.ground_at(M.summit.x, M.summit.z);
    if (std::abs(top - M.summit.y) > slack) {
      add("R2", Severity::error,
          "the ground at the summit stands " + fmt(top - M.summit.y, 0) + " m off it");
    }
    if (std::abs(M.summit.z - R::mountain::summit * size) > 1) {
      add("R2", Severity::error, "the summit is not on the summit ridge");
    }
    if (M.base.z < R::mountain::base * size - 1) {
      add("R2", Severity::error, "the base is not on the valley floor");
    }
  }

  // R5 — one open descent.
  const Band length_band = G.track.length;
  if (!within_band(L, length_band)) {
    add("R5", Severity::error,
        "the piste is " + fmt(L, 0) + " m (band " + band_text(length_band, " m") + ")");
  }
  if (std::abs(step - R::track::step) > 0.2) {
    add("R5", Severity::error, "stations every " + fmt(step, 2) + " m");
  }
  bool descends = true;
  for (size_t i = 1; i < n; i++) {
    if (pts[i].z <= pts[i - 1].z) descends = false;
  }
  if (!descends) add("R5", Severity::error, "the piste turns back up the map");
  if (n > 0 && std::abs(pts[0].z - R::track::start_z * size) > 1) {
    add("R5", Severity::error, "the start stands " + fmt(pts[0].z, 0) + " m down the map");
  }
  const int crossings = self_crossings(pts);
  if (crossings > 0) {
    add("R5", Severity::error, "the piste crosses itself " + str(crossings) + " time(s)");
  }
  const double separation = mapgen::min_separation(level.track);
  if (separation < R::track::separation_plan - 0.5) {
    add("R5", Severity::error,
        "two stretches pass " + fmt(separation) + " m apart (least " +
            str(R::track::separation_plan) + " m)");
  }
  if (n > 0 && pts[n - 1].z < R::track::finish_z * size - 1) {
    add("R5", Severity::error, "the piste ends above the valley floor");
  }

  // R6 — the bends against the floor their pitch sets, and the room the inside bench has.
  const mapgen::BendReading tight =
      mapgen::tightest_bend(level.track, [&](size_t i) { return pts[i].y; });
  if (tight.radius < tight.floor - 0.5) {
    add("R6", Severity::error,
        "a bend tightens to " + fmt(tight.radius) + " m on a " + fmt(tight.grade * 100) +
            " % pitch at " + fmt(tight.s) + " m (least " + fmt(tight.floor) + " m there)");
  }
  const double room = mapgen::bend_room(level.track);
  if (room < -0.5)
    add("R6", Severity::error, "a bend folds the inside bench by " + fmt(-room) + " m");

  // R7 — the width, and the arena.
  double width_min = INFINITY;
  double width_max = 0;
  for (const TrackPoint& p : pts) {
    width_min = num::min(width_min, p.width);
    width_max = num::max(width_max, p.width);
  }
  const Band widths{G.track.width.min, R::track::width.max};
  if (!within_band(width_min, widths) || !within_band(width_max, widths)) {
    add("R7", Severity::error,
        "width runs " + fmt(width_min) + "–" + fmt(width_max) + " m (band " +
            band_text(widths, " m") + ")");
  }
  if (n > 0 && pts[n - 1].width < R::track::width.max - 0.5) {
    add("R7", Severity::error, "the finish arena is only " + fmt(pts[n - 1].width) + " m wide");
  }

  // R8 — the grade along, outside the kickers, the finish's own flat, and the ground across.
  std::vector<const Kicker*> track_kickers;
  std::vector<const Kicker*> off_kickers;
  for (const Kicker& k : level.kickers) (k.on_track ? track_kickers : off_kickers).push_back(&k);
  std::vector<const mapgen::Cliff*> drops;
  for (const mapgen::Cliff& c : level.cliffs) {
    if (c.on_track) drops.push_back(&c);
  }
  std::vector<uint8_t> skip(n, 0);
  std::vector<uint8_t> steep(n, 0);
  for (size_t i = 0; i < n; i++) {
    const double s = pts[i].s;
    for (const Kicker* k : track_kickers) {
      const double u = s - k->s.value_or(0);
      if (u > -k->ramp - 4 && u < k->landing + 4) skip[i] = steep[i] = 1;
    }
    for (const mapgen::Cliff* d : drops) {
      const double u = s - d->s.value_or(0);
      if (u > -4 && u < d->face + d->landing + 4) skip[i] = 1;
      if (u > -d->shelf - 4 && u < d->face + d->landing + 4) steep[i] = 1;
    }
  }
  std::vector<double> ys(n);
  for (size_t i = 0; i < n; i++) ys[i] = pts[i].y;
  const mapgen::WindowGrades whole = mapgen::window_grades(ys, step, skip);
  const double max_grade = mapgen::window_grades(ys, step, steep).steepest;
  const double steepest_span = mapgen::steepest_span(level.track, level.kickers);
  const mapgen::PisteGrade colour = mapgen::piste_grade_of(steepest_span);
  const double climb = num::max(0.0, -whole.gentlest);
  const double ceiling = num::min(R::track::max_grade, G.track.max_grade);
  if (max_grade > ceiling + 0.01) {
    add("R8", Severity::error,
        "the line falls at " + fmt(max_grade, 3) + " (most " + str(ceiling) + ")");
  }
  if (level.grade && colour != *level.grade) {
    add("R23", Severity::error,
        std::string("a ") + mapgen::grade_name(*level.grade) + " piste whose steepest " +
            str(R::track::colour_window) + " m falls at " + fmt(steepest_span * 100, 0) +
            " % — a " + mapgen::grade_name(colour) + "'s");
  }
  if (climb > 0.005) {
    add("R8", Severity::error, "the line climbs at " + fmt(climb, 3) + " outside a kicker");
  }
  const double finish_at = mapgen::finish_from(L);
  std::vector<uint8_t> held(skip);
  for (size_t i = 0; i < n; i++) {
    if (pts[i].s > finish_at - R::track::grade_window) held[i] = 1;
  }
  const double min_grade = mapgen::window_grades(ys, step, held).gentlest;
  const double least_grade = R::track::min_grade;
  if (min_grade < least_grade - 0.01) {
    add("R8", Severity::error,
        "the line falls only " + fmt(min_grade, 3) + " (least " + str(least_grade) + ")");
  }
  const TrackPoint flat_from = mapgen::track_point_at(level.track, L - R::track::finish);
  const double finish_grade = (flat_from.y - pts[n - 1].y) / R::track::finish;
  if (std::abs(finish_grade) > 0.03) {
    add("R8", Severity::error, "the finish straight falls at " + fmt(finish_grade, 3));
  }
  double max_cross = 0;
  double worst_cross = 0;
  for (size_t i = 0; i < n; i += 3) {
    const TrackPoint& p = pts[i];
    bool on_face = false;
    for (const mapgen::Cliff* d : drops) {
      on_face = on_face || std::abs(p.s - d->s.value_or(0) - d->face / 2) < d->face / 2 + 4;
    }
    if (on_face) continue;
    const double hw = p.width / 2;
    const double rx = libm::cos(p.heading);
    const double rz = -libm::sin(p.heading);
    const double c = level.ground_at(p.x, p.z);
    for (const double side : {-1.0, 1.0}) {
      const double e = level.ground_at(p.x + rx * hw * side, p.z + rz * hw * side);
      const double sl = std::abs(e - c) / hw;
      if (sl > max_cross) {
        max_cross = sl;
        worst_cross = p.s;
      }
    }
  }
  if (max_cross > R::track::camber + 0.03) {
    add("R8", Severity::error,
        "the ground falls " + fmt(max_cross, 3) + " across the piste at s " + fmt(worst_cross, 0) +
            " m");
  }

  // R9 — the kickers on the piste (no park's: every one is a crest).
  const auto& crests = track_kickers;
  if (!within_band(double(crests.size()), G.kickers.on)) {
    add("R9", Severity::error,
        str(double(crests.size())) + " kicker(s) on the piste (band " + band_text(G.kickers.on) +
            ")");
  }
  for (size_t a = 0; a < crests.size(); a++) {
    const Kicker& k = *crests[a];
    const double s0 = k.s.value_or(0);
    const auto at = [&](double u) {
      const TrackPoint p = mapgen::track_point_at(level.track, s0 + u);
      return level.ground_at(p.x, p.z);
    };
    const double lip = at(0);
    const double kick = (lip - at(-3)) / 3 - (at(3) - lip) / 3;
    if (kick < kKick) {
      add("R9", Severity::warn,
          k.id + " at s " + fmt(s0, 0) + " m breaks only " + fmt(kick, 2) + " over its lip");
    }
    const double approach = (at(-k.ramp) - (lip - k.height)) / k.ramp;
    if (approach > G.kickers.approach_grade + 0.03) {
      add("R9", Severity::error, k.id + " is taken off a " + fmt(approach * 100, 0) + " % pitch");
    }
    for (size_t b = a + 1; b < crests.size(); b++) {
      const double ds = std::abs(k.s.value_or(0) - crests[b]->s.value_or(0));
      if (ds < R::kickers::on::spacing - 1) {
        add("R9", Severity::error,
            k.id + " and " + crests[b]->id + " stand " + fmt(ds, 0) + " m apart");
      }
    }
  }
  for (const Kicker* k : off_kickers) {
    const double distance = mapgen::nearest_track_point(level.track, k->x, k->z).distance;
    const double reach = num::max(k->ramp, k->landing) + k->width / 2 + R::kickers::edge;
    if (distance - reach < R::track::width.max / 2 + R::kickers::off::clearance - 1) {
      add("R4", Severity::error, k->id + " stands " + fmt(distance, 0) + " m from the piste");
    }
  }
  if (double(off_kickers.size()) <
      mapgen::scale_count(R::kickers::off::count, region.kickers * G.kickers.off).min) {
    add("R4", Severity::warn,
        "only " + str(double(off_kickers.size())) + " kicker(s) off the piste");
  }

  // R22 — the cliffs; R24 — the drops across the piste.
  check_cliffs(level, add);
  check_drops(level, track_kickers, add);

  // R10 — packed on the line, powder off it, outside the drifts (R17).
  const auto& drifts = level.drifts;
  const double F = R::drift::fade;
  const auto drifted = [&](double s) {
    for (const mapgen::Drift& d : drifts) {
      if (s > d.from - F && s < d.to + F) return true;
    }
    return false;
  };
  double packed_low = 1;
  double packed_high = 0;
  for (size_t i = 0; i < n; i += 5) {
    const TrackPoint& p = pts[i];
    if (!drifted(p.s)) packed_low = num::min(packed_low, level.packed_at(p.x, p.z));
    const double off = p.width / 2 + R::track::shoulder_packed + 3;
    const double rx = libm::cos(p.heading);
    const double rz = -libm::sin(p.heading);
    packed_high = num::max(packed_high, level.packed_at(p.x + rx * off, p.z + rz * off));
  }
  if (packed_low < 0.98) {
    add("R10", Severity::error, "the centreline is only " + fmt(packed_low, 2) + " packed");
  }
  if (packed_high > kPowderSlack) {
    add("R10", Severity::error, "powder beside the piste is " + fmt(packed_high, 2) + " packed");
  }

  // R11 — the gates.
  const auto& cps = level.checkpoints;
  double spacing_min = INFINITY;
  double spacing_max = 0;
  for (size_t i = 0; i < cps.size(); i++) {
    const mapgen::Checkpoint& a = cps[i];
    if (i + 1 < cps.size()) {
      const double d = cps[i + 1].s - a.s;
      spacing_min = num::min(spacing_min, d);
      spacing_max = num::max(spacing_max, d);
      if (d <= 0) {
        add("R11", Severity::error,
            "gate " + str(double(i + 1)) + " stands up the piste from gate " + str(double(i)));
      }
    }
    const mapgen::TrackHit hit = mapgen::nearest_track_point(level.track, a.x, a.z);
    if (std::abs(hit.distance) > 0.75) {
      add("R11", Severity::error,
          "gate " + str(double(i)) + " stands " + fmt(hit.distance) + " m off the line");
    }
  }
  const Band gap = R::checkpoint::spacing;
  if (cps.size() < 2 || spacing_min < gap.min - 0.5 || spacing_max > gap.max + 0.5) {
    add("R11", Severity::error,
        "gates " + fmt(spacing_min, 0) + "–" + fmt(spacing_max, 0) + " m apart");
  }
  const double gate_arc = mapgen::start_gate_arc();
  if (!cps.empty() && std::abs(cps[0].s - gate_arc) > 1e-6) {
    add("R11", Severity::error, "the start gate stands at s " + fmt(cps[0].s) + " m");
  }
  if (!cps.empty() && std::abs(cps.back().s - L) > 1e-6) {
    add("R11", Severity::error, "the finish is not at the piste's end");
  }

  // R12 — the start: the gate clear of every kicker, the run after it straight and gentle.
  for (const Kicker* k : track_kickers) {
    const double ds = k->s.value_or(0) - gate_arc;
    if (ds < R::spawn::kicker_gap - 1) {
      add("R12", Severity::error, k->id + " stands " + fmt(ds, 0) + " m from the start gate");
    }
  }
  for (const mapgen::Cliff* d : drops) {
    const double ds = d->s.value_or(0) - d->shelf - gate_arc;
    if (ds < R::spawn::kicker_gap - 1) {
      add("R12", Severity::error, d->id + " stands " + fmt(ds, 0) + " m from the start gate");
    }
  }
  const TrackPoint gate = mapgen::track_point_at(level.track, gate_arc);
  double bend = 0;
  double steepest = 0;
  for (double u = 0; u + R::track::grade_window <= R::spawn::run; u += step) {
    const TrackPoint a = mapgen::track_point_at(level.track, gate_arc + u);
    const TrackPoint b = mapgen::track_point_at(level.track, gate_arc + u + R::track::grade_window);
    bend = num::max(bend, std::abs(angle_diff(b.heading, gate.heading)));
    steepest = num::max(steepest, (a.y - b.y) / R::track::grade_window);
  }
  if (bend > R::spawn::straight + 0.02) {
    add("R12", Severity::error, "the run out of the gate turns " + fmt(bend, 2) + " rad");
  }
  if (steepest > G.spawn.max_slope + 0.02) {
    add("R12", Severity::error,
        "the run out of the gate is " + fmt(steepest * 100, 0) + " % steep");
  }
  const TrackPoint run_end = mapgen::track_point_at(level.track, gate_arc + R::spawn::run);
  const double start_fall = (gate.y - run_end.y) / R::spawn::run;
  if (G.spawn.min_slope > 0 && start_fall < G.spawn.min_slope - 0.02) {
    add("R12", Severity::error,
        "the run out of the gate falls only " + fmt(start_fall * 100, 0) + " %");
  }

  // R13 — the start line.
  check_start_line(level, add);

  // R14 — the forest: off the corridor, in its height band, under the tree line, room between.
  const double tree_line_y = M.base.y + (M.tree_line - M.altitude);
  int on_corridor = 0;
  int above_line = 0;
  mapgen::TrackHit hit;
  for (const mapgen::TreeDef& t : level.trees) {
    mapgen::nearest_within(level.track, t.x, t.z, R::track::width.max / 2 + R::forest::corridor,
                           hit);
    if (hit.distance < pts[size_t(hit.index)].width / 2 + R::forest::corridor - 0.5) on_corridor++;
    if (!within_band(t.height, R::forest::height)) {
      add("R14", Severity::error, "a tree " + fmt(t.height) + " m tall");
    }
    if (t.y > tree_line_y + 0.01) above_line++;
  }
  if (on_corridor > 0) {
    add("R14", Severity::error, str(on_corridor) + " tree(s) stand on the piste's corridor");
  }
  if (above_line > 0) {
    add("R14", Severity::error, str(above_line) + " tree(s) stand above the tree line");
  }
  // Keyed `bx * 8192 + bz`; looked up only, never iterated.
  std::unordered_map<int64_t, std::vector<uint32_t>> buckets;
  double tree_gap = INFINITY;
  double clump_gap = INFINITY;
  double clump_spread = 0;
  for (size_t ti = 0; ti < level.trees.size(); ti++) {
    const mapgen::TreeDef& t = level.trees[ti];
    const auto bx = int64_t(std::floor(t.x / R::forest::gap));
    const auto bz = int64_t(std::floor(t.z / R::forest::gap));
    for (int64_t dz = -1; dz <= 1; dz++) {
      for (int64_t dx = -1; dx <= 1; dx++) {
        const auto it = buckets.find((bx + dx) * 8192 + bz + dz);
        if (it == buckets.end()) continue;
        for (const uint32_t ui : it->second) {
          const mapgen::TreeDef& u = level.trees[ui];
          const double d = hypot(u.x - t.x, u.z - t.z);
          if (t.clump >= 0 && u.clump == t.clump) {
            clump_gap = num::min(clump_gap, d);
            clump_spread = num::max(clump_spread, d);
          } else {
            tree_gap = num::min(tree_gap, d);
          }
        }
      }
    }
    buckets[bx * 8192 + bz].push_back(uint32_t(ti));
    if (t.crown > R::forest::crown_max + 1e-9) {
      add("R14", Severity::error, "a crown " + fmt(t.crown) + " m wide");
    }
  }
  if (tree_gap < R::forest::gap - 0.01) {
    add("R14", Severity::error,
        "two trunks stand " + fmt(tree_gap) + " m apart (least " + str(R::forest::gap) + " m)");
  }
  if (clump_gap < R::forest::clumps::gap - 0.01) {
    add("R14", Severity::error, "two trunks of a clump stand " + fmt(clump_gap) + " m apart");
  }
  if (clump_spread > 2 * R::forest::clumps::radius + 0.01) {
    add("R14", Severity::error, "a clump spreads " + fmt(clump_spread) + " m");
  }
  if (double(level.trees.size()) < 600 * region.forest.density) {
    add("R14", Severity::warn, "only " + str(double(level.trees.size())) + " trees");
  }

  // R15 — the day; R19 — the weather.
  check_day(level, add);

  // R16 — one run.
  if (level.laps != R::race::laps) add("R16", Severity::error, str(level.laps) + " runs");

  // R17 — the drifts.
  double drift_length = 0;
  for (size_t i = 0; i < drifts.size(); i++) {
    const mapgen::Drift& d = drifts[i];
    const double len = d.to - d.from;
    drift_length += len;
    if (len < R::drift::length.min - 1 || len > R::drift::length.max + 1) {
      add("R17", Severity::error,
          "a drift at s " + fmt(d.from, 0) + " m is " + fmt(len, 0) + " m long");
    }
    if (d.from - F < R::drift::clear - 1 || d.to + F > L - R::drift::clear + 1) {
      add("R17", Severity::error,
          "a drift at s " + fmt(d.from, 0) + " m lies on the start or the finish");
    }
    if (i + 1 < drifts.size() && drifts[i + 1].from - d.to < R::drift::gap + 2 * F - 1) {
      add("R17", Severity::error,
          "two drifts stand " + fmt(drifts[i + 1].from - d.to, 0) + " m apart");
    }
    for (const Kicker* k : track_kickers) {
      const double s0 = k->s.value_or(0);
      if (d.from - F < s0 + k->landing && d.to + F > s0 - k->ramp) {
        add("R17", Severity::error, "a drift at s " + fmt(d.from, 0) + " m lies over " + k->id);
      }
    }
    double deepest = 0;
    // The scan steps by 5 m from a fractional start, so the counter is a double: those are the
    // distances the golden's findings were measured at.
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter)
    for (double s = d.from; s <= d.to; s += 5) {
      const TrackPoint p = mapgen::track_point_at(level.track, s);
      deepest = num::max(deepest, level.packed_at(p.x, p.z));
    }
    if (deepest > R::drift::packed + 0.05) {
      add("R17", Severity::error,
          "the drift at s " + fmt(d.from, 0) + " m is " + fmt(deepest, 2) + " packed");
    }
  }
  if (drift_length / L > G.drift.max + R::drift::length.min / L + 0.01) {
    add("R17", Severity::warn, fmt((100 * drift_length) / L, 0) + " % of the piste is drifted");
  }

  // R18 — the windrows, read off the ground across the bench they stand on.
  const double toe = R::track::shoulder_flat;
  constexpr int kAcross = 10;
  double berm_low = INFINITY;
  double berm_steep = 0;
  double worst_berm = 0;
  double steep_berm = 0;
  for (size_t i = 0; i < n; i += 3) {
    if (skip[i]) continue;
    const TrackPoint& p = pts[i];
    const double hw = p.width / 2;
    const double rx = libm::cos(p.heading);
    const double rz = -libm::sin(p.heading);
    for (const double side : {-1.0, 1.0}) {
      const auto at = [&](double d) {
        return level.ground_at(p.x + rx * d * side, p.z + rz * d * side);
      };
      const double edge = at(hw);
      double crest = -INFINITY;
      double prev = at(hw + toe);
      for (int j = 1; j <= kAcross; j++) {
        const double u = (double(j) / kAcross) * R::berm::width;
        const double y = at(hw + toe + u);
        crest = num::max(crest, y - edge);
        const double face = std::abs(y - prev) / (R::berm::width / kAcross);
        if (face > berm_steep) {
          berm_steep = face;
          steep_berm = p.s;
        }
        prev = y;
      }
      if (crest < berm_low) {
        berm_low = crest;
        worst_berm = p.s;
      }
    }
  }
  if (n > 0 && berm_low < kBermLow * R::berm::height.min) {
    add("R18", Severity::error,
        "the windrow stands only " + fmt(berm_low, 2) + " m over the edge at s " +
            fmt(worst_berm, 0) + " m");
  }
  if (berm_steep > R::berm::max_slope + kBermSteepSlack) {
    add("R18", Severity::error,
        "a windrow's face climbs at " + fmt(berm_steep, 2) + " (most " + str(R::berm::max_slope) +
            ") at s " + fmt(steep_berm, 0) + " m");
  }

  for (const Finding& f : out.findings) out.ok = out.ok && f.severity != Severity::error;
  return out;
}

}  // namespace ge::mountain::analysis

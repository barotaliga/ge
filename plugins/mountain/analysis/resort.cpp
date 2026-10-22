// The area's checks behind resort.hpp: the runs, the lifts, the courses, access, the woods, the
// hub and the tunnels, read off `Level::resort` and the grids.
//
// ge:summary The ski area's checks: R14 and R25 to R30 off the published area
// ge:keywords analysis analyze-resort
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/analysis/resort.hpp"
#include "mountain/analysis/routes.hpp"
#include "mountain/mapgen/access_build.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/hub.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::analysis {

namespace num = ge::core::num;
namespace R = mapgen::rules;
namespace RR = mapgen::resort_rules;
using ge::core::hypot;
using mapgen::Band;
using mapgen::Lift;
using mapgen::Run;
using mapgen::RunKind;
using mapgen::within_band;

namespace {

// R25 — how far the village may stand off the ground, m.
constexpr double kVillageSlack = 0.5;
// R26 — the least a lift climbs, m.
constexpr double kLiftClimb = 20;
// R27 — how far past a drift's fade the packed field may still read it, m: a cell either side.
constexpr double kDriftReach = 4;
// R14 — how far over the tree line a tree may stand, m (its trunk's ground, read bilinearly).
constexpr double kTreeLineSlack = 1;

// How far a plan point stands from a lift's line, m.
double off_lift(const Lift& l, double x, double z) {
  const double ex = l.top.x - l.bottom.x;
  const double ez = l.top.z - l.bottom.z;
  const double len2 = ex * ex + ez * ez;
  const double k = num::max(
      0.0, num::min(1.0, ((x - l.bottom.x) * ex + (z - l.bottom.z) * ez) / (len2 == 0 ? 1 : len2)));
  return hypot(x - (l.bottom.x + ex * k), z - (l.bottom.z + ez * k));
}

// A run's width at the nearest point a query found, read between the segment's two stations.
double width_at(const std::vector<mapgen::TrackPoint>& pts, const mapgen::TrackHit& hit) {
  const mapgen::TrackPoint& a = pts[size_t(hit.index)];
  if (size_t(hit.index) + 1 >= pts.size()) return a.width;
  const mapgen::TrackPoint& b = pts[size_t(hit.index) + 1];
  const double t = num::max(0.0, num::min(1.0, (hit.s - a.s) / num::max(1e-9, b.s - a.s)));
  return a.width + (b.width - a.width) * t;
}

int run_index(std::span<const Run> runs, const std::string& id) {
  for (size_t i = 0; i < runs.size(); i++) {
    if (runs[i].id == id) return int(i);
  }
  return -1;
}

}  // namespace

LevelAnalysis analyze_resort(const mapgen::Level& level) {
  LevelAnalysis out;
  const Add add = [&](const char* rule, Severity severity, std::string message) {
    out.findings.push_back({rule, severity, std::move(message)});
  };
  if (!level.resort) {
    add("R25", Severity::error, "no resort published");
    out.ok = false;
    return out;
  }
  const mapgen::ResortMap& area = *level.resort;
  const std::vector<Run>& runs = area.runs;
  // Every run's line as a piste the queries read.
  std::vector<mapgen::Track> lines(runs.size());
  for (size_t i = 0; i < runs.size(); i++) {
    lines[i].points = runs[i].points;
    lines[i].length = runs[i].length;
  }
  // How far inside `pad` m past a run's edge a plan point lies, m (the deepest over the runs);
  // nought or less where it lies outside every run's.
  const auto into_run = [&](double x, double z, double pad, bool pistes_only) {
    mapgen::TrackHit hit;
    double deepest = -pad;
    for (size_t i = 0; i < runs.size(); i++) {
      if (lines[i].points.size() < 2) continue;
      if (pistes_only && runs[i].kind != RunKind::piste) continue;
      nearest_within(lines[i], x, z, mapgen::kWidest / 2 + pad, hit);
      if (hit.distance == INFINITY) continue;
      deepest = num::max(deepest, width_at(lines[i].points, hit) / 2 + pad - hit.distance);
    }
    return deepest;
  };
  const auto on_run = [&](double x, double z, double pad, bool pistes_only) {
    return into_run(x, z, pad, pistes_only) > 0;
  };

  // R25 — the village on the valley floor.
  const mapgen::Vec3& v = area.village;
  if (std::abs(v.y - level.ground_at(v.x, v.z)) > kVillageSlack || v.z < area.floor_z - 1) {
    add("R25", Severity::error, "the village is not on the valley floor");
  }

  // R26 — the lifts climb, and their stations stand beside the pistes.
  for (const Lift& l : area.lifts) {
    if (l.top.y - l.bottom.y < kLiftClimb) {
      add("R26", Severity::error, l.id + " climbs only " + fmt(l.top.y - l.bottom.y, 0) + " m");
    }
    if (on_run(l.bottom.x, l.bottom.z, 0, true)) {
      add("R26", Severity::error, l.id + "'s bottom station stands on a piste");
    }
  }

  // R27 — every run: a line, a piste's colour, its ends named, packed along it.
  int pistes = 0;
  const double F = R::drift::fade;
  for (size_t i = 0; i < runs.size(); i++) {
    const Run& run = runs[i];
    if (run.kind == RunKind::piste) pistes++;
    if (run.points.size() < 2) {
      add("R27", Severity::error, "run " + run.id + " has no line");
      continue;
    }
    if (run.into && run_index(runs, run.into->run) < 0) {
      add("R27", Severity::error, "run " + run.id + " merges into a run that is not there");
    }
    if (run.branch && run_index(runs, run.branch->run) < 0) {
      add("R27", Severity::error, "run " + run.id + " leaves a run that is not there");
    }
    if (run.to && std::none_of(area.lifts.begin(), area.lifts.end(),
                               [&](const Lift& l) { return l.id == *run.to; })) {
      add("R27", Severity::error, "run " + run.id + " runs to a lift that is not there");
    }
    double low = 1;
    for (size_t k = 0; k < run.points.size(); k += 5) {
      const mapgen::TrackPoint& p = run.points[k];
      bool drifted = false;
      for (const mapgen::Drift& d : run.drifts) {
        drifted = drifted || (p.s > d.from - F - kDriftReach && p.s < d.to + F + kDriftReach);
      }
      if (!drifted) low = num::min(low, level.packed_at(p.x, p.z));
    }
    if (low < 0.98) {
      add("R27", Severity::error, "run " + run.id + "'s line is only " + fmt(low, 2) + " packed");
    }
    // The colour it is billed is the colour it measures; on a real face it may be billed the
    // colour its real piste is signed, within `least::billed` colours of what it measures (R25).
    if (run.kind == RunKind::piste) {
      const int into = run.into ? run_index(runs, run.into->run) : -1;
      const Run* onto = into >= 0 ? &runs[size_t(into)] : nullptr;
      const mapgen::PisteGrade measured =
          mapgen::run_colour(run.points, onto ? &onto->points : nullptr);
      const int off = std::abs(int(measured) - int(run.grade));
      if (off > (level.face ? RR::massif::real::least::billed : 0)) {
        add("R27", Severity::error,
            "run " + run.id + " is billed " + mapgen::grade_name(run.grade) + " and measures " +
                mapgen::grade_name(measured));
      }
    }
  }
  // A real face carries the runs its mountain gives it (R25).
  if (pistes < (level.face ? RR::massif::real::least::runs : RR::network::runs_min)) {
    add("R27", Severity::error, "only " + str(pistes) + " pistes");
  }

  // R28 — the courses.
  if (area.courses.empty()) add("R28", Severity::error, "no course down the mountain");
  for (const mapgen::Course& c : area.courses) {
    if (!within_band(c.length, RR::course::length)) {
      add("R28", Severity::error, "course " + c.id + " is " + fmt(c.length, 0) + " m");
    }
    for (const std::string& r : c.runs) {
      if (run_index(runs, r) < 0) {
        add("R28", Severity::error, "course " + c.id + " skis run " + r + ", which is not there");
      }
    }
  }

  // R29 — access: every piste skied again without a harder one, every lift reached.
  std::vector<mapgen::PlanRun> plan;
  plan.reserve(runs.size());
  for (const Run& run : runs) {
    std::optional<mapgen::RunJoin> into;
    if (run.into) into = mapgen::RunJoin{run_index(runs, run.into->run), run.into->s};
    std::optional<mapgen::RunSpec::Branch> branch;
    if (run.branch)
      branch = mapgen::RunSpec::Branch{run_index(runs, run.branch->run), run.branch->s};
    const bool road = run.kind == RunKind::road;
    plan.push_back({run.kind, road ? 0 : static_cast<int>(run.grade), run.from, &run.points,
                    run.length, into, run.into ? run.into->from : run.length, branch, run.to,
                    !run.into && !run.to});
  }
  std::vector<mapgen::PlanLift> lifts;
  lifts.reserve(area.lifts.size());
  for (const Lift& l : area.lifts) {
    lifts.push_back(
        {l.id, {l.bottom.x, l.bottom.z}, {l.top.x, l.top.z}, l.bottom.z >= area.floor_z - 1});
  }
  // On a real face the way home may run a colour harder (R25): its runs keep their own colours.
  const mapgen::PlanAccess access =
      mapgen::reckon_access(plan, lifts, level.face ? RR::massif::real::least::harder : 0);
  for (size_t i = 0; i < runs.size(); i++) {
    if (!access.ok[i]) {
      add("R29", Severity::error, "run " + runs[i].id + " cannot be skied again: " + access.why[i]);
    }
  }
  for (const std::string& id : access.orphans) {
    add("R29", Severity::error, "no skier reaches " + id + "'s bottom station");
  }
  // R29 — every run that reaches the floor finishes in the hub.
  for (const Run& run : runs) {
    if (run.into || run.to || run.points.size() < 2) continue;
    const mapgen::TrackPoint& last = run.points.back();
    const double off = mapgen::outside_hub(area.hub, last.x, last.z);
    if (off > 1) {
      add("R29", Severity::error, "run " + run.id + " ends " + fmt(off, 0) + " m outside the hub");
    }
  }

  // R30 — the two wind tunnels.
  if (area.tunnels.size() != 2) {
    add("R30", Severity::error, str(double(area.tunnels.size())) + " wind tunnels");
  }

  // R14 — the woods: off every run's corridor and every lift's line, under the tree line.
  const mapgen::Mountain& M = level.mountain;
  const double line_y = v.y + (M.tree_line - M.altitude);
  int on_runs = 0;
  double deepest = 0;
  int on_lifts = 0;
  int above = 0;
  for (const mapgen::TreeDef& t : level.trees) {
    const double in = into_run(t.x, t.z, R::forest::corridor, false);
    if (in > 0) {
      on_runs++;
      deepest = num::max(deepest, in);
    }
    for (const Lift& l : area.lifts) {
      if (off_lift(l, t.x, t.z) < RR::lift::clear - 0.5) {
        on_lifts++;
        break;
      }
    }
    if (t.y > line_y + kTreeLineSlack) above++;
  }
  if (on_runs > 0) {
    add("R14", Severity::error,
        str(on_runs) + " tree(s) stand on a run's corridor, the deepest " + fmt(deepest, 2) +
            " m in");
  }
  if (on_lifts > 0) add("R26", Severity::error, str(on_lifts) + " tree(s) stand on a lift's line");
  if (above > 0) add("R14", Severity::error, str(above) + " tree(s) stand above the tree line");

  // R42 — the ski routes.
  check_routes(level, add);

  for (const Finding& f : out.findings) out.ok = out.ok && f.severity != Severity::error;
  return out;
}

}  // namespace ge::mountain::analysis

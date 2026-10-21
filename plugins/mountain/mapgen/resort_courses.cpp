// The ski-area build's last stages: the woods grown by height round everything laid, the day
// dealt and the face turned to its sun, and the courses composed down the network.
//
// ge:summary Stages 7 to 9 of the ski-area build: the woods, the day and the courses
// ge:keywords resort-woods resort-courses woods day facing courses tree-line clear
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include <variant>
#include <vector>

#include "core/heightfield.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/courses.hpp"
#include "mountain/mapgen/forest.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/hub.hpp"
#include "mountain/mapgen/lift_chain.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/resort_woods.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/summit_ramps.hpp"
#include "mountain/mapgen/sun.hpp"

namespace ge::mountain::mapgen {

using ge::core::hypot;
using ge::core::sample_field;

namespace {

// The hour the face is turned to: the sun a little past noon, when most of a day's skiing is.
constexpr double kFacingHour = 12.5;

}  // namespace

StageResult resort_woods(ResortAttempt& a) {
  const double base_y = sample_field(a.ground, a.village.x, a.village.z);
  const double line_y = base_y + (a.plan.tree_line - a.plan.altitude);
  const WoodsRow& row = woods_row(a.region->id);
  std::vector<const SummitRamp*> ramps;
  for (const auto& [lift, off] : a.ramps) {
    for (const SummitRamp& r : off) ramps.push_back(&r);
  }
  NetHit hit;
  const double keep = resort_rules::lift::clear;
  ForestOptions options;
  options.clear = [&](double x, double z) {
    if (hub_clear(a.hub.hub, a.sub, x, z)) return true;
    for (const LiftPlan& l : a.lift_plans) {
      if (off_line(l, x, z) < keep) return true;
    }
    for (const StationPad& p : a.pads) {
      if (hypot(x - p.x, z - p.z) < p.r + keep) return true;
    }
    for (const SummitRamp* r : ramps) {
      if (off_ramp(*r, x, z) < 0) return true;
    }
    if (a.way && off_way(*a.way, x, z) < keep) return true;
    // Off every run's corridor, not only the nearest's (R14).
    return a.net.covers(x, z, rules::forest::corridor, hit);
  };
  options.cover = [&](double y) { return woods_at_depth(row, line_y - y); };
  options.tall = [&](double y) { return tall_at_depth(row, line_y - y); };
  std::vector<Kicker> kickers;
  std::vector<Cliff> cliffs;
  for (const BuiltRun& b : a.built) {
    kickers.insert(kickers.end(), b.kickers.begin(), b.kickers.end());
    cliffs.insert(cliffs.end(), b.drops.begin(), b.drops.end());
  }
  kickers.insert(kickers.end(), a.off_kickers.begin(), a.off_kickers.end());
  cliffs.insert(cliffs.end(), a.cliffs.begin(), a.cliffs.end());
  const Track no_track{};
  a.trees = grow_forest(a.rng, a.plan, a.ground, no_track, kickers, cliffs, line_y, &options);
  return std::nullopt;
}

StageResult resort_courses(ResortAttempt& a) {
  a.day = deal_sun(a.rng, a.region->sun.latitude, a.region->sun.day_of_year);
  a.facing = face_the_sun(a.sub, {kFacingHour, a.day.day_of_year, a.day.latitude});

  std::vector<const WalkedRun*> runs;
  runs.reserve(a.built.size());
  for (const BuiltRun& b : a.built) runs.push_back(&b.walked);
  for (size_t i = 0; i < a.built.size(); i++) {
    const WalkedRun& w = a.built[i].walked;
    // A course starts at a top station on a piste: a lane is no run down it.
    if (w.spec.kind == RunKind::road) continue;
    auto composed = compose_course(runs, int(i));
    if (!std::holds_alternative<CoursePlan>(composed)) continue;
    CoursePlan plan = std::move(std::get<CoursePlan>(composed));
    if (plan.length < resort_rules::course::length.min ||
        plan.length > resort_rules::course::length.max) {
      continue;
    }
    for (TrackPoint& p : plan.points) p.y = sample_field(a.ground, p.x, p.z);
    // Every kicker the course skies, named by its arc down the course.
    std::vector<Kicker> on_it;
    for (size_t r = 0; r < a.built.size(); r++) {
      for (const Kicker& k : a.built[r].kickers) {
        const double at = course_arc(plan, int(r), k.s.value_or(0));
        if (at < 0) continue;
        Kicker moved = k;
        moved.s = at;
        on_it.push_back(std::move(moved));
      }
    }
    const Track track{plan.points, plan.length, nullptr};
    Course course{w.spec.id,
                  piste_grade_of(steepest_span(track, on_it)),
                  {},
                  plan.length,
                  plan.points.front().y - plan.points.back().y};
    for (const CoursePiece& p : plan.pieces) {
      course.runs.push_back(a.built[size_t(p.run)].walked.spec.id);
    }
    a.courses.push_back(std::move(course));
    a.course_plans.push_back(std::move(plan));
  }
  if (a.courses.empty()) return "no course runs down the network to the village";
  return std::nullopt;
}

}  // namespace ge::mountain::mapgen

// The definitions behind resort_level.hpp.
//
// ge:summary The area published once, and a course's track, gates, features and day set on it
// ge:keywords resort-level set-course
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#include "mountain/mapgen/resort_level.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/heightfield.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/course_day.hpp"
#include "mountain/mapgen/course_gates.hpp"
#include "mountain/mapgen/courses.hpp"
#include "mountain/mapgen/massif.hpp"
#include "mountain/mapgen/park.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using ge::core::sample_field;

namespace {

// The salt a course's gates are hashed with, by its number.
constexpr uint32_t kGateSalt = 0x51a1e;

// The hash a course's gates are set off, by its number.
uint32_t gate_salt(const ResortAttempt& a, uint32_t number) {
  return a.sub ^ static_cast<uint32_t>(num::imul(number, kGateSalt));
}

// R27 — a built run as the map publishes it.
Run publish_run(const ResortAttempt& a, const BuiltRun& b) {
  const WalkedRun& w = b.walked;
  Run run{w.spec.id,   w.spec.kind,  b.grade,      w.track.points, w.track.length,
          w.spec.from, std::nullopt, std::nullopt, w.spec.to,      b.drifts};
  if (w.into) {
    run.into = Run::Into{a.built[size_t(w.into->run)].walked.spec.id, w.into->s, w.merge_start};
  }
  if (w.spec.branch) {
    run.branch = Run::Branch{a.built[size_t(w.spec.branch->run)].walked.spec.id, w.spec.branch->s};
  }
  return run;
}

}  // namespace

Level resort_base(ResortAttempt& a, int laps) {
  Level level;
  level.seed = a.seed;
  level.size = a.plan.size;
  level.cell = a.ground.cell;
  level.ground = std::move(a.ground);
  level.packed = std::move(a.packed);
  level.crust = std::move(a.crust);
  level.trees = std::move(a.trees);
  level.laps = laps;
  level.attempt = a.attempt;
  level.version = a.version;
  level.region = a.region->id;
  if (a.face) level.face = a.face->crop->id;

  ResortMap area;
  area.runs.reserve(a.built.size());
  for (const BuiltRun& b : a.built) area.runs.push_back(publish_run(a, b));
  area.lifts.reserve(a.lift_plans.size());
  for (const LiftPlan& l : a.lift_plans) {
    Lift lift{l.id,
              l.kind,
              {l.bottom.x, sample_field(level.ground, l.bottom.x, l.bottom.z), l.bottom.z},
              {l.top.x, sample_field(level.ground, l.top.x, l.top.z), l.top.z},
              {}};
    const auto off = a.ramps.find(l.id);
    if (off != a.ramps.end()) lift.ramps = off->second;
    area.lifts.push_back(std::move(lift));
  }
  area.courses = a.courses;
  area.village = {a.village.x, sample_field(level.ground, a.village.x, a.village.z), a.village.z};
  area.floor_z = a.plan.base_z;
  area.hub = a.hub.hub;
  area.tunnels = a.tunnels;
  // The mountain's figures every course shares; its base and summit are the course's.
  const double sea = sea_level_of(a.plan, level.ground, area.village);
  level.mountain.vertical = a.plan.vertical;
  level.mountain.altitude = area.village.y - sea;
  level.mountain.tree_line = area.village.y - sea + (a.plan.tree_line - a.plan.altitude);
  if (a.plan.face) {
    // On a real face the floor's real height is the map's nought: the village stands its own
    // height over it, and the tree line is the real one.
    level.mountain.altitude = area.village.y + a.plan.altitude;
    level.mountain.tree_line = a.plan.tree_line;
  }
  level.mountain.sea = sea;
  level.resort = std::move(area);
  return level;
}

void set_course(Level& level, const ResortAttempt& a, size_t index) {
  const CoursePlan& plan = a.course_plans[index];
  const Course& course = a.courses[index];
  Track track;
  track.points = plan.points;
  for (TrackPoint& p : track.points) p.y = sample_field(level.ground, p.x, p.z);
  track.length = plan.length;

  // The kickers, drops and drifts the course skis, by its arc; every other run's, by its run.
  std::vector<Kicker> on_course;
  std::vector<Kicker> others;
  std::vector<Cliff> drops;
  std::vector<Cliff> cliffs;
  std::vector<Drift> drifts;
  for (size_t r = 0; r < a.built.size(); r++) {
    const BuiltRun& b = a.built[r];
    const std::string& run = b.walked.spec.id;
    for (size_t n = 0; n < b.kickers.size(); n++) {
      Kicker k = b.kickers[n];
      const double at = course_arc(plan, int(r), k.s.value_or(0));
      if (at >= 0) {
        k.s = at;
        k.on_track = true;
        on_course.push_back(std::move(k));
      } else {
        k.id = run + "K" + std::to_string(n + 1);
        k.on_track = false;
        k.s = std::nullopt;
        others.push_back(std::move(k));
      }
    }
    for (size_t n = 0; n < b.drops.size(); n++) {
      Cliff d = b.drops[n];
      const double at = course_arc(plan, int(r), d.s.value_or(0));
      if (at >= 0) {
        d.s = at;
        d.on_track = true;
        drops.push_back(std::move(d));
      } else {
        d.id = run + "D" + std::to_string(n + 1);
        d.on_track = false;
        d.s = std::nullopt;
        cliffs.push_back(std::move(d));
      }
    }
  }
  // A drift down a stretch the course skis, cut to that stretch.
  for (const CoursePiece& p : plan.pieces) {
    for (const Drift& d : a.built[size_t(p.run)].drifts) {
      const double from = num::max(d.from, p.from);
      const double to = num::min(d.to, p.to);
      if (to > from) drifts.push_back({course_arc(plan, p.run, from), course_arc(plan, p.run, to)});
    }
  }
  const auto by_arc = [](const auto& p, const auto& q) {
    return p.s.value_or(0) < q.s.value_or(0);
  };
  std::stable_sort(on_course.begin(), on_course.end(), by_arc);
  std::stable_sort(drops.begin(), drops.end(), by_arc);
  std::stable_sort(drifts.begin(), drifts.end(),
                   [](const Drift& p, const Drift& q) { return p.from < q.from; });
  for (size_t n = 0; n < on_course.size(); n++) on_course[n].id = "K" + std::to_string(n + 1);
  for (size_t n = 0; n < drops.size(); n++) drops[n].id = "D" + std::to_string(n + 1);

  const uint32_t number = static_cast<uint32_t>(std::stoi(course.id));
  level.checkpoints = course_gates(track, drops, on_course, course.grade, gate_salt(a, number));
  const CourseDay day = course_day(a.sub, *a.region, a.day.latitude, a.facing, course.id);

  const TrackPoint& last = track.points.back();
  const Vec3 base{last.x, last.y, last.z};
  const double peak_x = a.plan.massif ? a.plan.massif->peak_x : 0;
  level.mountain.summit = {peak_x, base.y + a.plan.vertical, a.plan.summit_z};
  level.mountain.base = base;
  level.track = std::move(track);
  grid_on_track(level.track, level.spawn, level.grid);
  level.kickers = std::move(on_course);
  level.kickers.insert(level.kickers.end(), others.begin(), others.end());
  level.kickers.insert(level.kickers.end(), a.off_kickers.begin(), a.off_kickers.end());
  level.kickers.insert(level.kickers.end(), a.route_kickers.begin(), a.route_kickers.end());
  level.cliffs = std::move(drops);
  level.cliffs.insert(level.cliffs.end(), cliffs.begin(), cliffs.end());
  level.cliffs.insert(level.cliffs.end(), a.cliffs.begin(), a.cliffs.end());
  level.drifts = std::move(drifts);
  level.sun = day.sun;
  level.weather = day.weather;
  level.grade = course.grade;
  level.resort->course = course.id;
}

std::optional<std::string> set_parked_course(Level& level, const ResortAttempt& a, size_t index) {
  set_course(level, a, index);
  const Course& course = a.courses[index];
  const uint32_t salt = gate_salt(a, static_cast<uint32_t>(std::stoi(course.id)));
  std::vector<Kicker> on_course;
  for (const Kicker& k : level.kickers) {
    if (k.on_track) on_course.push_back(k);
  }
  std::vector<Cliff> drops;
  for (const Cliff& d : level.cliffs) {
    if (d.on_track) drops.push_back(d);
  }
  const auto gates_for = [&](std::span<const Kicker> park) {
    return course_gates(level.track, drops, on_course, course.grade, salt, park);
  };
  // Planned off the start gate and the finish alone, each gap from a run-out to the next ramp one
  // gate's slot (a gate between them) or room for the gates' own spacing.
  const Band spacing = resort_rules::course::gates::spacing;
  ParkAsk ask;
  ask.gates = {start_gate_arc(), level.track.length};
  for (const Drift& d : level.drifts) {
    ask.avoid.emplace_back(d.from - rules::drift::fade, d.to + rules::drift::fade);
  }
  ask.slot = rules::trick::gate_before + rules::trick::gate_after + 2;
  ask.spread = 14;
  ask.far = ask.slot + spacing.min + 8;
  ParkPlan plan = plan_park(level.track, on_course, drops, ask);
  if (plan.field.empty()) return plan.why;
  std::vector<Kicker> field = std::move(plan.field);
  // The gates set round the park; a kicker they stand on, crowd or leave without a run-in, or
  // whose stretch they cannot space, dropped, until they stand.
  for (int round = 0; round < 8 && double(field.size()) >= rules::trick::count.min; round++) {
    std::vector<Checkpoint> gates = gates_for(field);
    std::vector<uint8_t> bad(field.size(), 0);
    const auto span_of = [&](const Kicker& k) {
      const double s = k.s.value_or(0);
      return std::pair<double, double>{s - k.ramp - rules::trick::gate_before,
                                       s + k.landing + rules::trick::gate_after};
    };
    for (size_t f = 0; f < field.size(); f++) {
      const auto [from, to] = span_of(field[f]);
      const double foot = field[f].s.value_or(0) - field[f].ramp;
      const Checkpoint* before = nullptr;
      for (const Checkpoint& g : gates) {
        if (g.s > from + 1 && g.s < to - 1) bad[f] = 1;
        if (g.s <= from + 1 && (before == nullptr || g.s > before->s)) before = &g;
      }
      if (before != nullptr && before->y - track_point_at(level.track, foot).y <
                                   rules::trick::run_in * field[f].height) {
        bad[f] = 1;
      }
    }
    for (size_t i = 0; i + 1 < gates.size(); i++) {
      const double ga = gates[i].s;
      const double gb = gates[i + 1].s;
      if (gb - ga >= spacing.min - 0.5 && gb - ga <= spacing.max + 0.5) continue;
      // The last gap runs out to the finish and may be short.
      if (i + 2 == gates.size() && gb - ga <= spacing.max + 0.5) continue;
      for (size_t f = 0; f < field.size(); f++) {
        const auto [from, to] = span_of(field[f]);
        if (from < gb + spacing.max && to > ga - spacing.max) bad[f] = 1;
      }
    }
    if (std::find(bad.begin(), bad.end(), 1) == bad.end()) {
      level.checkpoints = std::move(gates);
      for (size_t f = 0; f < field.size(); f++) field[f].id = "T" + std::to_string(f + 1);
      stamp_park(level, std::move(field));
      return std::nullopt;
    }
    std::vector<Kicker> kept;
    for (size_t f = 0; f < field.size(); f++) {
      if (bad[f] == 0) kept.push_back(std::move(field[f]));
    }
    field = std::move(kept);
  }
  return "only " + std::to_string(field.size()) +
         " park kicker(s) stand among the course's gates (R20)";
}

}  // namespace ge::mountain::mapgen

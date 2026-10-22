// The definitions behind generate.hpp: the sub-seeds, one attempt, the search over attempts
// (single piste), and the compile that bakes the grids.
//
// ge:summary One attempt's fixed draw order, the search over attempts and the compile
// ge:keywords generate
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/generate.hpp"

#include <cmath>
#include <variant>

#include "core/heightfield.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/output.hpp"
#include "core/prng.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/analysis/resort.hpp"
#include "mountain/mapgen/cliffs.hpp"
#include "mountain/mapgen/course_gates.hpp"
#include "mountain/mapgen/drift.hpp"
#include "mountain/mapgen/drops.hpp"
#include "mountain/mapgen/forest.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/park.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/resort_level.hpp"
#include "mountain/mapgen/route_features.hpp"
#include "mountain/mapgen/ski_routes.hpp"
#include "mountain/mapgen/spawn.hpp"
#include "mountain/mapgen/sun.hpp"
#include "mountain/mapgen/surface.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/track.hpp"
#include "mountain/mapgen/versions.hpp"
#include "mountain/mapgen/weather.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using ge::core::sample_field;

namespace {

// How many pistes an attempt walks before it gives up on its mountain.
constexpr int kDraws = 40;

// One attempt: a level, or the reason this sub-seed could not make one.
std::variant<Level, std::string> attempt_level(double seed, int attempt, int laps, int version,
                                               const Region& region, const GradeRow& grade,
                                               bool tricks) {
  const uint32_t sub = sub_seed(seed, attempt);
  ge::core::Rng rng = ge::core::make_rng(sub);
  const TerrainPlan plan = plan_terrain(rng, region, grade);
  ge::core::Heightfield ground = bake_country(plan);

  auto chosen = choose_start(rng, ground, grade);
  if (auto* why = std::get_if<std::string>(&chosen)) return std::move(*why);
  const Spawn start = std::get<Spawn>(chosen);
  std::optional<Piste> found;
  std::string why;
  const std::optional<double> colour = !grade.id || *grade.id == PisteGrade::black
                                           ? std::nullopt
                                           : std::optional<double>(grade.steepest.max);
  for (int d = 0; d < kDraws && !found; d++) {
    auto drawn = draw_piste(rng, plan, ground, start);
    if (auto* refused = std::get_if<std::string>(&drawn)) {
      why = std::move(*refused);
      continue;
    }
    Piste& piste = std::get<Piste>(drawn);
    if (auto graded = grade_piste(piste, ground, grade.track.max_grade, colour)) {
      why = std::move(*graded);
      continue;
    }
    found = std::move(piste);
  }
  if (!found) return "no piste fits this mountain (last: " + why + ")";
  Piste& piste = *found;

  const std::vector<TrackDrop> track_drops = lay_drops(sub, piste.track, grade);
  if (double(track_drops.size()) < grade.drops.min) {
    return "only " + num::to_string(double(track_drops.size())) + " drop(s) fit the piste (R24)";
  }
  const std::vector<TrackKicker> track_kickers =
      lay_track_kickers(rng, piste.track, grade, track_drops);
  std::vector<Cliff> drops = publish_drops(piste.track, track_drops);
  Corridor corridor = stamp_corridor(piste, ground);
  std::vector<Kicker> off_kickers = lay_off_kickers(rng, plan, ground, [&](double x, double z) {
    return nearest_track_point(piste.track, x, z).distance;
  });
  std::vector<Cliff> cliffs = lay_cliffs(sub, plan, ground, piste.track, off_kickers, drops);

  // Publish the heights the ground actually carries.
  for (TrackPoint& p : piste.track.points) p.y = sample_field(ground, p.x, p.z);
  std::vector<Kicker> kickers = publish_track_kickers(piste.track, track_kickers);
  kickers.insert(kickers.end(), off_kickers.begin(), off_kickers.end());
  for (Kicker& k : kickers) k.y = sample_field(ground, k.x, k.z);
  for (Cliff& d : drops) d.y = sample_field(ground, d.x, d.z);
  // R20 — the park is planned here, so the drifts keep off it, and stamped last, onto the
  // finished mountain. An attempt whose piste carries none is no tricks map.
  std::vector<Kicker> field;
  if (tricks) {
    ParkAsk ask;
    for (const Checkpoint& c : lay_checkpoints(piste.track)) ask.gates.push_back(c.s);
    ParkPlan planned = plan_park(piste.track, kickers, drops, ask);
    if (planned.field.empty()) return planned.why;
    field = std::move(planned.field);
  }
  std::vector<Kicker> clear_of = kickers;
  clear_of.insert(clear_of.end(), field.begin(), field.end());
  std::vector<Drift> drifts = deal_drifts(sub, piste.track.length, clear_of, grade.drift, drops);
  stamp_drifts(corridor.packed, corridor.near, corridor.along, drifts, rules::track::step);
  std::optional<ge::core::Heightfield> crust = lay_crust(sub, region, ground);
  if (crust) fold_surface(corridor.packed, corridor.dist, region, *crust);

  // THE MOUNTAIN as published (R2): the base at the finish line, the summit the profile's own
  // height above it at the head of the fall line.
  const TrackPoint& last = piste.track.points.back();
  const Vec3 base{last.x, sample_field(ground, last.x, last.z), last.z};
  const Mountain mountain{{start.x, base.y + plan.vertical, plan.summit_z},
                          base,
                          plan.vertical,
                          plan.altitude,
                          plan.tree_line,
                          base.y - plan.altitude};
  const double tree_line_y = base.y + (plan.tree_line - plan.altitude);
  std::vector<Cliff> edges = drops;
  edges.insert(edges.end(), cliffs.begin(), cliffs.end());
  std::vector<TreeDef> trees =
      grow_forest(rng, plan, ground, piste.track, kickers, edges, tree_line_y);
  const DealtSun day = deal_sun(rng, region.sun.latitude, region.sun.day_of_year);
  // R15 — the one piste's face is due north, so no `facing` is dealt.
  const Sun dealt{day.hour, day.day_of_year, day.latitude, std::nullopt};
  const DealtWeather sky = deal_weather(sub, dealt);

  // THE COMPILE: the parts bound into the Level every reader codes against.
  Level level;
  level.seed = seed;
  level.size = rules::world::size;
  level.cell = ground.cell;
  level.ground = std::move(ground);
  level.track = std::move(piste.track);
  level.checkpoints = lay_checkpoints(level.track);
  grid_on_track(level.track, level.spawn, level.grid);
  level.trees = std::move(trees);
  level.sun = Sun{sky.hour, day.day_of_year, day.latitude, std::nullopt};
  level.laps = laps;
  level.packed = std::move(corridor.packed);
  level.kickers = std::move(kickers);
  level.cliffs = std::move(edges);
  level.mountain = mountain;
  level.attempt = attempt;
  level.drifts = std::move(drifts);
  level.weather = sky.weather;
  level.version = version;
  level.region = region.id;
  level.grade = grade.id;
  level.crust = std::move(crust);
  if (!field.empty()) stamp_park(level, std::move(field));
  return level;
}

std::string join(const std::vector<std::string>& parts, const char* sep) {
  std::string out;
  for (size_t i = 0; i < parts.size(); i++) {
    if (i > 0) out += sep;
    out += parts[i];
  }
  return out;
}

// The errors among an analysis's findings, as one line.
std::string errors_of(const analysis::LevelAnalysis& checked) {
  std::vector<std::string> errors;
  for (const analysis::Finding& f : checked.findings) {
    if (f.severity == analysis::Severity::error) errors.push_back(f.rule + " " + f.message);
  }
  return join(errors, ", ");
}

// R25–R28 — one attempt at a ski area raced as a map: the area built, its first course's map
// stood up and the area held to its rules on it, then every course held on its own map (one that
// will not stand is not offered), and the course asked for set on the map, written to `level`.
// Why the attempt is refused, or nothing.
// R42 — the ski routes of a ski area's base map, found on the built mountain before the area
// is analysed, so the rule book holds the map they are laid on: v9's the shortest line down;
// v10's wild, with the trunks on them felled and their jumps cut into the ground (every course
// set on the base then carries the jumps, `O1…`). Nothing draws from a stream.
void lay_routes(Level& level, ResortAttempt& a, int version) {
  if (!level.resort) return;
  ResortMap& area = *level.resort;
  const bool wild = !generator_traits(version).straight_routes;
  std::vector<SkiRoute> routes = lay_ski_routes(
      {level.ground, area.runs, area.lifts, level.trees, a.cliffs, level.size}, wild);
  if (wild) {
    a.route_kickers = lay_route_jumps(level.ground, routes);
    level.trees = fell_on_routes(level.trees, routes, a.route_kickers);
    for (TreeDef& t : level.trees) t.y = sample_field(level.ground, t.x, t.z);
  }
  area.routes = std::move(routes);
}

std::optional<std::string> attempt_resort(double seed, int attempt, int laps, int version,
                                          const Region& region, const GenerateOptions& opts,
                                          const std::string& name, Level& level) {
  ResortAttempt a = begin_resort(seed, attempt, sub_seed(seed, attempt), region, version);
  std::string reached;
  if (StageResult why = build_resort(a, reached)) return why;
  level = resort_base(a, laps);
  if (!generator_traits(version).no_routes) lay_routes(level, a, version);
  set_course(level, a, 0);
  const analysis::LevelAnalysis area = analysis::analyze_resort(level);
  if (!area.ok) return errors_of(area);
  std::vector<CoursePlan> plans;
  std::vector<Course> courses;
  for (size_t i = 0; i < a.courses.size(); i++) {
    if (i > 0) set_course(level, a, i);
    const analysis::LevelAnalysis checked = analysis::analyze_course(level);
    if (!checked.ok) {
      ge::core::output::debug(name + ": course " + a.courses[i].id + " left out — " +
                              errors_of(checked));
      continue;
    }
    plans.push_back(std::move(a.course_plans[i]));
    courses.push_back(std::move(a.courses[i]));
  }
  if (courses.empty()) return "no course down the network stands";
  a.course_plans = std::move(plans);
  a.courses = std::move(courses);
  if (level.resort) level.resort->courses = a.courses;
  const size_t index = choose_course(a.courses, seed, opts.course, opts.grade, deal_grade(seed));
  if (opts.tricks) {
    // R20 — the course asked for, or the one chosen and, where that carries no park that
    // stands, the first of the others that does. A ski area none of whose courses carries one
    // is no tricks map, unless a course was asked for: that one is then ridden without a park.
    std::vector<size_t> order{index};
    for (size_t i = 0; !opts.course && i < a.courses.size(); i++) {
      if (i != index) order.push_back(i);
    }
    for (const size_t i : order) {
      Level parked = level;
      std::optional<std::string> why = set_parked_course(parked, a, i);
      if (!why) {
        const analysis::LevelAnalysis checked = analysis::analyze_course(parked);
        if (checked.ok) {
          level = std::move(parked);
          return std::nullopt;
        }
        why = errors_of(checked);
      }
      ge::core::output::debug(name + ": course " + a.courses[i].id + " carries no park — " + *why);
    }
    if (!opts.course) return "no course carries a park (R20)";
  }
  set_course(level, a, index);
  return std::nullopt;
}

}  // namespace

uint32_t sub_seed(double seed, int attempt) {
  return num::to_uint32(seed + attempt * 2654435769.0);
}

void Level::normal_at(double x, double z, Vec3& out) const {
  double g[3];
  ge::core::sample_field_gradient(ground, x, z, g);
  const double nx = -g[1];
  const double nz = -g[2];
  const double inv = 1 / std::sqrt(nx * nx + 1 + nz * nz);
  out.x = nx * inv;
  out.y = inv;
  out.z = nz * inv;
}

Level generate_level(double seed, const GenerateOptions& opts) {
  const int attempts = opts.attempts.value_or(16);
  const int laps = opts.laps.value_or(rules::race::laps);
  const GeneratorTraits& traits = generator_traits(opts.version);
  const std::string name = "level " + num::to_string(seed);
  const Region& region = region_row(opts.region);
  const GradeRow& grade = kUngraded;
  std::vector<std::string> reasons;
  const int first = opts.only ? *opts.only : 0;
  const int end =
      opts.only ? (*opts.only >= 0 && *opts.only < attempts ? *opts.only + 1 : 0) : attempts;
  if (!traits.single_piste) {
    for (int a = first; a < end; a++) {
      Level level;
      if (const auto why =
              attempt_resort(seed, a, laps, traits.version, region, opts, name, level)) {
        reasons.push_back("#" + std::to_string(a) + ": " + *why);
        continue;
      }
      if (!reasons.empty()) {
        ge::core::output::debug(name + ": accepted attempt " + std::to_string(a) + " after " +
                                join(reasons, "; "));
      }
      return level;
    }
    throw GenerateError(name + ": no clean ski area in " + std::to_string(attempts) +
                        " attempts — " + join(reasons, "; "));
  }
  for (int a = first; a < end; a++) {
    auto built = attempt_level(seed, a, laps, traits.version, region, grade, opts.tricks);
    if (auto* why = std::get_if<std::string>(&built)) {
      reasons.push_back("#" + std::to_string(a) + ": " + *why);
      continue;
    }
    Level& level = std::get<Level>(built);
    const analysis::LevelAnalysis checked = analysis::analyze_level(level);
    if (checked.ok) {
      if (!reasons.empty()) {
        ge::core::output::debug(name + ": accepted attempt " + std::to_string(a) + " after " +
                                join(reasons, "; "));
      }
      return std::move(level);
    }
    reasons.push_back("#" + std::to_string(a) + ": " + errors_of(checked));
  }
  throw GenerateError(name + ": no clean map in " + std::to_string(attempts) + " attempts — " +
                      join(reasons, "; "));
}

}  // namespace ge::mountain::mapgen

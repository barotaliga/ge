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
#include "mountain/mapgen/cliffs.hpp"
#include "mountain/mapgen/drift.hpp"
#include "mountain/mapgen/drops.hpp"
#include "mountain/mapgen/forest.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/regions.hpp"
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
                                               const Region& region, const GradeRow& grade) {
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

  const std::vector<TrackDrop> track_drops = lay_drops(sub, piste, grade);
  if (double(track_drops.size()) < grade.drops.min) {
    return "only " + num::to_string(double(track_drops.size())) + " drop(s) fit the piste (R24)";
  }
  const std::vector<TrackKicker> track_kickers = lay_track_kickers(rng, piste, grade, track_drops);
  std::vector<Cliff> drops = publish_drops(piste, track_drops);
  Corridor corridor = stamp_corridor(piste, ground);
  std::vector<Kicker> off_kickers = lay_off_kickers(rng, plan, ground, piste);
  std::vector<Cliff> cliffs = lay_cliffs(sub, plan, ground, piste.track, off_kickers, drops);

  // Publish the heights the ground actually carries.
  for (TrackPoint& p : piste.track.points) p.y = sample_field(ground, p.x, p.z);
  std::vector<Kicker> kickers = publish_track_kickers(piste, track_kickers);
  kickers.insert(kickers.end(), off_kickers.begin(), off_kickers.end());
  for (Kicker& k : kickers) k.y = sample_field(ground, k.x, k.z);
  for (Cliff& d : drops) d.y = sample_field(ground, d.x, d.z);
  std::vector<Drift> drifts = deal_drifts(sub, piste.track.length, kickers, grade.drift, drops);
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
  if (!traits.single_piste) {
    throw GenerateError(name + ": generator version " + std::to_string(traits.version) +
                        " builds a ski area, which is not built yet (docs/goals.md M4b). " +
                        "Ask for version 1 (GenerateOptions::version = 1), or build the ski " +
                        "area first.");
  }
  const Region& region = region_row(opts.region);
  const GradeRow& grade = kUngraded;
  std::vector<std::string> reasons;
  const int first = opts.only ? *opts.only : 0;
  const int end =
      opts.only ? (*opts.only >= 0 && *opts.only < attempts ? *opts.only + 1 : 0) : attempts;
  for (int a = first; a < end; a++) {
    auto built = attempt_level(seed, a, laps, traits.version, region, grade);
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
    std::vector<std::string> errors;
    for (const analysis::Finding& f : checked.findings) {
      if (f.severity == analysis::Severity::error) errors.push_back(f.rule + " " + f.message);
    }
    reasons.push_back("#" + std::to_string(a) + ": " + join(errors, ", "));
  }
  throw GenerateError(name + ": no clean map in " + std::to_string(attempts) + " attempts — " +
                      join(reasons, "; "));
}

}  // namespace ge::mountain::mapgen

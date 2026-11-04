// plugins/mountain/tarn/reservoir.hpp: a dealt map's valley holds a reservoir behind an earth dam.
// The water lies over ground the flood found under it, clear of the ski area, between the least
// and the most a reservoir holds; the dam's crest stands over the water by its freeboard and its
// ends run into the valley's sides; frozen to bear, the shore is filled flat. The same map finds
// the same reservoir.
//
// ge:summary Test: the valley's reservoir on a dealt map: the site, the water, the dam, the ice
// ge:keywords test reservoir dam find_reservoir lay_reservoir flood crest freeboard
// ge:skill mountain testing
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/tarn/reservoir.hpp"
#include "mountain/tarn/tarn.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

const mg::Level& seed38() {
  static const std::unique_ptr<mg::Level> built =
      std::make_unique<mg::Level>(mg::generate_level(38, {}));
  return *built;
}

double ground(const mg::Level& level, double x, double z) {
  return ge::core::sample_field(level.ground, x, z);
}

// The ground's nodes inside the shore: how many, and how many stand over the water by 1 m.
struct Inside {
  int nodes = 0, dry = 0;
  double highest = -1e30;
};
Inside inside(const mg::Level& level, const mg::MapLake& lake) {
  Inside out;
  const ge::core::Heightfield& g = level.ground;
  for (int row = 0; row < g.rows; row += 2) {
    for (int col = 0; col < g.cols; col += 2) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      if (std::hypot(x - lake.x, z - lake.z) > 6 * lake.radius) continue;
      if (!tn::inside_shore(lake.shore, x, z)) continue;
      const double y =
          g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) + static_cast<size_t>(col)];
      ++out.nodes;
      if (y > lake.level + 1) ++out.dry;
      out.highest = std::max(out.highest, y);
    }
  }
  return out;
}

double to_shore(const std::vector<mg::PlanPoint>& s, double x, double z) {
  if (tn::inside_shore(s, x, z)) return 0;
  double best = 1e30;
  for (size_t i = 0, j = s.size() - 1; i < s.size(); j = i++) {
    const double ex = s[i].x - s[j].x, ez = s[i].z - s[j].z;
    const double t =
        std::clamp(((x - s[j].x) * ex + (z - s[j].z) * ez) / (ex * ex + ez * ez), 0.0, 1.0);
    best = std::min(best, std::hypot(x - s[j].x - t * ex, z - s[j].z - t * ez));
  }
  return best;
}

mg::Level with_reservoir(double day) {
  mg::Level level = seed38();
  level.sun.day_of_year = day;
  if (const auto r = tn::find_reservoir(level)) tn::lay_reservoir(level, *r);
  return level;
}

}  // namespace

TEST(a_dealt_map_s_valley_holds_a_reservoir_clear_of_the_ski_area) {
  const mg::Level& level = seed38();
  const std::optional<tn::Reservoir> r = tn::find_reservoir(level);
  CHECK_MSG(r.has_value(), "seed 38 has no valley a dam closes clear of its ski area");
  if (!r) return;
  const mg::MapLake& lake = r->lake;
  const double area = 3.141592653589793 * lake.radius * lake.radius;
  CHECK_MSG(area >= 0.8 * tn::kLeastReservoir && area <= 1.2 * tn::kMostReservoir,
            "its water " + std::to_string(area / 1e4) + " ha");
  CHECK(lake.shore.size() >= 4 && lake.depth > 2);
  for (const mg::PlanPoint& p : lake.shore) {
    CHECK(p.x >= 0 && p.z >= 0 && p.x <= level.size && p.z <= level.size);
  }
  // The ground inside the shore lies under the water, but for its outline's thinning.
  const Inside in = inside(level, lake);
  CHECK(in.nodes > 1000);
  CHECK_MSG(in.dry * 20 <= in.nodes,
            std::to_string(in.dry) + " of " + std::to_string(in.nodes) + " nodes dry inside");
  // Clear of every run and lift.
  if (level.resort) {
    double nearest = 1e30;
    for (const mg::Run& run : level.resort->runs) {
      for (size_t k = 0; k < run.points.size(); k += 8)
        nearest = std::min(nearest, to_shore(lake.shore, run.points[k].x, run.points[k].z));
    }
    CHECK_MSG(nearest >= tn::kDamClear - 2 * tn::kFloodCell,
              "a run passes " + std::to_string(nearest) + " m from the water");
  }
  // The dam stands over the water by its freeboard, across the valley's fall.
  const mg::MapDam& dam = r->dam;
  CHECK(std::fabs(dam.crest - lake.level - tn::kFreeboard) < 1e-9);
  CHECK(dam.height >= tn::kDamHeights.front() && dam.height <= tn::kDamHeights.back());
  // The same map finds the same reservoir.
  const auto again = tn::find_reservoir(level);
  CHECK(again && again->lake.level == lake.level && again->lake.shore.size() == lake.shore.size());
}

TEST(the_dam_holds_the_water_and_runs_into_the_valley_sides) {
  const mg::Level level = with_reservoir(200);  // midsummer: open water
  CHECK(level.dams.size() == 1 && level.lakes.size() == 1);
  if (level.dams.empty() || level.lakes.empty()) return;
  const mg::MapDam& dam = level.dams[0];
  const mg::MapLake& lake = level.lakes[static_cast<size_t>(dam.lake)];
  CHECK(lake.ice == 0 && lake.depth > 2);
  // Along its crest the ground stands at the crest or over it.
  int low = 0;
  for (int k = 0; k <= 40; ++k) {
    const double t = k / 40.0;
    const double x = dam.a.x + t * (dam.b.x - dam.a.x), z = dam.a.z + t * (dam.b.z - dam.a.z);
    if (ground(level, x, z) < dam.crest - 0.2) ++low;
  }
  CHECK_MSG(low == 0, std::to_string(low) + " points of the crest under it");
  // Its ends meet the valley's sides at its crest.
  CHECK(ground(seed38(), dam.a.x, dam.a.z) >= dam.crest - 1);
  CHECK(ground(seed38(), dam.b.x, dam.b.z) >= dam.crest - 1);
  // Downstream of the dam lies dry ground under the water's surface.
  const double mx = 0.5 * (dam.a.x + dam.b.x), mz = 0.5 * (dam.a.z + dam.b.z);
  const double below = ground(level, mx + dam.downhill.x * (8 + 2 * dam.height),
                              mz + dam.downhill.z * (8 + 2 * dam.height));
  CHECK(below < lake.level);
  CHECK(!tn::on_lake(lake, mx + dam.downhill.x * 40, mz + dam.downhill.z * 40));
  for (const mg::TreeDef& t : level.trees) CHECK(!tn::inside_shore(lake.shore, t.x, t.z));
}

TEST(ice_that_bears_fills_the_reservoir_flat) {
  const mg::Level level = with_reservoir(40);  // midwinter
  CHECK(level.lakes.size() == 1);
  if (level.lakes.empty()) return;
  const mg::MapLake& lake = level.lakes[0];
  CHECK_MSG(tn::bears(lake), "midwinter's ice on seed 38's reservoir does not bear a skier");
  CHECK(lake.depth == 0);
  const Inside in = inside(level, lake);
  int under = 0;
  const ge::core::Heightfield& g = level.ground;
  for (int row = 0; row < g.rows; row += 2) {
    for (int col = 0; col < g.cols; col += 2) {
      const double x = g.origin_x + col * g.cell, z = g.origin_z + row * g.cell;
      if (!tn::inside_shore(lake.shore, x, z)) continue;
      if (g.data[static_cast<size_t>(row) * static_cast<size_t>(g.cols) +
                 static_cast<size_t>(col)] < lake.level - 1e-3)
        ++under;
    }
  }
  CHECK(in.nodes > 1000);
  CHECK_MSG(under == 0, std::to_string(under) + " nodes inside the frozen shore under its ice");
}

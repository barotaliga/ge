// plugins/mountain/tarn/tarn.hpp: a tarn's site lies high on the mountain, flat and clear of every
// run and lift; the winter so far counts from the freeze-up and is none in summer; ice that bears
// a skier is filled flat, thinner ice lies over a bowl dug under the rim, the trees in it felled;
// the same map lays the same tarn.
//
// ge:summary Test: a tarn laid on a map: its site, the winter's days, the basin dug or filled
// ge:keywords test tarn find_tarn lay_tarn dig_basin winter_days tarn_ice bears on_lake
// ge:skill mountain testing
#include <algorithm>
#include <cmath>
#include <memory>

#include "check.hpp"
#include "core/heightfield.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/tarn/tarn.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

constexpr double kRadius = 60;

const mg::Level& map38() {
  static const std::unique_ptr<mg::Level> built = [] {
    mg::GenerateOptions opts;
    opts.version = 10;
    return std::make_unique<mg::Level>(mg::generate_level(38, opts));
  }();
  return *built;
}

double ground(const mg::Level& l, double x, double z) {
  return ge::core::sample_field(l.ground, x, z);
}

}  // namespace

TEST(the_site_is_high_flat_and_clear_of_the_runs_and_lifts) {
  const mg::Level& l = map38();
  const std::optional<mg::MapLake> lake = tn::find_tarn(l, kRadius);
  CHECK_MSG(lake.has_value(), "seed 38 found no tarn; see tarn.hpp's THE SITE");
  if (!lake) return;
  const double base = l.mountain.base.y, top = l.mountain.summit.y;
  const double y = ground(l, lake->x, lake->z);
  CHECK(y >= base + tn::kTarnLow * (top - base) && y <= base + tn::kTarnHigh * (top - base));
  CHECK(lake->radius == kRadius);
  // The surface is the shore's lowest ground: no point of the shore stands under it.
  for (const ge::water::Point& p : tn::shore_of(*lake))
    CHECK(ground(l, p.x, p.z) >= lake->level - 1e-6);
  // No circle: the shore swells and pinches, within its bulge.
  double near = 1e30, far = 0;
  for (int i = 0; i < 64; ++i) {
    const double r = tn::reach_of(*lake, 6.283185307179586 * i / 64);
    near = std::min(near, r);
    far = std::max(far, r);
  }
  CHECK(far - near > 0.15 * kRadius);
  CHECK(far <= tn::kTarnBulge * kRadius + 1e-9);
  const double reach = 2 * tn::kTarnBulge * kRadius + tn::kTarnClear;
  for (const mg::TrackPoint& p : l.track.points)
    CHECK(std::hypot(p.x - lake->x, p.z - lake->z) >= reach);
  if (l.resort) {
    for (const mg::Run& run : l.resort->runs) {
      for (const mg::TrackPoint& p : run.points)
        CHECK(std::hypot(p.x - lake->x, p.z - lake->z) >= reach);
    }
  }
  // The same map, the same site.
  const std::optional<mg::MapLake> again = tn::find_tarn(l, kRadius);
  CHECK(again && again->x == lake->x && again->z == lake->z && again->level == lake->level);
}

TEST(the_winter_counts_from_the_freeze_up_and_is_none_in_summer) {
  mg::Level l;
  l.sun = {12, 320 + 40, 46, std::nullopt};
  CHECK(tn::winter_days(l) == 40);
  l.sun.day_of_year = 30;  // late January: the winter since mid-November
  CHECK(tn::winter_days(l) == 75);
  l.sun.day_of_year = 200;  // July
  CHECK(tn::winter_days(l) == 0);
  l.sun.latitude = -40;  // the south's winter is the north's summer
  CHECK(tn::winter_days(l) == 64);
  l.sun.day_of_year = 30;
  CHECK(tn::winter_days(l) == 0);
}

TEST(ice_that_bears_a_skier_is_filled_flat) {
  CHECK(!tn::bears(0.03));
  CHECK(tn::bears(0.2));
  mg::Level l = map38();
  const std::optional<mg::MapLake> found = tn::find_tarn(l, kRadius);
  CHECK(found.has_value());
  if (!found) return;
  mg::MapLake lake = *found;
  lake.ice = 0.3;
  tn::lay_tarn(l, lake);
  CHECK(l.lakes.size() == 1);
  const mg::MapLake& laid = l.lakes[0];
  CHECK(laid.depth == 0);
  CHECK(laid.ice > 0);
  for (int i = -4; i <= 4; ++i) {
    const double x = laid.x + 0.2 * kRadius * i, z = laid.z + 0.1 * kRadius * i;
    CHECK(std::abs(ground(l, x, z) - laid.level) < 0.05);
    CHECK(tn::on_lake(laid, x, z));
  }
  CHECK(!tn::on_lake(laid, laid.x + 1.01 * tn::reach_of(laid, 0), laid.z));
  CHECK(tn::on_lake(laid, laid.x + 0.99 * tn::reach_of(laid, 0), laid.z));
  for (const mg::TreeDef& t : l.trees) CHECK(!tn::on_lake(laid, t.x, t.z));
}

TEST(thin_ice_lies_over_a_bowl_dug_under_the_rim) {
  mg::Level l = map38();
  const std::optional<mg::MapLake> found = tn::find_tarn(l, kRadius);
  CHECK(found.has_value());
  if (!found) return;
  mg::MapLake lake = *found;
  tn::dig_basin(l, lake, tn::kTarnDepth, false);
  CHECK(lake.depth > tn::kTarnDepth - 0.5 && lake.depth < tn::kTarnDepth + 1);
  // Deep in the middle, shelving up to the shore, and the bank stands over the water.
  CHECK(ground(l, lake.x, lake.z) < lake.level - tn::kTarnDepth + 0.5);
  const double east = tn::reach_of(lake, 0);
  CHECK(ground(l, lake.x + 0.9 * east, lake.z) > ground(l, lake.x + 0.2 * east, lake.z));
  for (int i = 0; i < 16; ++i) {
    const double a = 6.283185307179586 * i / 16, r = 1.1 * tn::reach_of(lake, a);
    CHECK(ground(l, lake.x + r * std::cos(a), lake.z + r * std::sin(a)) > lake.level);
  }
  const std::vector<ge::water::Point> shore = tn::shore_of(lake);
  CHECK(shore.size() == 64);
  CHECK(std::abs(std::hypot(shore[16].x - lake.x, shore[16].z - lake.z) -
                 tn::reach_of(lake, 6.283185307179586 / 4)) < 1e-9);
}

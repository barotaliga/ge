// plugins/classicenhance/climate.hpp: every level of the classic pack has its air, played in the
// evening or at night, in a wind, under low cloud; the lookup takes any case and the pack's path;
// an unknown map gets the fallback; the warm pools steam in the cold levels and the breath shows
// in the cold halls; the lava warms the air at its edge and not across a hall.
//
// ge:summary Test: the classic levels' air: every level, evening or night, windy, low cloud
// ge:keywords test classicenhance climate level evening night wind cloud steam breath lava
// ge:skill classicenhance testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "classicenhance/climate.hpp"
#include "wind/cold.hpp"
#include "wind/mist.hpp"

namespace ce = ge::classicenhance;
namespace gw = ge::wind;

TEST(every_level_is_evening_or_night_windy_and_under_low_cloud) {
  const char* kPack[] = {"start", "e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6", "e1m7", "e1m8"};
  CHECK(ce::level_airs().size() == sizeof kPack / sizeof kPack[0]);
  for (const char* map : kPack) {
    const ce::LevelAir* l = ce::level_air(map);
    CHECK_MSG(l != nullptr, std::string("no air for ") + map);
    if (l == nullptr) continue;
    CHECK(!l->what.empty());
    // Past sunset and before sunrise.
    CHECK(l->hour >= l->climate.sunset || l->hour < l->climate.sunrise);
    CHECK(l->climate.wind >= 7);
    CHECK(l->climate.cloud >= 0.8);
    CHECK(l->climate.mean_c > -30 && l->climate.mean_c < 30);
    CHECK(l->climate.humidity > 0 && l->climate.humidity <= 1);
  }
}

TEST(the_lookup_takes_any_case_and_the_packs_path) {
  CHECK(ce::level_air("E1M3") == ce::level_air("e1m3"));
  CHECK(ce::level_air("maps/e1m3.bsp") == ce::level_air("e1m3"));
  CHECK(ce::level_air("nowhere") == nullptr);
  const ce::LevelAir& f = ce::fallback_air();
  CHECK(f.hour >= f.climate.sunset || f.hour < f.climate.sunrise);
  CHECK(f.climate.wind >= 7 && f.climate.cloud >= 0.8);
}

TEST(the_wind_keeps_the_open_yards_clear_of_radiation_fog) {
  for (const ce::LevelAir& l : ce::level_airs()) {
    const gw::Mist m = gw::mist_at(l.climate, gw::Site{}, l.hour);
    CHECK_MSG(m.kind != gw::MistKind::radiation && m.kind != gw::MistKind::valley,
              std::string(l.map) + " lays radiation fog in a wind");
  }
}

TEST(warm_pools_steam_in_the_cold_levels_and_the_breath_shows_in_the_cold_halls) {
  gw::Site pool;
  pool.water_m = 0;
  pool.sky = 0;
  gw::Site hall;
  hall.sky = 0;
  int steaming = 0, breathing = 0;
  for (const ce::LevelAir& l : ce::level_airs()) {
    if (gw::mist_at(l.climate, pool, l.hour).kind == gw::MistKind::steam) ++steaming;
    const double breath = gw::breath_of(gw::air_at(l.climate, hall, l.hour));
    if (breath > 0.1) ++breathing;
    // The lava halls are warm: no breath.
    if (l.climate.mean_c > 12) CHECK(breath == 0);
  }
  CHECK(steaming >= 2);
  CHECK(breathing >= 5);
  const ce::LevelAir* vault = ce::level_air("e1m8");
  CHECK(vault != nullptr && gw::mist_at(vault->climate, pool, vault->hour).visibility < 200);
}

TEST(lava_warms_the_air_at_its_edge_and_not_across_a_hall) {
  CHECK(std::fabs(ce::lava_warmth(0) - ce::kLavaWarmth) < 1e-12);
  CHECK(ce::lava_warmth(ce::kLavaReach) < 0.4 * ce::kLavaWarmth);
  CHECK(ce::lava_warmth(40) < 0.02);
  CHECK(std::fabs(32 * ce::kMetresPerUnit - 1) < 1e-12);
}

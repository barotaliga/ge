// engine/wind/mist.hpp: a clear, still night near water lays radiation fog round 0 C that the
// morning burns off; a wind mixes it away and cloud keeps it from forming; a hollow lays a deeper
// valley fog; an overcast, saturated mountain is in its hill fog; water far warmer than the air
// steams, indoors too; moist air over a thawing snowfield lays advection fog in a breeze; the same
// saturation lays thicker fog at 5 C than at -15 C; fog below 0 C is freezing; a fog still forming
// (a breeze's last wisps) veils hardly anything, a formed one keeps its kind's range; and every
// number keeps its bits in tests/golden/wind_mist.txt.
//
// A deliberate change moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/wind_mist_test`, and put the `climate` lab's charts
// before and after in the PR.
//
// ge:summary Test: fog from the air: radiation, valley, hill, steam and advection fog; golden
// ge:keywords test fog mist radiation valley hill steam advection burn-off freezing golden
// ge:skill water testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "wind/mist.hpp"

namespace {

namespace gw = ge::wind;

constexpr const char* kGolden = "tests/golden/wind_mist.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

// A damp late-autumn night by a lake, clear and still.
gw::Climate damp_night() {
  gw::Climate c;
  c.mean_c = 1;
  c.range_c = 10;
  c.humidity = 0.8;
  c.cloud = 0.05;
  c.wind = 1;
  c.water_c = 4;
  return c;
}

gw::Site shore() {
  gw::Site s;
  s.water_m = 20;
  return s;
}

}  // namespace

TEST(a_clear_still_night_by_water_lays_radiation_fog_the_morning_burns_off) {
  const gw::Climate c = damp_night();
  const gw::Mist dawn = gw::mist_at(c, shore(), 5.5);
  CHECK(dawn.kind == gw::MistKind::radiation);
  CHECK(dawn.visibility < 400);
  CHECK(std::fabs(dawn.density - 3 / dawn.visibility) < 1e-12);
  CHECK(dawn.depth > 5 && dawn.depth < 60);
  CHECK(dawn.freezing);
  CHECK(gw::mist_at(c, shore(), 13).kind == gw::MistKind::none);
  CHECK(gw::mist_at(c, shore(), 13).visibility == 20000);
}

TEST(a_wind_mixes_it_away_and_cloud_keeps_it_from_forming) {
  gw::Climate c = damp_night();
  c.wind = 9;
  CHECK(gw::mist_at(c, shore(), 5.5).strength < 0.1);
  c = damp_night();
  c.cloud = 0.95;
  CHECK(gw::mist_at(c, shore(), 5.5).kind != gw::MistKind::radiation);
}

TEST(a_fog_still_forming_veils_hardly_anything) {
  gw::Climate c = damp_night();
  c.wind = 8;
  const gw::Mist wisps = gw::mist_at(c, shore(), 5.5);
  CHECK(wisps.kind != gw::MistKind::none && wisps.strength < gw::kFormed);
  CHECK(wisps.visibility > 5000 && wisps.visibility <= gw::kClearRange);
  // Formed, its range is its kind's (at full strength under 400 m).
  CHECK(gw::mist_at(damp_night(), shore(), 5.5).visibility < 400);
}

TEST(a_hollow_lays_a_deeper_valley_fog) {
  const gw::Climate c = damp_night();
  gw::Site valley = shore();
  valley.hollow = 300;
  const gw::Mist v = gw::mist_at(c, valley, 5.5), r = gw::mist_at(c, shore(), 5.5);
  CHECK(v.kind == gw::MistKind::valley);
  CHECK(v.depth > r.depth + 50);
}

TEST(an_overcast_saturated_mountain_is_in_its_hill_fog) {
  gw::Climate c = damp_night();
  c.cloud = 0.9;
  c.wind = 8;
  c.humidity = 0.9;
  gw::Site summit;
  summit.y = 1500;
  const gw::Mist m = gw::mist_at(c, summit, 14);
  CHECK(m.kind == gw::MistKind::hill);
  CHECK(m.visibility < 400);
  gw::Site foot;
  CHECK(gw::mist_at(c, foot, 14).kind == gw::MistKind::none);
}

TEST(water_far_warmer_than_the_air_steams_indoors_too) {
  gw::Climate c = damp_night();
  c.mean_c = -12;
  c.water_c = 4;
  c.wind = 7;
  c.cloud = 0.9;
  gw::Site pool;
  pool.water_m = 0;
  pool.sky = 0;
  const gw::Mist m = gw::mist_at(c, pool, 22);
  CHECK(m.kind == gw::MistKind::steam);
  CHECK(m.rise > 0 && m.noise > 0.8);
  CHECK(m.depth > 2 && m.depth <= 15);
  gw::Site inland = pool;
  inland.water_m = 300;
  CHECK(gw::mist_at(c, inland, 22).strength < gw::kNoMist);
}

TEST(moist_air_over_a_thawing_snowfield_lays_advection_fog_in_a_breeze) {
  gw::Climate c;
  c.mean_c = 6;
  c.humidity = 0.95;
  c.cloud = 0.6;
  c.wind = 4;
  gw::Site field;
  field.snow = 1;
  CHECK(gw::mist_at(c, field, 14).kind == gw::MistKind::advection);
  gw::Site bare;
  CHECK(gw::mist_at(c, bare, 14).kind != gw::MistKind::advection);
  c.wind = 20;
  CHECK(gw::mist_at(c, field, 14).kind != gw::MistKind::advection);
}

TEST(the_same_saturation_lays_thicker_fog_at_5_c_than_at_minus_15) {
  gw::Climate mild = damp_night(), bitter = damp_night();
  mild.mean_c = 9;
  bitter.mean_c = -11;
  mild.humidity = bitter.humidity = 0.97;
  const gw::Site field;
  const gw::Mist m = gw::mist_at(mild, field, 5.5), b = gw::mist_at(bitter, field, 5.5);
  CHECK(m.kind == gw::MistKind::radiation && b.kind == gw::MistKind::radiation);
  CHECK(m.visibility < b.visibility);
  CHECK(!m.freezing && b.freezing);
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  for (const double mean : {-12.0, 1.0, 9.0}) {
    for (const double wind : {0.5, 3.0, 9.0}) {
      gw::Climate c = damp_night();
      c.mean_c = mean;
      c.wind = wind;
      gw::Site s = shore();
      s.hollow = 40;
      s.snow = mean < 3 ? 1 : 0;
      for (const double h : {2.0, 6.0, 14.0, 21.0}) {
        const gw::Mist m = gw::mist_at(c, s, h);
        got.push_back({static_cast<uint64_t>(m.kind), ge::test::bits(m.strength),
                       ge::test::bits(m.density), ge::test::bits(m.depth),
                       static_cast<uint64_t>(m.freezing)});
      }
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# kind, strength, density, depth, freezing per climate, wind and hour "
           "(wind_mist_test)\n";
    for (const auto& row : got) {
      std::string line;
      for (const uint64_t word : row) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(word));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  CHECK_MSG(got == want, "the fog moved; if the move is meant, rewrite " + std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the climate "
                             "lab's charts before and after in the PR (skill: water)");
}

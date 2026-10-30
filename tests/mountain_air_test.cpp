// plugins/mountain/air/air.hpp: the wind never blows up the mountain, whatever bearing R19 dealt
// and however it veers, and the fold is continuous; its speed breathes about the dealt mean and
// its gust stays in 0..1; a sky that does not snow lets nothing fall; a fall stays in 0..1, a
// flurry comes and goes to nothing while a steady fall barely wavers; the visual range is 45 m at
// a fall of 1 and opens as its inverse square; and every number keeps its bits in
// tests/golden/mountain_air.txt.
//
// A deliberate change to the air moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/mountain_air_test`, and put the `precip` lab's table
// before and after in the PR.
//
// ge:summary Test: the wind and the fall at a moment: downhill, breath, squalls, visibility; golden
// ge:keywords test air wind fall downhill_from wind_at fall_at visibility golden
// ge:skill mountain testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "mountain/air/air.hpp"
#include "mountain/mapgen/weather.hpp"

namespace {

namespace ma = ge::mountain::air;
namespace mg = ge::mountain::mapgen;

constexpr const char* kGolden = "tests/golden/mountain_air.txt";

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

mg::Level sky(double seed, mg::WeatherKind kind, double snowfall, double wind, double from) {
  mg::Level l;
  l.seed = seed;
  l.weather.kind = kind;
  l.weather.snowfall = snowfall;
  l.weather.wind = wind;
  l.weather.wind_from = from;
  return l;
}

}  // namespace

TEST(the_wind_never_blows_up_the_mountain) {
  for (int b = 0; b < 72; ++b) {
    const mg::Level l = sky(7 + b, mg::WeatherKind::clear, 0, 8, b * 0.0872664626);
    for (int k = 0; k * 3.7 < 120; ++k) {
      const double t = k * 3.7;
      const ma::Wind w = ma::wind_at(l, t);
      CHECK(w.z >= -1e-9 * w.speed);
      CHECK(w.gust >= 0 && w.gust <= 1);
      CHECK(w.speed >= 8 * 0.5 && w.speed <= 8 * 1.5);
      CHECK(std::fabs(std::hypot(w.x, w.z) - w.speed) < 1e-9);
    }
  }
  // Continuous through the fold: either side of a bearing straight across the slope.
  const double across = 1.5707963267948966;
  CHECK(std::fabs(ma::downhill_from(across - 1e-6) - ma::downhill_from(across + 1e-6)) < 1e-5);
  // A wind already blowing down the mountain is left as it is.
  CHECK(ma::downhill_from(3.0) == 3.0);
}

TEST(a_dry_sky_lets_nothing_fall) {
  for (const mg::WeatherKind k : {mg::WeatherKind::clear, mg::WeatherKind::fair,
                                  mg::WeatherKind::overcast, mg::WeatherKind::fog}) {
    const ma::Fall f = ma::fall_at(sky(3, k, 0.8, 5, 0), 12);
    CHECK(f.fall == 0);
    CHECK(f.visibility == 20000);
  }
}

TEST(a_flurry_comes_and_goes_and_a_steady_fall_barely_wavers) {
  const mg::Level flurries = sky(11, mg::WeatherKind::flurries, 0.3, 4, 0);
  const mg::Level snow = sky(11, mg::WeatherKind::snow, 0.6, 4, 0);
  double flo = 1, fhi = 0, slo = 1, shi = 0;
  for (int k = 0; k * 1.3 < 1200; ++k) {
    const double t = k * 1.3;
    const double f = ma::fall_at(flurries, t).fall, s = ma::fall_at(snow, t).fall;
    CHECK(f >= 0 && f <= 1 && s >= 0 && s <= 1);
    flo = std::fmin(flo, f);
    fhi = std::fmax(fhi, f);
    slo = std::fmin(slo, s);
    shi = std::fmax(shi, s);
  }
  CHECK(flo < 0.06);  // nearly nothing at a lull
  CHECK(slo > 0.6 * 0.6 && shi < 0.6 * 1.4);
  CHECK(fhi - flo > shi - slo);
  // A storm's gusts lift snow off the ground: its fall never sits still, and stays in 0..1.
  const mg::Level storm = sky(5, mg::WeatherKind::storm, 0.9, 14, 2);
  for (int k = 0; k * 2.1 < 600; ++k) {
    const double t = k * 2.1;
    const ma::Fall f = ma::fall_at(storm, t);
    CHECK(f.fall >= 0 && f.fall <= 1);
    CHECK(f.visibility >= 45);
  }
}

TEST(the_visual_range_opens_as_the_fall_thins) {
  CHECK(ma::visibility_in(1) == 45);
  CHECK(ma::visibility_in(0.5) == 180);
  CHECK(ma::visibility_in(0) == 20000);
  CHECK(ma::visibility_in(0.01) == 20000);
}

// A storm picked by hand: laid off the map's seed (the same twice, none without thunder), its
// first cell over the mountain's middle `kOverhead` s in, where its gust front blows harder than
// the map's own wind and its rain falls as rain under a summer's freezing level, lowered under the
// downdraft; the placeless functions keep their bits.
TEST(a_storm_passes_over_the_middle_with_its_gusts_and_rain) {
  mg::Level l = sky(38, mg::WeatherKind::fair, 0, 4, 2.5);
  l.mountain.base = {0, 1000, 0};
  l.mountain.summit = {0, 2200, -3000};
  CHECK(ma::storm_of(l).cells == 0);
  mg::SkyPick pick;
  pick.thunder = 0.7;
  mg::with_sky(l, pick);
  CHECK(l.weather.snowline == 2200 + mg::kSummerFreezing);
  const ge::wind::Storm s = ma::storm_of(l), again = ma::storm_of(l);
  CHECK(s.cells > 1 && s.cells == again.cells);
  CHECK_BITS(s.cell[0].x, again.cell[0].x);
  const double mx = 0, mz = -1500;
  const ge::wind::CellAt c = ge::wind::cell_at(s, 0, ma::kOverhead);
  CHECK(c.stage == ge::wind::Stage::mature);
  CHECK(std::hypot(c.x - mx, c.z - mz) < 1);
  // The gust front's strongest air somewhere in the first quarter hour, over the map's own.
  double most = 0;
  for (int k = 0; k < 900; k += 5) most = std::fmax(most, ma::wind_at(l, s, k, mx, mz).speed);
  CHECK(most > 2 * 4 + 8);
  // Rain under the core, all of it rain at the base; the freezing level lowered there.
  const ma::Fall wet = ma::fall_at(l, s, ma::kOverhead, mx, 1000, mz);
  CHECK(wet.fall > 0.4 && wet.rain > 0.99);
  CHECK(wet.visibility < 20000);
  CHECK(ma::cooled_at(s, ma::kOverhead, mx, mz) > 0.5 * ma::kCooled * 0.7);
  CHECK(ma::cooled_at(s, ma::kOverhead, mx + 30000, mz) < 1);
  // Far from every cell, the storm adds next to nothing.
  const ma::Fall dry = ma::fall_at(l, s, ma::kOverhead, mx + 60000, 1000, mz);
  CHECK(dry.fall < 0.01);
  CHECK_BITS(ma::wind_at(l, s, 12, mx + 60000, mz).speed, ma::wind_at(l, 12).speed);
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  const mg::Level levels[] = {sky(38, mg::WeatherKind::snow, 0.55, 6, 1.2),
                              sky(7, mg::WeatherKind::storm, 0.9, 15, 4.0),
                              sky(1234, mg::WeatherKind::flurries, 0.25, 3, 5.5)};
  for (const mg::Level& l : levels) {
    for (const double t : {0.0, 9.25, 61.5, 487.0}) {
      const ma::Wind w = ma::wind_at(l, t);
      const ma::Fall f = ma::fall_at(l, t);
      got.push_back({ge::test::bits(w.x), ge::test::bits(w.z), ge::test::bits(w.speed),
                     ge::test::bits(w.gust), ge::test::bits(f.fall), ge::test::bits(f.visibility)});
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# per level and moment: wind x, z, speed, gust; fall, visibility (mountain_air_test)\n";
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
  CHECK_MSG(got == want, "the air moved; if the move is meant, rewrite " + std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the precip "
                             "lab's table before and after in the PR (skill: mountain)");
}

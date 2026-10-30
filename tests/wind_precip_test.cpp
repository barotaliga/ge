// engine/wind/precip.hpp: the rate climbs with the dial and meets the tiers' edges where stated;
// the tiers name a drizzle, a downpour, a cloudburst and a blizzard; the phase is snow above the
// freezing level and rain under its band; a drop's pace saturates near 9.6 m/s and a flake's is
// about 1; one sees less through snow than through rain at the same rate; the wet ground fills,
// drains and dries toward its steady state; snow covers and melts; lightning strikes only in heavy
// falls, as often as said; and every number keeps its bits in
// tests/golden/wind_precip.txt.
//
// A deliberate change moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/wind_precip_test`, and put the `precip` lab's tables
// before and after in the PR.
//
// ge:summary Test: snow, sleet and rain: rate, tiers, phase, fall speed, visibility, wet; golden
// ge:keywords test precip rain snow sleet tier rate phase drop visibility wet puddle golden
// ge:skill water testing
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "wind/precip.hpp"

namespace {

namespace gw = ge::wind;

constexpr const char* kGolden = "tests/golden/wind_precip.txt";

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

gw::Precip rain(double i) { return {i, 1}; }
gw::Precip snow(double i) { return {i, 0}; }

}  // namespace

TEST(the_rate_climbs_with_the_dial_and_meets_the_rain_tiers_edges) {
  CHECK(gw::rate_of(rain(0)) == 0);
  CHECK(gw::rate_of(rain(gw::kNoFall * 0.5)) == 0);
  double last = 0;
  for (int k = 1; k <= 50; ++k) {
    const double r = gw::rate_of(rain(k / 50.0));
    CHECK(r > last);
    last = r;
  }
  CHECK(std::fabs(gw::rate_of(rain(0.25)) - 2.5) < 0.01);
  CHECK(std::fabs(gw::rate_of(rain(0.8)) - 50) < 0.5);
  CHECK(gw::rate_of(rain(1)) > 140 && gw::rate_of(rain(1)) < 160);
  CHECK(std::fabs(gw::rate_of(snow(1)) - 10) < 0.01);
  // A mix lies between its ends.
  const double mid = gw::rate_of({0.6, 0.5});
  CHECK(mid > gw::rate_of(snow(0.6)) && mid < gw::rate_of(rain(0.6)));
}

TEST(the_tiers_name_a_drizzle_a_downpour_a_cloudburst_and_a_blizzard) {
  CHECK(gw::tier_of(rain(0), 0) == gw::Tier::none);
  CHECK(gw::tier_of(rain(0.1), 2) == gw::Tier::light);
  CHECK(gw::tier_of(rain(0.35), 2) == gw::Tier::moderate);
  CHECK(gw::tier_of(rain(0.6), 2) == gw::Tier::heavy);
  CHECK(gw::tier_of(rain(0.6), 18) == gw::Tier::storm);
  CHECK(gw::tier_of(rain(0.9), 0) == gw::Tier::storm);
  CHECK(gw::tier_of(snow(0.4), 2) == gw::Tier::light);
  CHECK(gw::tier_of(snow(0.7), 2) == gw::Tier::moderate);
  CHECK(gw::tier_of(snow(0.9), 5) == gw::Tier::heavy);
  CHECK(gw::tier_of(snow(0.9), 20) == gw::Tier::storm);
  CHECK(std::string(gw::tier_name(gw::Tier::storm)) == "storm");
}

TEST(snow_falls_above_the_freezing_level_and_rain_under_its_band) {
  CHECK(gw::rain_share_at(2000, 1500) == 0);
  CHECK(gw::rain_share_at(1500, 1500) == 0);
  CHECK(gw::rain_share_at(1500 - gw::kMeltBand, 1500) == 1);
  CHECK(gw::rain_share_at(0, 1500) == 1);
  const double half = gw::rain_share_at(1500 - 0.5 * gw::kMeltBand, 1500);
  CHECK(std::fabs(half - 0.5) < 1e-12);
}

TEST(a_drop_falls_near_ten_metres_a_second_and_a_flake_near_one) {
  CHECK(gw::drop_speed(0) == 0);
  CHECK(gw::drop_speed(0.5) > 1.5 && gw::drop_speed(0.5) < 3);
  CHECK(gw::drop_speed(2) > 6 && gw::drop_speed(2) < 7.5);
  CHECK(gw::drop_speed(6) > 9 && gw::drop_speed(6) < 9.65);
  CHECK(gw::median_drop(1) > 0.8 && gw::median_drop(1) < 1);
  CHECK(gw::median_drop(50) > gw::median_drop(5));
  CHECK(std::fabs(gw::fall_speed(snow(0.5)) - 1) < 1e-12);
  CHECK(gw::fall_speed(rain(0.5)) > 4);
  CHECK(gw::fall_speed({0.5, 0.5}) > 1 && gw::fall_speed({0.5, 0.5}) < gw::fall_speed(rain(0.5)));
}

TEST(snow_hides_more_than_rain_and_a_heavier_fall_more) {
  CHECK(gw::visibility_of(rain(0)) == 20000);
  CHECK(gw::visibility_of(rain(0.9)) < 1000);
  CHECK(gw::visibility_of(rain(0.1)) > 5000);
  // Past a flurry's scatter (which sees as far as a drizzle does), snow hides more.
  for (const double i : {0.5, 0.8}) {
    CHECK(gw::visibility_of(snow(i)) < gw::visibility_of(rain(i)));
  }
  for (const double i : {0.2, 0.5, 0.8}) {
    CHECK(gw::visibility_of(rain(i + 0.1)) < gw::visibility_of(rain(i)));
    CHECK(gw::visibility_of(snow(i + 0.1)) < gw::visibility_of(snow(i)));
  }
}

TEST(the_wet_ground_fills_drains_and_dries_toward_its_steady_state) {
  const double dry = gw::dry_rate(3, 0);
  gw::Wet w;
  // A light rain soaks the film inside a few minutes; an hour of it reaches its steady puddles.
  w = gw::step_wet(w, 2, dry, 600);
  CHECK(w.wet == 1);
  for (int k = 0; k < 24; ++k) w = gw::step_wet(w, 2, dry, 600);
  const gw::Wet steady = gw::wet_steady(2, dry);
  CHECK(std::fabs(w.water - steady.water) < 0.01);
  CHECK(steady.puddles > 0.1 && steady.puddles < 0.5);
  CHECK(gw::wet_steady(40, dry).puddles == 1);
  CHECK(gw::wet_steady(0, dry).wet == 0);
  // After the rain the puddles go first, then the film, faster in the sun and the wind.
  gw::Wet shade = w, sun = w;
  for (int k = 0; k < 6; ++k) {
    shade = gw::step_wet(shade, 0, gw::dry_rate(1, 0), 600);
    sun = gw::step_wet(sun, 0, gw::dry_rate(8, 1), 600);
  }
  CHECK(shade.puddles < w.puddles && shade.wet > 0);
  CHECK(sun.water < shade.water);
  // One long step lands where the many short ones do.
  const gw::Wet one = gw::step_wet({}, 5, dry, 3600);
  gw::Wet many;
  for (int k = 0; k < 60; ++k) many = gw::step_wet(many, 5, dry, 60);
  CHECK(std::fabs(one.water - many.water) < 1e-9);
}

TEST(snow_covers_what_is_open_to_it_and_the_rain_and_the_sun_take_it) {
  gw::Cover c;
  // An hour of a moderate snow (0.7 of the dial, 1.9 mm/h of water) lays near two centimetres.
  for (int k = 0; k < 60; ++k) c = gw::step_cover(c, snow(0.7), 0, 60);
  CHECK(c.depth > 15 && c.depth < 25);
  CHECK(c.cover > 0.9 && c.cover < 1);
  // Rain takes it faster than the sun does.
  gw::Cover rained = c, sunned = c;
  for (int k = 0; k < 30; ++k) {
    rained = gw::step_cover(rained, rain(0.5), 0, 60);
    sunned = gw::step_cover(sunned, {}, 1, 60);
  }
  CHECK(rained.depth < sunned.depth && sunned.depth < c.depth);
  CHECK(gw::step_cover({}, rain(0.9), 0, 3600).depth == 0);
}

TEST(lightning_strikes_only_in_heavy_falls_and_about_as_often_as_said) {
  CHECK(gw::strokes_per_minute(rain(0.3), 2) == 0);
  CHECK(gw::strokes_per_minute(rain(0.6), 2) > 0.9);
  CHECK(gw::strokes_per_minute(rain(1), 20) > 5);
  CHECK(gw::strokes_per_minute(snow(0.95), 20) < gw::strokes_per_minute(rain(0.95), 20));
  int strokes = 0;
  bool lit = false;
  double brightest = 0;
  for (int k = 0; k < 3600 * 50; ++k) {
    const double f = gw::flash_at(k * 0.02, rain(1), 20);
    brightest = std::max(brightest, f);
    if (f > 0.3 && !lit) ++strokes;
    lit = f > 0.3;
    CHECK(gw::flash_at(k * 0.02, rain(0.3), 20) == 0);
  }
  // Six a minute, each counted once for its main stroke and some for a bright return: an hour of
  // them is 360 strokes give or take what the draw gives.
  CHECK(strokes > 250 && strokes < 900);
  CHECK(brightest > 0.8 && brightest <= 1.5);
  // A pure function of the clock.
  CHECK(gw::flash_at(123.4, rain(1), 20) == gw::flash_at(123.4, rain(1), 20));
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  for (const double i : {0.05, 0.25, 0.5, 0.75, 1.0}) {
    for (const double r : {0.0, 0.4, 1.0}) {
      const gw::Precip p{i, r};
      got.push_back({ge::test::bits(gw::rate_of(p)), ge::test::bits(gw::fall_speed(p)),
                     ge::test::bits(gw::visibility_of(p)),
                     static_cast<uint64_t>(gw::tier_of(p, 10))});
    }
  }
  for (const double y : {900.0, 1250.0, 1400.0, 1600.0}) {
    got.push_back({ge::test::bits(gw::rain_share_at(y, 1500))});
  }
  gw::Wet w;
  for (const double rate : {3.0, 3.0, 0.0, 12.0, 0.0}) {
    w = gw::step_wet(w, rate, gw::dry_rate(4, 0.5), 900);
    got.push_back({ge::test::bits(w.water), ge::test::bits(w.wet), ge::test::bits(w.puddles)});
  }
  gw::Cover c;
  for (const double i : {0.8, 0.8, 0.0, 0.5}) {
    c = gw::step_cover(c, {i, i == 0.5 ? 1.0 : 0.0}, 0.3, 1800);
    got.push_back({ge::test::bits(c.depth), ge::test::bits(c.cover)});
  }
  // The first four lit moments of a storm (a hundredth of a second apart, a second between).
  int lit = 0;
  for (int k = 0; k < 100000 && lit < 4; ++k) {
    const double f = gw::flash_at(k * 0.01, {1, 1}, 20, 7);
    if (f <= 0.2) continue;
    got.push_back({ge::test::bits(k * 0.01), ge::test::bits(f)});
    ++lit;
    k += 100;
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# rate, fall speed, visibility, tier per fall; rain share per height; the wet ground, "
           "the snow's cover per step; a storm's flash per time "
           "(wind_precip_test)\n";
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
  CHECK_MSG(got == want, "the precipitation moved; if the move is meant, rewrite " +
                             std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the precip "
                             "lab's tables before and after in the PR (skill: water)");
}

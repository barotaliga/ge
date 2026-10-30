// engine/water/rain.hpp: the rain is heard by its rate (every voice rises with it, a downpour at
// full); where the ear is splits it (on water, under a roof, the patter only where the fall
// reaches); snow falls silently and hisses only on a wind; a lake in a light rain hisses high
// and in a heavy one lower; a drizzle's drops are heard one by one and a downpour's are not; the
// bed renders the same bytes for the same seed and louder for a harder rain.
//
// ge:summary Tests for the rain's sound: levels by rate, the place's split, driven snow, the bed
// ge:keywords test rain sound hear_rain rain_power patter drum driven-snow rain_drops bed rate
// ge:skill water audio testing
#include <cmath>
#include <cstring>
#include <vector>

#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "water/rain.hpp"
#include "wind/precip.hpp"

namespace {

namespace a = ge::audio;
namespace gw = ge::water;

ge::wind::Precip rain(double i) { return {i, 1}; }
ge::wind::Precip snow(double i) { return {i, 0}; }

std::vector<float> render(a::Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * a::kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

// Two seconds of the bed under `p` at `place`.
std::vector<float> bed_sound(const ge::wind::Precip& p, const gw::RainPlace& place, uint32_t seed) {
  a::Synth s;
  gw::RainBed bed = gw::make_rain_bed(0, seed);
  const gw::RainHearing heard = gw::hear_rain(p, 3, place);
  std::vector<float> out;
  for (int i = 0; i < 120; ++i) {
    gw::rain_bed_update(bed, s, heard, 1.0f / 60);
    std::vector<float> chunk = render(s, 1.0 / 60);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  return out;
}

}  // namespace

TEST(the_rain_is_heard_by_its_rate) {
  CHECK(gw::rain_power(0) == 0);
  const gw::RainHearing none = gw::hear_rain(rain(0), 3, {});
  CHECK(none.patter == 0 && none.wash == 0 && none.water == 0 && none.driven == 0);
  // A downpour (50 mm/h) at full.
  const gw::RainHearing full = gw::hear_rain({0.8, 1}, 3, {});
  CHECK(ge::wind::rate_of({0.8, 1}) > 45 && ge::wind::rate_of({0.8, 1}) < 55);
  CHECK(full.patter > 0.9f && full.patter < 1.1f);
  float last = 0;
  for (const double i : {0.05, 0.2, 0.4, 0.6, 0.8, 1.0}) {
    const gw::RainHearing h = gw::hear_rain(rain(i), 3, {});
    CHECK(h.patter > last);
    CHECK(h.wash > 0 && h.patter <= 1.5f);
    last = h.patter;
  }
  // A drizzle is a whisper against a downpour.
  CHECK(gw::hear_rain(rain(0.05), 3, {}).patter < 0.1f);
}

TEST(where_the_ear_is_splits_the_rain) {
  const ge::wind::Precip p = rain(0.5);
  const gw::RainHearing open = gw::hear_rain(p, 3, {});
  const gw::RainHearing shore = gw::hear_rain(p, 3, {.open = 1, .water = 0.6});
  const gw::RainHearing under = gw::hear_rain(p, 3, {.open = 0, .water = 0, .roof = 1});
  CHECK(open.water == 0 && open.drum == 0);
  CHECK(shore.water > 0 && shore.patter < open.patter);
  CHECK(std::fabs(shore.water + shore.patter - open.patter) < 1e-5f);
  // Under a roof: no patter round the ear, the roof drummed on, the rain outside heard.
  CHECK(under.patter == 0 && under.drum > open.patter && under.wash > 0);
  CHECK(under.wash < open.wash);
}

TEST(snow_falls_silently_and_hisses_on_a_wind) {
  const gw::RainHearing calm = gw::hear_rain(snow(0.8), 2, {});
  const gw::RainHearing gale = gw::hear_rain(snow(0.8), 20, {});
  CHECK(calm.patter == 0 && calm.driven == 0);
  CHECK(gale.driven > 0.5f);
  CHECK(gw::hear_rain(snow(0.4), 20, {}).driven < gale.driven);
}

TEST(a_lake_in_the_rain_hisses_high_and_lower_as_it_comes_down_harder) {
  const gw::RainPlace lake{.open = 1, .water = 1};
  const auto light = gw::rain_targets(gw::hear_rain(rain(0.2), 3, lake), {});
  const auto heavy = gw::rain_targets(gw::hear_rain(rain(0.9), 3, lake), {});
  CHECK(light[gw::kOnWater].cutoff > 12000);
  CHECK(heavy[gw::kOnWater].cutoff < light[gw::kOnWater].cutoff);
  CHECK(heavy[gw::kOnWater].level > light[gw::kOnWater].level);
}

TEST(a_drizzles_drops_are_heard_one_by_one_and_a_downpours_are_not) {
  a::Synth s;
  ge::core::Rng rng = ge::core::make_rng(5);
  int drizzle = 0, downpour = 0;
  const gw::RainHearing thin = gw::hear_rain(rain(0.02), 0, {.open = 1, .water = 0.5});
  const gw::RainHearing thick = gw::hear_rain(rain(0.8), 0, {.open = 1, .water = 0.5});
  for (int i = 0; i < 600; ++i) {
    drizzle += gw::rain_drops(s, rng, thin, 1.0f / 60);
    downpour += gw::rain_drops(s, rng, thick, 1.0f / 60);
  }
  CHECK(drizzle > 10);
  CHECK(downpour == 0);
}

TEST(the_bed_is_the_same_for_the_same_seed_and_louder_for_harder_rain) {
  const gw::RainPlace shore{.open = 1, .water = 0.3};
  const std::vector<float> a1 = bed_sound(rain(0.3), shore, 4), a2 = bed_sound(rain(0.3), shore, 4);
  CHECK(a1.size() == a2.size());
  CHECK(std::memcmp(a1.data(), a2.data(), a1.size() * sizeof(float)) == 0);
  float last = -200;
  for (const double i : {0.1, 0.4, 0.8}) {
    const float db = a::measure(bed_sound(rain(i), shore, 4)).rms_db;
    CHECK(db > last + 3);
    last = db;
  }
}

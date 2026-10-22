// The skier's beds (plugins/skier/app): the wind climbs with the air and the tuck moves it into
// the roar; still air is silence; the snow's hiss is the groomer's and the hush the powder's; the
// edge tears only on an edge and goes with a skid; nothing sounds in the air but the wind; and the
// ride bed reads the skid off the way and builds, steers and tears down its layers on the synth.
//
// ge:summary Tests for the skier's wind and snow beds and the ride bed
// ge:keywords test wind bed snow bed ride bed tuck skid hiss powder rack
// ge:skill skier audio testing
#include <vector>

#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "skier/app/ride_bed.hpp"

namespace {

using namespace ge::skier;

float level_sum(const auto& targets) {
  float s = 0;
  for (const auto& t : targets) s += t.level;
  return s;
}

}  // namespace

TEST(still_air_is_silent_and_the_wind_climbs_with_it) {
  CHECK(level_sum(wind_targets({.wind = 0}, {})) == 0);
  const float slow = level_sum(wind_targets({.wind = 10}, {}));
  const float fast = level_sum(wind_targets({.wind = 30}, {}));
  const float gale = level_sum(wind_targets({.wind = 60}, {}));
  CHECK(slow > 0 && fast > 2 * slow && gale > fast);
}

TEST(the_tuck_moves_the_wind_into_the_roar_and_quiets_the_suit) {
  const auto up = wind_targets({.wind = 25, .tuck = 0}, {});
  const auto down = wind_targets({.wind = 25, .tuck = 1}, {});
  CHECK(down[kRoar].level > 4 * up[kRoar].level);
  CHECK(down[kFlutter].level < up[kFlutter].level);
  CHECK(down[kRush].level < up[kRush].level);
}

TEST(a_crosswind_lands_on_its_ear) {
  const auto left = wind_targets({.wind = 20, .side = -1}, {});
  CHECK(left[kRush].pan < 0 && left[kWhistle].pan < left[kRush].pan);
  CHECK(left[kRush].level > wind_targets({.wind = 20}, {})[kRush].level);
}

TEST(the_groomer_hisses_and_the_powder_hushes) {
  SnowVoice v{.speed = 12, .pace = 0.5f, .packed = 1};
  const auto groomer = snow_targets(v, {});
  v.packed = 0;
  const auto powder = snow_targets(v, {});
  CHECK(groomer[kHiss].level > 0 && groomer[kPowder].level == 0);
  CHECK(powder[kPowder].level > 0 && powder[kHiss].level == 0);
  CHECK(powder[kPowder].cutoff < groomer[kHiss].cutoff);
}

TEST(the_edge_tears_on_an_edge_and_a_skid_rasps_instead) {
  SnowVoice v{.speed = 18, .pace = 0.7f, .packed = 1, .edge = 0.8f};
  const auto carve = snow_targets(v, {});
  v.skid = 1;
  const auto skid = snow_targets(v, {});
  CHECK(carve[kEdgeTear].level > 0 && carve[kSkidRasp].level == 0);
  CHECK(skid[kEdgeTear].level < 0.5f * carve[kEdgeTear].level && skid[kSkidRasp].level > 0);
  v.on_snow = false;
  CHECK(level_sum(snow_targets(v, {})) == 0);
}

TEST(the_ride_bed_reads_the_skid_off_the_way) {
  SkierState s;
  s.heading = 0;
  s.vz = 10;
  s.speed = 10;
  CHECK(snow_voice_of(s, kReferenceSkis).skid < 1e-6f);
  s.vz = 0;
  s.vx = 10;  // sliding sideways
  CHECK(snow_voice_of(s, kReferenceSkis).skid > 0.99f);
  s.down = true;
  CHECK(snow_voice_of(s, kReferenceSkis).edge == 0);
}

TEST(the_ride_bed_builds_steers_and_tears_down_its_layers) {
  ge::audio::Synth synth;
  RideBed bed = make_ride_bed(0, 1);
  SkierState s;
  s.vz = 20;
  s.speed = 20;
  s.tuck = 1;
  std::vector<float> out(size_t{2} * 800);
  for (int f = 0; f < 60; f++) {
    ride_bed_update(bed, synth, s, kReferenceSkis, 1.0f / 60, {});
    synth.render(out.data(), 800);
  }
  CHECK(bed.air > 15 && bed.air <= 20);
  CHECK(synth.stats().layers == static_cast<int>(kWindLayers) + static_cast<int>(kSnowLayers));
  CHECK(ge::audio::measure(out).loudest_db > -50);
  ride_bed_silence(bed, synth);
  for (int f = 0; f < 12; f++) synth.render(out.data(), 800);  // they fade, then free
  CHECK(synth.stats().layers == 0);
}

// engine/water/sound.hpp: a bubble rings at its Minnaert pitch; nothing is heard far from water;
// a fresh wind's lapping is louder on the shore than a calm's; a fall twice as far is half as
// loud and its side is the ear's; an ear a metre under is under and muffles the bed's highs; a
// bigger splash is darker and longer, a faster one louder; the bed and the one-shots render the
// same bytes for the same seed.
//
// ge:summary Tests for the water's sound: Minnaert, hear's intensities, the muffle, splashes
// ge:keywords test water sound hear minnaert splash bubble muffle lapping roar intensity
// ge:skill water audio testing
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "water/flow.hpp"
#include "water/sound.hpp"
#include "water/world.hpp"

namespace {

namespace a = ge::audio;
namespace gw = ge::water;

// A round lake 150 m across round the origin, its surface at 0 and its bed shelving to a beach.
double beach(const void*, double x, double z) {
  const double r = std::sqrt(x * x + z * z);
  return -8 * (1 - (r / 150) * (r / 150));
}

gw::World lake(double wind) {
  ge::wind::Field field;
  field.mean = wind;
  field.from = 0;
  gw::World world(field);
  std::vector<gw::Point> shore;
  for (int i = 0; i < 64; ++i) {
    const double t = 2 * 3.14159265358979 * i / 64;
    shore.push_back({150 * std::cos(t), 150 * std::sin(t)});
  }
  gw::add_lake(world, shore, 0, {}, {beach, nullptr});
  return world;
}

// A flow region with water held behind a 6 m ledge at x = 12 (as the stream test's), run until
// it pours over.
gw::World ledge() {
  gw::World world;
  gw::Flow f = gw::make_flow({.nx = 64, .nz = 8, .cell = 0.5});
  gw::set_bed(f, 0, 0, 12, 4, 6);
  gw::fill_to(f, 0, 0, 8, 4, 7);
  gw::add_flow(world, std::move(f));
  for (int i = 0; i < 240; ++i) gw::step_flows(world, 1.0 / 120);
  return world;
}

std::vector<float> render(a::Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * a::kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

a::Meter splash_meter(double speed, double size) {
  a::Synth s;
  ge::core::Rng rng = ge::core::make_rng(3);
  gw::play_splash(s, rng, {.speed = speed, .size = size});
  return a::measure(render(s, 2));
}

}  // namespace

TEST(a_bubble_rings_at_its_minnaert_pitch) {
  CHECK(std::fabs(gw::minnaert_hz(0.003) - 1086.7) < 1);
  const a::Tone t = gw::bubble(0.01, 0.01f);
  CHECK(std::fabs(t.from_hz - 326) < 0.5);
  CHECK(t.to_hz > t.from_hz);
  CHECK(gw::bubble(0.01, 0.01f).duration_ms > gw::bubble(0.002, 0.01f).duration_ms);
}

TEST(nothing_is_heard_far_from_the_water) {
  const gw::World world = lake(8);
  const gw::Hearing h = gw::hear(world, 600, 2, 0, 10);
  CHECK(h.lap.level == 0 && h.surf.level == 0 && h.rush.level == 0 && h.roar.level == 0);
  CHECK(h.under == 0);
  for (const a::LayerTarget& t : gw::water_targets(h, {})) CHECK(t.level == 0);
}

TEST(a_fresh_wind_laps_louder_on_the_shore) {
  const gw::World calm = lake(1), fresh = lake(10);
  // Averaged over a few seconds: the lapping follows the waves past the shore.
  double lc = 0, lf = 0;
  for (int i = 0; i < 40; ++i) {
    const double t = 10 + 0.1 * i;
    lc += gw::hear(calm, 0, 1.7, -152, t).lap.level;
    lf += gw::hear(fresh, 0, 1.7, -152, t).lap.level;
  }
  CHECK_MSG(lf > 2 * lc && lc > 0,
            "calm " + std::to_string(lc / 40) + ", fresh " + std::to_string(lf / 40));
}

TEST(a_fall_twice_as_far_is_half_as_loud_and_on_its_side) {
  const gw::World world = ledge();
  const gw::Flow& f = world.flows[0];
  CHECK(!f.falls.empty());
  const gw::Fall& fall = f.falls[0];
  // Facing +z, the fall at +x is on the right.
  const gw::Hearing near = gw::hear(world, fall.land_x + 20, fall.land_y, fall.land_z, 0, 0);
  const gw::Hearing far = gw::hear(world, fall.land_x + 40, fall.land_y, fall.land_z, 0, 0);
  CHECK(near.roar.level > 0);
  const double ratio = far.roar.level / near.roar.level;
  CHECK_MSG(std::fabs(ratio - 0.5) < 0.05, "far over near " + std::to_string(ratio));
  CHECK(near.roar.pan < -0.5);  // the ear is east of it, facing +z: the fall is on the left
  CHECK(near.drop > 1);
}

TEST(an_ear_under_the_water_muffles_the_bed) {
  const gw::World world = lake(6);
  const gw::Hearing over = gw::hear(world, 0, 1.0, 0, 5);
  const gw::Hearing under = gw::hear(world, 0, -1.0, 0, 5);
  CHECK(over.under == 0);
  CHECK(under.under == 1);
  CHECK(std::fabs(under.depth - 1) < 0.3);
  const auto t = gw::water_targets(under, {});
  for (int i = 0; i < gw::kRoom; ++i) CHECK(t[static_cast<size_t>(i)].cutoff <= 900.5f);
  CHECK(t[gw::kRoom].level > 0);
  CHECK(gw::water_targets(over, {})[gw::kRoom].level == 0);
  // An airborne sound heard from under is darker and quieter.
  a::LayerTarget wind{.level = 0.02f, .cutoff = 3000};
  gw::muffle(wind, 1);
  CHECK(wind.cutoff == gw::kMuffleHz && wind.level < 0.02f * 0.6f);
}

TEST(a_bigger_splash_is_darker_and_longer_a_faster_one_louder) {
  const a::Meter small = splash_meter(5, 0.05), big = splash_meter(5, 0.6);
  CHECK_MSG(big.centroid_hz < small.centroid_hz,
            std::to_string(big.centroid_hz) + " Hz vs " + std::to_string(small.centroid_hz));
  CHECK(big.length_s > small.length_s);
  const a::Meter slow = splash_meter(2, 0.15), fast = splash_meter(9, 0.15);
  CHECK(fast.loudest_db > slow.loudest_db + 6);
}

TEST(the_bed_and_the_one_shots_are_the_same_for_the_same_seed) {
  const gw::World world = ledge();
  std::vector<float> out[2];
  for (std::vector<float>& o : out) {
    a::Synth s;
    gw::WaterBed bed = gw::make_water_bed(0, 9);
    for (int i = 0; i < 60; ++i) {
      gw::water_bed_update(bed, s, world, 30, 1, 2, i / 60.0, 0, 1.0f / 60);
      if (i == 20) gw::play_splash(s, bed.rng, {.speed = 6, .size = 0.3});
      std::vector<float> chunk = render(s, 1.0 / 60);
      o.insert(o.end(), chunk.begin(), chunk.end());
    }
  }
  CHECK(out[0].size() == out[1].size());
  CHECK(std::memcmp(out[0].data(), out[1].data(), out[0].size() * sizeof(float)) == 0);
  CHECK(a::measure(out[0]).loudest_db > -60);
}

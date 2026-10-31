// engine/render/mist_fog.hpp: what the air says lies, as the frame's fog. No mist is an empty
// volume; a bank lies on its floor as deep as the air laid it (4 m at the least), even to its top,
// with the mist's extinction and carving, drifting on the wind it is handed; steam over a pool is
// thickest on the water, thinned e^-3 to its top, carved into small wisps that climb at its rise;
// steam's mean over its depth is the mist's extinction;
// on the ground under each point a bank's height is the ground's; a valley's radiation fog is a
// level sea whose top is the floor plus its depth, and a hill fog is no sea.
//
// ge:summary Tests for fog from the air, drawn: banks, steam's rising wisps, a valley's level sea
// ge:keywords test render fog mist mist_volume mist_valley steam-fog bank valley-fog
// ge:skill render testing
#include <cmath>

#include "check.hpp"
#include "render/mist_fog.hpp"

namespace {

namespace r = ge::render;
namespace gw = ge::wind;

bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }

gw::Mist mist(gw::MistKind kind, double density, double depth, double noise, double rise = 0) {
  gw::Mist m;
  m.kind = kind;
  m.strength = 1;
  m.density = density;
  m.depth = depth;
  m.visibility = 3 / density;
  m.noise = noise;
  m.rise = rise;
  return m;
}

r::MistPlace pool() {
  r::MistPlace at;
  at.centre[0] = 8, at.centre[1] = 18;
  at.half[0] = 3.2, at.half[1] = 4;
  at.floor_y = -0.1;
  at.drift[0] = 1.5, at.drift[1] = -0.5;
  return at;
}

}  // namespace

TEST(no_mist_is_an_empty_volume) {
  const r::FogVolume v = r::mist_volume(gw::Mist{}, pool());
  CHECK(v.density == 0);
}

TEST(a_bank_lies_on_its_floor_as_deep_as_the_air_laid_it) {
  const r::FogVolume v = r::mist_volume(mist(gw::MistKind::radiation, 0.02, 7.4, 0.55), pool());
  CHECK(near(v.density, 0.02) && near(v.noise, 0.55) && v.falloff == 0);
  CHECK(near(2 * v.half[1], 7.4) && near(v.centre[1] - v.half[1], -0.1));
  CHECK(near(v.centre[0], 8) && near(v.centre[2], 18) && near(v.half[0], 3.2));
  CHECK(near(v.half[2], 4) && v.edge <= 3.2);
  CHECK(near(v.wind[0], 1.5) && v.wind[1] == 0 && near(v.wind[2], -0.5));
  CHECK(near(v.noise_size, r::kBankNoiseSize) && v.shape == r::FogVolume::Shape::box);
  // A shallow one is still 4 m deep.
  CHECK(near(2 * r::mist_volume(mist(gw::MistKind::advection, 0.01, 1, 0.45), pool()).half[1], 4));
  // A thin edge asked for is kept.
  r::MistPlace thin = pool();
  thin.edge = 0.5;
  CHECK(near(r::mist_volume(mist(gw::MistKind::radiation, 0.02, 7.4, 0.55), thin).edge, 0.5));
}

TEST(steam_is_thickest_on_the_water_and_its_wisps_climb) {
  const r::FogVolume v = r::mist_volume(mist(gw::MistKind::steam, 0.03, 6, 0.85, 0.3), pool());
  CHECK(near(2 * v.half[1], 6) && near(v.centre[1] - v.half[1], -0.1));
  CHECK(near(std::exp(-v.falloff * 6), std::exp(-r::kSteamFalloff)));
  // Its mean over its depth is the mist's extinction.
  CHECK(near(v.density * (1 - std::exp(-v.falloff * 6)) / (v.falloff * 6), 0.03));
  CHECK(near(v.wind[1], 0.3) && near(v.noise, 0.85));
  CHECK(v.noise_size >= 2 && v.noise_size <= 12 && v.noise_size < r::kBankNoiseSize);
  CHECK(v.edge <= 2);
}

TEST(a_bank_on_the_ground_takes_the_grounds_height) {
  r::MistPlace at = pool();
  at.hug_ground = true;
  at.round = true;
  const r::FogVolume v = r::mist_volume(mist(gw::MistKind::radiation, 0.02, 30, 0.55), at);
  CHECK(v.hug_ground && v.centre[1] == 0 && v.shape == r::FogVolume::Shape::ellipsoid);
  CHECK(near(2 * v.half[1], 30));
}

TEST(a_valleys_fog_is_a_level_sea_and_a_hill_fog_none) {
  const double drift[2] = {0.4, 0.2};
  const r::ValleyFog f = r::mist_valley(mist(gw::MistKind::valley, 0.015, 150, 0.55), 1200, drift);
  CHECK(near(f.top_m, 1350) && near(f.density, 0.015));
  CHECK(near(f.drift[0], 0.4) && near(f.drift[1], 0.2));
  CHECK(r::mist_valley(mist(gw::MistKind::hill, 0.02, 200, 0.4), 1200, drift).density == 0);
  CHECK(r::mist_valley(mist(gw::MistKind::steam, 0.02, 6, 0.85), 1200, drift).density == 0);
}

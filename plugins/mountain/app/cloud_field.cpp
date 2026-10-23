// The broken skies' stacks of layers, dealt off the map's seed, and the wind aloft that carries
// them (cloud_field.hpp says what each band is).
//
// ge:summary The broken skies' cloud stacks, dealt per map, and the wind aloft
// ge:keywords cloud field stacks deal hash wind-aloft veer drift cloud_field sky_at
// ge:skill mountain render
// ge:test tests/mountain_cloud_field_test.cpp
#include "mountain/app/cloud_field.hpp"

#include <algorithm>
#include <cstdint>

#include "core/libm/libm.hpp"
#include "mountain/air/air.hpp"
#include "mountain/app/weather_look.hpp"
#include "render/clouds.hpp"

namespace ge::mountain::app {

namespace {

using mapgen::WeatherKind;

// One kind of layer: its bands [least, most] (the base over the valley floor, the depth, the
// cover, the cell's span across the wind, m), how far a cell is drawn out along the wind, 1 a
// heap or 0 a sheet, the optical depth a full column shows the eye and takes off the sun, a
// cell's life (s) and the least speed the kind is seen to move at (m/s).
struct Def {
  double base[2], depth[2], cover[2], cell[2];
  double stretch, dome, opacity, shadow_tau, life, speed;
};

constexpr Def kCumulus{{1500, 2000}, {500, 900}, {0.22, 0.36}, {1300, 1900}, 1.3, 1, 28, 2.4,
                       480,          4};
constexpr Def kCongestus{
    {1500, 1800}, {1300, 2000}, {0.32, 0.44}, {1700, 2400}, 1.3, 1, 40, 3, 600, 4};
constexpr Def kRipples{{3600, 4500}, {250, 450}, {0.15, 0.3}, {600, 900}, 1.6, 0.6, 5, 0.5, 900, 8};
constexpr Def kStreaks{{7500, 9000}, {400, 800}, {0.35, 0.5}, {2500, 4000}, 5, 0, 0.9,
                       0.25,         1200,       15};
constexpr Def kFairStreaks{{7600, 8800}, {300, 600}, {0.02, 0.14}, {3000, 5000}, 5, 0, 0.9, 0.25,
                           1200,         15};
constexpr Def kVeil{{7000, 7800}, {600, 900}, {0.75, 0.92}, {6000, 9000}, 2.5, 0, 0.35, 0.2,
                    1800,         15};

struct Stack {
  int count;
  const Def* defs[render::kCloudLayers];
};

Stack stack_of(WeatherKind k) {
  switch (k) {
    case WeatherKind::fair:
      return {2, {&kCumulus, &kFairStreaks, nullptr}};
    case WeatherKind::flurries:
      return {2, {&kCongestus, &kRipples, nullptr}};
    case WeatherKind::high:
      return {2, {&kStreaks, &kVeil, nullptr}};
    default:
      return {0, {nullptr, nullptr, nullptr}};
  }
}

// The map's seed and a salt to 0..1.
double hash01(double seed, uint32_t salt) {
  uint32_t h = static_cast<uint32_t>(static_cast<int64_t>(seed)) ^ (salt * 0x27d4eb2du);
  h *= 0x9e3779b1u;
  h ^= h >> 16;
  h *= 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;
  return static_cast<double>(h) / 4294967296.0;
}

double deal(const double band[2], double u) { return band[0] + (band[1] - band[0]) * u; }

constexpr double kPi = 3.14159265358979323846;

}  // namespace

double wind_aloft(double surface, double h, double least) {
  return std::min(45.0, std::max(least, surface * (1 + 1.2 * core::libm::log(1 + h / 600))));
}

double veer_aloft(double h, double latitude) {
  return (latitude < 0 ? -1.0 : 1.0) * 0.35 * core::libm::log(1 + h / 1000);
}

render::Clouds cloud_layers_for(const mapgen::Weather& weather, double seed, double floor_y,
                                double latitude, double t) {
  const Stack stack = stack_of(weather.kind);
  render::Clouds out;
  out.count = stack.count;
  for (int i = 0; i < stack.count; ++i) {
    const Def& d = *stack.defs[i];
    auto u = [&](uint32_t k) { return hash01(seed, 101 + static_cast<uint32_t>(i) * 17 + k); };
    render::CloudLayer& l = out.layers[i];
    const double base = deal(d.base, u(0));
    const double depth = deal(d.depth, u(1));
    const double mid = base + 0.5 * depth;
    l.base_m = floor_y + base;
    l.top_m = l.base_m + depth;
    l.cover = deal(d.cover, u(2));
    l.cell_m = deal(d.cell, u(3));
    l.stretch = d.stretch;
    l.dome = d.dome;
    l.density = d.opacity / depth;
    l.shadow_tau = d.shadow_tau;
    l.heading = weather.wind_from + kPi + veer_aloft(mid, latitude);
    // Where in the noise this map's layer starts: a share of the tile along the wind and of the
    // cell's lives, so two maps' skies differ.
    const double tile = l.cell_m * l.stretch * render::kCloudTileCells;
    l.drift_m = u(4) * tile + wind_aloft(weather.wind, mid, d.speed) * kCloudDrift * t;
    l.evolve = u(5) * render::kCloudTileCells + t / d.life;
  }
  return out;
}

render::Clouds cloud_field(const mapgen::Level& level, double t) {
  return cloud_layers_for(level.weather, level.seed, level.mountain.base.y, level.sun.latitude, t);
}

void sky_at(const mapgen::Level& level, double t, render::Weather& weather,
            render::Clouds& clouds) {
  clouds = cloud_field(level, t);
  weather = weather_look(level.weather, air::fall_at(level, t), clouds.count > 0);
}

}  // namespace ge::mountain::app

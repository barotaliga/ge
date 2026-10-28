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
#include "render/cloud_kinds.hpp"
#include "render/clouds.hpp"

namespace ge::mountain::app {

namespace {

using mapgen::WeatherKind;

using render::CloudKind;

const CloudKind& kind(const char* name) { return *render::cloud_kind_named(name); }

struct Stack {
  int count;
  const CloudKind* kinds[render::kCloudLayers];
};

Stack stack_of(WeatherKind k) {
  switch (k) {
    case WeatherKind::fair:
      return {2, {&kind("cumulus"), &kind("wisps"), nullptr, nullptr}};
    case WeatherKind::flurries:
      return {2, {&kind("congestus"), &kind("altocumulus"), nullptr, nullptr}};
    case WeatherKind::high:
      return {2, {&kind("cirrus"), &kind("cirrostratus"), nullptr, nullptr}};
    case WeatherKind::overcast:
      return {1, {&kind("stratus"), nullptr, nullptr, nullptr}};
    case WeatherKind::snow:
    case WeatherKind::storm:
      return {1, {&kind("nimbostratus"), nullptr, nullptr, nullptr}};
    default:
      return {0, {nullptr, nullptr, nullptr, nullptr}};
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

render::Clouds cloud_layers_of(const render::CloudKind* const* kinds, int count,
                               const mapgen::Weather& weather, double seed, double floor_y,
                               double latitude, double t) {
  render::Clouds out;
  out.count = std::clamp(count, 0, render::kCloudLayers);
  for (int i = 0; i < out.count; ++i) {
    const CloudKind& d = *kinds[i];
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
    l.anvil = d.anvil;
    l.erode = d.erode;
    l.fibre = d.fibre;
    l.patch = d.patch;
    l.drop_um = d.drop;
    l.drop_spread = d.spread;
    l.halo = d.halo;
    l.deck = d.deck;
    l.thin = d.thin;
    l.thick_cells = d.thick_cells;
    // This sky's layer thinner or thicker than the kind's middle.
    l.thick_bias = 0.8 * u(6) - 0.4;
    l.heading = weather.wind_from + kPi + veer_aloft(std::max(mid, 0.0), latitude);
    // Where in the noise this map's layer starts: a share of the tile along the wind and of the
    // cell's lives, so two maps' skies differ.
    const double tile = l.cell_m * l.stretch * render::kCloudTileCells;
    l.drift_m =
        u(4) * tile + wind_aloft(weather.wind, std::max(mid, 0.0), d.speed) * kCloudDrift * t;
    l.evolve = u(5) * render::kCloudTileCells + t / d.life;
  }
  return out;
}

render::Clouds cloud_layers_for(const mapgen::Weather& weather, double seed, double floor_y,
                                double latitude, double t) {
  const Stack stack = stack_of(weather.kind);
  return cloud_layers_of(stack.kinds, stack.count, weather, seed, floor_y, latitude, t);
}

void lid_under(const render::CloudKind* const* kinds, int count, render::Weather& weather) {
  for (int i = 0; i < count; ++i) {
    const CloudKind& k = *kinds[i];
    // A lid: a full, thick, low sheet over the valley floor (not one lying in it; a middle
    // layer's thin patches let the sun through).
    if (k.cover[0] < 0.9 || k.opacity < 10 || k.base[1] <= 0 || k.base[0] > 2500) continue;
    const double dark = std::min(1.0, k.opacity / 100);
    weather.cover = 1;
    weather.deck_light = std::min(weather.deck_light, 1.15 - 0.45 * dark);
    weather.deck_zenith = std::min(weather.deck_zenith, 0.62 - 0.25 * dark);
    const double tint[3] = {0.95, 1.0, 1.1};
    for (int c = 0; c < 3; ++c) weather.deck_tint[c] = tint[c];
  }
}

render::Clouds cloud_field(const mapgen::Level& level, double t) {
  return cloud_layers_for(level.weather, level.seed, level.mountain.base.y, level.sun.latitude, t);
}

void sky_at(const mapgen::Level& level, double t, render::Weather& weather, render::Clouds& clouds,
            bool layers, double eye_y) {
  clouds = layers ? cloud_field(level, t) : render::Clouds{};
  weather = weather_look(level.weather, air::fall_at(level, t, eye_y), clouds.count > 0);
}

}  // namespace ge::mountain::app

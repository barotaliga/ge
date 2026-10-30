// The wind's gusts and veer, the fall's squalls, and the visual range through it.
//
// ge:summary The wind and the falling snow at a moment: definitions
// ge:keywords air wind gust veer fall squall visibility phase hash field_of
// ge:skill mountain
// ge:test tests/mountain_air_test.cpp
#include "mountain/air/air.hpp"

#include <algorithm>
#include <cstdint>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/weather.hpp"
#include "wind/precip.hpp"
#include "wind/storm.hpp"

namespace ge::mountain::air {

namespace {

constexpr double kPi = 3.141592653589793;
constexpr double kTau = 2 * kPi;

struct Swell {
  double period;  // s
  double share;   // of the mean
};

constexpr Swell kGusts[] = {{6.3, 0.14}, {17.9, 0.2}, {53, 0.16}};
constexpr double kVeer = 0.22;      // rad either way
constexpr double kVeerPeriod = 41;  // s
constexpr Swell kSqualls[] = {{37, 0.35}, {113, 0.4}, {347, 0.25}};
constexpr double kBlown = 0.3;         // a storm's gust's share lifted off the ground
constexpr double kRangeFull = 45;      // m, at a fall of 1
constexpr double kRangeClear = 20000;  // m, at none

// A phase in [0, 2 pi) hashed off the map's seed and a salt.
double phase(const mapgen::Level& level, uint32_t salt, uint32_t mul1, uint32_t mul2, int s1,
             int s2) {
  uint32_t h = (core::num::to_uint32(level.seed) ^ salt) * mul1;
  h ^= h >> s1;
  h *= mul2;
  h ^= h >> s2;
  return static_cast<double>(h) / 4294967296.0 * kTau;
}

double wind_phase(const mapgen::Level& level, uint32_t salt) {
  return phase(level, salt, 0x9e3779b1u, 0x85ebca77u, 15, 13);
}

double fall_phase(const mapgen::Level& level, uint32_t salt) {
  return phase(level, salt, 0x85ebca6bu, 0xc2b2ae35u, 13, 16);
}

// How deep each snowing sky breathes about its mean, as a share of it.
double breath(mapgen::WeatherKind kind) {
  switch (kind) {
    case mapgen::WeatherKind::flurries:
      return 0.9;
    case mapgen::WeatherKind::storm:
      return 0.35;
    default:
      return 0.3;
  }
}

// The fall's visual range moved toward the rain's by its share of it, in the logarithm.
void see_through_rain(Fall& out) {
  if (out.rain <= 0) return;
  const double rain = wind::visibility_of({out.fall, 1});
  out.visibility = out.visibility * core::libm::pow(rain / out.visibility, out.rain);
}

// m/s: a severe cell's downdraft (engine/wind/storm.cpp: 4 + 16 of its peak).
constexpr double kFullDowndraft = 20;

}  // namespace

double downhill_from(double from) {
  // From `from` the air moves toward `from + pi`, whose +z part is -cos(from): up the mountain
  // while cos(from) > 0.
  return core::libm::cos(from) > 0 ? kPi - from : from;
}

wind::Field field_of(const mapgen::Level& level) {
  wind::Field f;
  f.mean = level.weather.wind;
  f.from = level.weather.wind_from;
  f.gusts = 3;
  for (uint32_t i = 0; i < 3; ++i) {
    f.gust[i] = {kGusts[i].period, kGusts[i].share, wind_phase(level, i + 1)};
  }
  f.veer = {kVeerPeriod, kVeer, wind_phase(level, 9)};
  f.fold = downhill_from;
  f.roughness = wind::kRoughness::snow;
  return f;
}

Wind wind_at(const mapgen::Level& level, double t) {
  const wind::Air a = wind::open_at(field_of(level), t);
  Wind out;
  out.x = a.x;
  out.z = a.z;
  out.speed = a.speed;
  out.gust = a.gust;
  return out;
}

Fall fall_at(const mapgen::Level& level, double t, double y) {
  const mapgen::Weather& w = level.weather;
  Fall out;
  if (!mapgen::snows(w.kind) || w.snowfall <= 0) return out;
  double g = 0;
  for (uint32_t i = 0; i < 3; ++i) {
    g += kSqualls[i].share *
         core::libm::sin(kTau * t / kSqualls[i].period + fall_phase(level, 0x5au + i));
  }
  double fall = w.snowfall * (1 + breath(w.kind) * g);
  if (w.kind == mapgen::WeatherKind::storm) fall += kBlown * (wind_at(level, t).gust - 0.5);
  out.fall = core::clamp(fall, 0.0, 1.0);
  out.visibility = visibility_in(out.fall);
  if (w.snowline > mapgen::kNoSnowline && out.fall > 0) {
    out.rain = wind::rain_share_at(y, w.snowline + 0.5 * wind::kMeltBand);
    see_through_rain(out);
  }
  return out;
}

wind::Storm storm_of(const mapgen::Level& level) {
  const mapgen::Weather& w = level.weather;
  if (w.thunder <= 0) return wind::Storm();
  // The cells drift the way the map's own wind blows, folded down the mountain.
  const double from = downhill_from(w.wind_from);
  const double vx = -kSteer * core::libm::sin(from), vz = -kSteer * core::libm::cos(from);
  const double mx = 0.5 * (level.mountain.summit.x + level.mountain.base.x);
  const double mz = 0.5 * (level.mountain.summit.z + level.mountain.base.z);
  wind::StormSpec spec;
  spec.seed = (core::num::to_uint32(level.seed) ^ 0x5707e3u) * 0x9e3779b1u + 1;
  spec.start = -wind::kTowering;
  spec.x = mx - vx * (kOverhead - spec.start);
  spec.z = mz - vz * (kOverhead - spec.start);
  spec.renew = 3600;
  spec.severity = core::clamp(w.thunder, 0.0, 1.0);
  spec.vx = vx;
  spec.vz = vz;
  spec.ground = level.mountain.base.y;
  // The cloud's base clears the summit, so a skier on top stands under the storm, not in it.
  spec.base = core::num::max(spec.base, level.mountain.summit.y - spec.ground + kBaseOverSummit);
  return wind::lay_storm(spec);
}

Wind wind_at(const mapgen::Level& level, const wind::Storm& storm, double t, double x, double z) {
  Wind out = wind_at(level, t);
  if (storm.cells == 0) return out;
  const wind::Air o = wind::outflow_at(storm, {x, 0, z, 10}, t, wind::kRoughness::snow);
  if (o.speed <= 0) return out;
  out.x += o.x;
  out.z += o.z;
  out.speed = core::hypot(out.x, out.z);
  out.gust = core::num::max(out.gust, o.gust);
  return out;
}

double cooled_at(const wind::Storm& storm, double t, double x, double z) {
  double most = 0;
  for (int k = 0; k < storm.cells; ++k) {
    const wind::CellAt c = wind::cell_at(storm, k, t);
    if (c.downdraft <= 0) continue;
    const double dx = x - c.x, dz = z - c.z, core = storm.cell[k].core;
    const double q = (dx * dx + dz * dz) / (core * core);
    most = core::num::max(most, c.downdraft / kFullDowndraft / ((1 + q) * (1 + q)));
  }
  return kCooled * core::num::min(1.0, most);
}

Fall fall_at(const mapgen::Level& level, const wind::Storm& storm, double t, double x, double y,
             double z) {
  Fall out = fall_at(level, t, y);
  if (storm.cells == 0) return out;
  const double rain = wind::rain_at(storm, x, z, t);
  if (rain <= 0) return out;
  const mapgen::Weather& w = level.weather;
  const double line = w.snowline > mapgen::kNoSnowline ? w.snowline : y + 1e6;
  const double freezing = line + 0.5 * wind::kMeltBand - cooled_at(storm, t, x, z);
  const double share = wind::rain_share_at(y, freezing);
  // The two falls as independent chances, the rain's share of them weighted by each.
  const double fall = 1 - (1 - out.fall) * (1 - rain);
  out.rain = fall > 0 ? (out.fall * out.rain + rain * share) / (out.fall + rain) : 0;
  out.fall = core::clamp(fall, 0.0, 1.0);
  out.visibility = visibility_in(out.fall);
  see_through_rain(out);
  return out;
}

double visibility_in(double fall) {
  return fall > 0 ? core::num::min(kRangeClear, kRangeFull / (fall * fall)) : kRangeClear;
}

}  // namespace ge::mountain::air

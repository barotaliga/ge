// Snow, sleet and rain: the rate, the tiers, the phase, the pace, the visibility, the swirl and the
// wet ground.
//
// ge:summary Snow, sleet and rain: the formulas behind precip.hpp
// ge:keywords precip rain sleet snow rate tier phase drop visibility swirl wet puddle
// ge:skill water render
// ge:test tests/wind_precip_test.cpp
#include "wind/precip.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {

namespace lm = core::libm;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double smooth(double t) {
  const double s = clamp01(t);
  return s * s * (3 - 2 * s);
}

// The two ends' rates, mm/h of water.
double rain_rate(double i) { return 0.64 * lm::pow(232.6, i); }
double snow_rate(double i) { return 0.04 * lm::pow(250.0, i); }

// a^(1-s) b^s: two positive numbers blended in the logarithm.
double log_mix(double a, double b, double s) { return a * lm::pow(b / a, s); }

constexpr double kClear = 20000;   // m: what one sees through dry air in these formulas
constexpr double kStormWind = 15;  // m/s

}  // namespace

const char* tier_name(Tier tier) {
  switch (tier) {
    case Tier::none:
      return "none";
    case Tier::light:
      return "light";
    case Tier::moderate:
      return "moderate";
    case Tier::heavy:
      return "heavy";
    case Tier::storm:
      return "storm";
  }
  return "none";
}

double rate_of(const Precip& p) {
  if (p.intensity < kNoFall) return 0;
  const double i = clamp01(p.intensity);
  return log_mix(snow_rate(i), rain_rate(i), clamp01(p.rain));
}

Tier tier_of(const Precip& p, double wind) {
  if (p.intensity < kNoFall) return Tier::none;
  const double rate = rate_of(p);
  const double rain = clamp01(p.rain);
  Tier t;
  if (rain >= 0.5) {
    t = rate < 2.5 ? Tier::light : rate < 7.6 ? Tier::moderate : Tier::heavy;
    if (rate >= 50) return Tier::storm;
  } else {
    const double seen = visibility_of(p);
    t = seen > 1000 ? Tier::light : seen > 500 ? Tier::moderate : Tier::heavy;
  }
  if (t == Tier::heavy && wind >= kStormWind) return Tier::storm;
  return t;
}

double rain_share_at(double y, double freezing) { return smooth((freezing - y) / kMeltBand); }

double drop_speed(double d) {
  if (d <= 0) return 0;
  return std::max(0.0, 9.65 - 10.3 * lm::exp(-0.6 * d));
}

double median_drop(double rate) {
  if (rate <= 0) return 0;
  return 3.67 / (4.1 * lm::pow(rate, -0.21));
}

double fall_speed(const Precip& p) {
  constexpr double kFlake = 1.0;  // m/s
  const double rain = clamp01(p.rain);
  const double rate = std::max(rate_of(p), rain_rate(kNoFall));
  const double drop = drop_speed(median_drop(rate));
  // A wet flake falls faster than a dry one well before it is a drop.
  return kFlake + (drop - kFlake) * rain * rain;
}

double visibility_of(const Precip& p) {
  const double rate = rate_of(p);
  if (rate <= 0) return kClear;
  const double rain = 1000 * 11.5 * lm::pow(rate, -0.63);
  const double snow = 1000 * 1.2 * lm::pow(rate, -0.9);
  return std::min(kClear, log_mix(snow, rain, clamp01(p.rain)));
}

double swirl_of(double speed, double turbulence) {
  return 0.3 + std::max(0.0, turbulence) * std::max(0.0, speed);
}

double dry_rate(double wind, double sun) {
  return 0.05 + 0.03 * std::max(0.0, wind) + 0.25 * clamp01(sun);
}

namespace {

Wet wet_of(double water) {
  Wet w;
  w.water = std::clamp(water, 0.0, kPools);
  w.wet = clamp01(w.water / kFilm);
  w.puddles = clamp01((w.water - kFilm) / (kPools - kFilm));
  return w;
}

}  // namespace

Wet step_wet(const Wet& w, double rate, double dry, double dt) {
  // Sub-steps of at most a minute: the drain's time constant is ten minutes.
  const double secs = std::max(0.0, dt);
  const int n = std::max(1, static_cast<int>(std::ceil(secs / 60 - 1e-9)));
  const double h = secs / 3600 / n;
  double water = w.water;
  for (int k = 0; k < n; ++k) {
    const double drain = kDrain * std::max(0.0, water - kFilm);
    water = std::clamp(water + (std::max(0.0, rate) - std::max(0.0, dry) - drain) * h, 0.0, kPools);
  }
  return wet_of(water);
}

Wet wet_steady(double rate, double dry) {
  const double gain = std::max(0.0, rate) - std::max(0.0, dry);
  if (gain <= 0) return wet_of(0);
  // Rain that outpaces the drying fills the film and stands over it until the drain matches.
  return wet_of(kFilm + gain / kDrain);
}

Cover step_cover(const Cover& c, const Precip& p, double sun, double dt) {
  const double h = std::max(0.0, dt) / 3600;
  const double rate = rate_of(p);
  const double rain = clamp01(p.rain);
  const double laid = kNewSnow * rate * (1 - rain);
  const double melt = 4 * rate * rain + 2 * clamp01(sun);
  Cover out;
  out.depth = std::clamp(c.depth + (laid - melt) * h, 0.0, kDeepestCover);
  out.cover = 1 - lm::exp(-out.depth / kCoverDepth);
  return out;
}

double strokes_per_minute(const Precip& p, double wind) {
  const Tier t = tier_of(p, wind);
  if (t != Tier::heavy && t != Tier::storm) return 0;
  const double rain = clamp01(p.rain);
  const double per = t == Tier::storm ? 1 + 5 * clamp01((p.intensity - 0.6) / 0.4) : 1;
  return per * (0.1 + 0.9 * rain);
}

namespace {

// A 64-bit mix of a slot and a seed (splitmix64's finaliser): the same bits on every machine.
uint64_t mix(uint64_t v) {
  v += 0x9e3779b97f4a7c15ull;
  v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
  v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
  return v ^ (v >> 31);
}

double unit_of(uint64_t v, int k) { return double((v >> (k * 21)) & 0x1fffff) / double(0x200000); }

// A stroke's light `s` s after it starts: the main stroke, then two returns, each a quick decay.
double stroke_light(double s) {
  if (s < 0 || s > 0.4) return 0;
  double l = lm::exp(-s / 0.045);
  if (s > 0.11) l += 0.6 * lm::exp(-(s - 0.11) / 0.035);
  if (s > 0.23) l += 0.35 * lm::exp(-(s - 0.23) / 0.03);
  return l;
}

constexpr double kSlot = 0.5;  // s

}  // namespace

double flash_at(double t, const Precip& p, double wind, uint32_t seed) {
  const double per_min = strokes_per_minute(p, wind);
  if (per_min <= 0) return 0;
  const double chance = per_min * kSlot / 60;
  const int64_t now = static_cast<int64_t>(std::floor(t / kSlot));
  double light = 0;
  // A stroke lasts under a slot, so this slot's and the last one's are all that can be lit.
  for (int64_t k = now - 1; k <= now; ++k) {
    const uint64_t h = mix(static_cast<uint64_t>(k) ^ (uint64_t(seed) << 40));
    if (unit_of(h, 0) >= chance) continue;
    const double start = (static_cast<double>(k) + unit_of(h, 1)) * kSlot;
    light += (0.4 + 0.6 * unit_of(h, 2)) * stroke_light(t - start);
  }
  return std::min(light, 1.5);
}

}  // namespace ge::wind

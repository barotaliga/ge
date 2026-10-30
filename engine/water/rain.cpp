// The rain's sound (rain.hpp): what is heard of a fall, the bed's layers, the single drops.
//
// ge:summary The rain's sound: hear_rain's levels by the rain's power, the bed, single drops
// ge:keywords rain sound hear_rain rain_power patter wash drum driven-snow rain_targets
// ge:keywords rain_drops plink tick bubble
// ge:skill water audio
// ge:test tests/water_rain_test.cpp
#include "water/rain.hpp"

#include <algorithm>
#include <cmath>

namespace ge::water {

namespace {

using audio::Color;
using audio::FilterType;
using audio::LayerSpec;

LayerSpec noise(Color color, FilterType filter, float q) {
  LayerSpec s;
  s.kind = LayerSpec::Kind::noise;
  s.color = color;
  s.filter = filter;
  s.q = q;
  return s;
}

float capped(double v) { return static_cast<float>(std::clamp(v, 0.0, 1.5)); }

double smooth(double a, double b, double x) {
  const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

// A rain this many drops a second near the ear is a patter, not drops heard one by one.
constexpr double kMostSingles = 60;

}  // namespace

double rain_power(double rate) {
  if (rate <= 0) return 0;
  const double v = wind::drop_speed(wind::median_drop(rate));
  return 0.5 * 1000 * (rate / 3.6e6) * v * v;
}

RainHearing hear_rain(const wind::Precip& p, double wind_ms, const RainPlace& place) {
  RainHearing h;
  h.wind = static_cast<float>(wind_ms);
  if (p.intensity < wind::kNoFall) return h;
  const double share = std::clamp(p.rain, 0.0, 1.0);
  const double all = wind::rate_of(p);
  const double rain = all * share, snow = all * (1 - share);
  const double open = std::clamp(place.open, 0.0, 1.0);
  const double lake = std::clamp(place.water, 0.0, 1.0);
  const double roof = std::clamp(place.roof, 0.0, 1.0);
  const double level = std::sqrt(rain_power(rain) / rain_power(kFullRain));
  h.patter = capped(level * open * (1 - lake));
  h.water = capped(level * open * lake);
  h.drum = capped(1.2 * level * roof);
  h.wash = capped(level * (0.5 + 0.5 * open));
  h.driven = capped(std::sqrt(snow / kFullDriven) * smooth(kDrivenFrom, kDrivenFull, wind_ms));
  h.drop = static_cast<float>(wind::median_drop(rain));
  h.drops = static_cast<float>(wind::drops_per_m2s(rain, 1.0) * std::max(open, roof));
  h.lake = static_cast<float>(lake * open / std::max(1e-6, std::max(open, roof)));
  return h;
}

const std::array<LayerSpec, kRainLayers> kRainSpecs = {
    noise(Color::pink, FilterType::bandpass, 0.7f),   // patter
    noise(Color::pink, FilterType::lowpass, 0.7f),    // wash
    noise(Color::white, FilterType::bandpass, 1.4f),  // on water
    noise(Color::pink, FilterType::bandpass, 0.9f),   // drum
    noise(Color::white, FilterType::highpass, 0.7f),  // driven snow
};

// The rain swells and eases slowly; the driven snow follows the gusts.
const std::array<float, kRainLayers> kRainGlides = {0.4f, 0.6f, 0.4f, 0.4f, 0.15f};

std::array<audio::LayerTarget, kRainLayers> rain_targets(const RainHearing& heard,
                                                         const WaterMix& mix) {
  const float bright = 0.5f + 0.5f * audio::clamp01(mix.tone);
  const float drop = heard.drop;
  std::array<audio::LayerTarget, kRainLayers> t{};
  t[kPatter].level = 0.05f * heard.patter * mix.level;
  t[kPatter].cutoff = (1600 + 900 * drop) * bright;
  // The rain out of sight: a low wash, brightening as it comes down harder.
  t[kWash].level = 0.035f * heard.wash * mix.level;
  t[kWash].cutoff = (700 + 2200 * std::min(1.0f, heard.wash)) * bright;
  // A millimetre's drops ring near 14 kHz; a heavy rain's bigger ones lower.
  t[kOnWater].level = 0.03f * heard.water * mix.level;
  t[kOnWater].cutoff = 14000 / (1 + 0.5f * std::max(0.0f, drop - 1));
  t[kDrum].level = 0.05f * heard.drum * mix.level;
  t[kDrum].cutoff = 450 + 250 * drop;
  t[kDriven].level = 0.03f * heard.driven * mix.level;
  t[kDriven].cutoff = (2500 + 150 * heard.wind) * bright;
  return t;
}

RainBed make_rain_bed(int bus, uint32_t seed) {
  RainBed bed;
  bed.rack.specs = kRainSpecs;
  bed.rack.glides_s = kRainGlides;
  bed.rack.bus = bus;
  bed.rng = core::make_rng(seed);
  return bed;
}

void rain_bed_update(RainBed& bed, audio::Synth& synth, const RainHearing& heard, float dt,
                     const WaterMix& mix) {
  audio::rack_apply(bed.rack, synth, rain_targets(heard, mix));
  rain_drops(synth, bed.rng, heard, dt * mix.level, bed.rack.bus);
}

void rain_bed_silence(RainBed& bed, audio::Synth& synth) { audio::rack_stop(bed.rack, synth); }

int rain_drops(audio::Synth& synth, core::Rng& rng, const RainHearing& heard, float dt, int bus) {
  const double near = heard.drops * kNearDrops;
  if (near <= 0 || near > kMostSingles || dt <= 0) return 0;
  const int n = std::min(kMostDrops, static_cast<int>(near * dt + core::next(rng)));
  const double size = std::clamp(static_cast<double>(heard.drop), 0.5, 3.0);
  // The fewer, the more each is heard: they fade into the patter as it thickens.
  const float alone = static_cast<float>(1 - near / kMostSingles);
  for (int i = 0; i < n; ++i) {
    const float delay = static_cast<float>(core::next(rng)) * dt * 1000;
    const float pan = static_cast<float>(core::range(rng, -0.8, 0.8));
    if (core::next(rng) < heard.lake) {
      // A drop into water traps a bubble of a fifth of a millimetre and more as it grows.
      const double r = 0.0002 * (0.9 + 0.3 * size * core::next(rng));
      synth.tone(bubble(r, 0.035f * alone, delay, pan), bus);
    } else {
      synth.noise(audio::Noise{.duration_ms = 5,
                               .volume = static_cast<float>(0.03 * size) * alone,
                               .delay_ms = delay,
                               .color = Color::pink,
                               .filter = {.type = FilterType::bandpass,
                                          .hz = static_cast<float>(2400 + 600 * size),
                                          .q = 1.5f},
                               .attack_ms = audio::kMinAttackMs,
                               .pan = pan},
                  bus);
    }
  }
  return n;
}

}  // namespace ge::water

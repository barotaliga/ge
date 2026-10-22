// The wind bed's layers and their targets (wind_bed.hpp).
//
// ge:summary The wind bed's specs, glides and the targets function
// ge:keywords wind bed rush roar whistle flutter buffet targets
// ge:skill skier audio
// ge:test tests/skier_ride_bed_test.cpp
#include "skier/app/wind_bed.hpp"

namespace ge::skier {

namespace {

using audio::Color;
using audio::FilterType;
using audio::LayerSpec;

// Where the whistle and the buffet come in and are fully up, m/s of air.
constexpr float kWhistleFrom = 11, kWhistleFull = 32;
constexpr float kBuffetFrom = 25, kBuffetFull = 65;

float ramp(float v, float lo, float hi) { return audio::clamp01((v - lo) / (hi - lo)); }

LayerSpec noise(Color color, FilterType filter, float q) {
  LayerSpec s;
  s.kind = LayerSpec::Kind::noise;
  s.color = color;
  s.filter = filter;
  s.q = q;
  return s;
}

}  // namespace

const std::array<LayerSpec, kWindLayers> kWindSpecs = {
    noise(Color::pink, FilterType::lowpass, 0.5f),   // rush
    noise(Color::brown, FilterType::lowpass, 0.9f),  // roar
    noise(Color::white, FilterType::bandpass, 6),    // whistle
    noise(Color::pink, FilterType::bandpass, 1.6f),  // flutter
    noise(Color::brown, FilterType::lowpass, 1.4f),  // buffet
};

const std::array<float, kWindLayers> kWindGlides = {0.15f, 0.2f, 0.12f, 0.1f, 0.25f};

std::array<audio::LayerTarget, kWindLayers> wind_targets(const WindVoice& voice,
                                                         const WindMix& mix) {
  // `gust` is 1 at a schuss and runs on past it to kWindTop; `full` is the same held at 1, for
  // what a gale does not make any brighter.
  const float gust = ramp(voice.wind, 0, kWindTop) * (kWindTop / kWindFull);
  const float full = gust < 1 ? gust : 1;
  const float tuck = audio::clamp01(voice.tuck);
  const float whistle = ramp(voice.wind, kWhistleFrom, kWhistleFull);
  const float buffet = ramp(voice.wind, kBuffetFrom, kBuffetFull);
  const float bright = 0.5f + 0.5f * mix.tone;
  const float side = voice.side < -1 ? -1 : voice.side > 1 ? 1 : voice.side;
  const float across = side < 0 ? -side : side;
  // The windward ear: the rush leans to it a little, the cloth and the whistle (at the side of
  // the head) more. A wind across him lands on the ear rather than streaming past the helmet.
  const float pan = side * 0.35f;
  const float loud = gust <= 1 ? gust * gust : gust * gust * gust;

  std::array<audio::LayerTarget, kWindLayers> t{};
  t[kRush].level = loud * (voice.airborne ? 0.034f : 0.022f) * (1 - 0.35f * tuck) *
                   (1 + 1.2f * across) * mix.wind;
  t[kRush].cutoff = (400 + 2200 * full) * bright;
  t[kRush].pan = pan;
  t[kRoar].level = gust * (0.004f + 0.03f * tuck) * mix.wind;
  t[kRoar].cutoff = 120 + 260 * full;
  t[kWhistle].level = whistle * whistle * 0.005f * (1 + 0.6f * across) * mix.wind;
  t[kWhistle].cutoff = (1800 + 2600 * whistle) * bright;
  t[kWhistle].pan = pan * 2;
  t[kFlutter].level = gust * (0.008f * (1 - tuck) + 0.001f) * (1 + 0.8f * across) * mix.wind;
  t[kFlutter].cutoff = 700 + 900 * full;
  t[kFlutter].pan = pan * 2;
  t[kBuffet].level = buffet * buffet * 0.06f * mix.wind;
  t[kBuffet].cutoff = 70 + 150 * buffet;
  return t;
}

}  // namespace ge::skier

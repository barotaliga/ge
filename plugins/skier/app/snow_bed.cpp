// The skis-on-snow bed's layers and their targets (snow_bed.hpp).
//
// ge:summary The snow bed's specs, glides, the pair as heard and the targets function
// ge:keywords snow bed hiss powder edge chatter skid targets ski-voice
// ge:skill skier audio
// ge:test tests/skier_ride_bed_test.cpp
#include "skier/app/snow_bed.hpp"

namespace ge::skier {

namespace {

using audio::clamp01;
using audio::Color;
using audio::FilterType;
using audio::LayerSpec;

// The speed a skier in powder is up on top of it rather than ploughing, m/s: past it the hush
// thins. And where the chatter starts and is fully up.
constexpr float kPlaneSpeed = 12;
constexpr float kChatterFrom = 12, kChatterFull = 30;

float ramp(float v, float lo, float hi) { return clamp01((v - lo) / (hi - lo)); }

LayerSpec noise(Color color, FilterType filter, float q) {
  LayerSpec s;
  s.kind = LayerSpec::Kind::noise;
  s.color = color;
  s.filter = filter;
  s.q = q;
  return s;
}

}  // namespace

SkiVoice ski_voice_of(const SkiSpec& spec) {
  return {clamp01(static_cast<float>((spec.waist - 0.066) / 0.05)),
          clamp01(static_cast<float>((spec.length - 1.55) / 0.63))};
}

const std::array<LayerSpec, kSnowLayers> kSnowSpecs = {
    noise(Color::pink, FilterType::bandpass, 0.6f),   // hiss
    noise(Color::brown, FilterType::lowpass, 0.8f),   // powder
    noise(Color::white, FilterType::bandpass, 1.4f),  // edge
    noise(Color::white, FilterType::bandpass, 4),     // chatter
    noise(Color::pink, FilterType::bandpass, 0.9f),   // skid
};

const std::array<float, kSnowLayers> kSnowGlides = {0.08f, 0.1f, 0.05f, 0.04f, 0.06f};

std::array<audio::LayerTarget, kSnowLayers> snow_targets(const SnowVoice& voice,
                                                         const SnowMix& mix) {
  const float pace = clamp01(voice.pace);
  const float level = voice.on_snow ? mix.snow : 0;
  const float moving = ramp(voice.speed, 0, 3);
  const float plough = 1 - 0.6f * ramp(voice.speed, 3, kPlaneSpeed);
  const float groomed = clamp01(voice.packed);
  const float loose = 1 - groomed;
  const float edge = clamp01(voice.edge);
  const float skid = clamp01(voice.skid);
  const float chatter = ramp(voice.speed, kChatterFrom, kChatterFull);
  // How firm the snow is under the steel: a groomer firms up with the speed the edge is driven
  // into it at.
  const float firm = groomed * (0.3f + 0.7f * pace);

  std::array<audio::LayerTarget, kSnowLayers> t{};
  t[kHiss].level = level * groomed * moving * (0.004f + 0.02f * pace * (0.8f + 0.2f * pace));
  t[kHiss].cutoff = (1800 + 3600 * pace) * (1.12f - 0.24f * voice.ski.length);
  t[kPowder].level =
      level * loose * moving * plough * (0.012f + 0.02f * pace) * (0.85f + 0.35f * voice.ski.width);
  t[kPowder].cutoff = (260 + 700 * pace) * (1.1f - 0.25f * voice.ski.width);
  t[kEdgeTear].level =
      level * groomed * edge * (1 - 0.7f * skid) * ramp(voice.speed, 3, 25) * 0.014f;
  t[kEdgeTear].cutoff = 2600 + 2400 * pace;
  t[kChatter].level = level * groomed * firm * edge * chatter * chatter * 0.012f;
  t[kChatter].cutoff = 900 + 1400 * chatter;
  t[kSkidRasp].level = level * skid * ramp(voice.speed, 1, 20) * (0.006f + 0.016f * groomed);
  t[kSkidRasp].cutoff = 500 + 1200 * pace + 600 * skid + 500 * firm;
  return t;
}

}  // namespace ge::skier

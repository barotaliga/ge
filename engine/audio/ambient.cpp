// The ambient bed's step: each level walked toward its target, its voice started, steered and
// stopped with it.
//
// ge:summary The ambient bed's step: the levels walked to their targets, the voices kept in step
// ge:keywords ambient bed step level target voice start stop steer
// ge:skill audio
// ge:test tests/audio_ambient_test.cpp
#include "audio/ambient.hpp"

namespace ge::audio {

namespace {

constexpr float kStopFadeS = 0.05f;

}  // namespace

AmbientBed::AmbientBed(float rate, float floor, int bus) : rate_(rate), floor_(floor), bus_(bus) {}

int AmbientBed::add(const Pcm& pcm, float gain) {
  AmbientLayer l;
  l.pcm = &pcm;
  l.gain = gain;
  layers_.push_back(l);
  return static_cast<int>(layers_.size()) - 1;
}

void AmbientBed::target(int i, float level) {
  if (i < 0 || i >= static_cast<int>(layers_.size())) return;
  level = level > 1 ? 1 : level;
  layers_[static_cast<size_t>(i)].target = level < floor_ ? 0 : level;
}

void AmbientBed::step(Synth& synth, float dt) {
  const float most = rate_ * (dt > 0 ? dt : 0);
  for (AmbientLayer& l : layers_) {
    const float d = l.target - l.level;
    l.level = d > most ? l.level + most : d < -most ? l.level - most : l.target;
    if (l.level <= 0) {
      if (l.voice.valid()) synth.sample_stop(l.voice, kStopFadeS);
      l.voice = SampleId{};
      continue;
    }
    if (!l.voice.valid() || !synth.sample_live(l.voice)) {
      SamplePlay play;
      play.gain = l.level * l.gain;
      play.loop = Loop::whole;
      l.voice = synth.sample(*l.pcm, play, bus_);
    } else {
      synth.sample_set(l.voice, l.level * l.gain, 0, dt);
    }
  }
}

void AmbientBed::silence(Synth& synth) {
  for (AmbientLayer& l : layers_) {
    if (l.voice.valid()) synth.sample_stop(l.voice, kStopFadeS);
    l.voice = SampleId{};
    l.level = l.target = 0;
  }
}

}  // namespace ge::audio

// What a sound is (engine/audio/sound.hpp): a play's shape scales every voice's level, every
// frequency and cutoff, every duration and delay, and replaces the pan only when asked; a tone
// with no glide keeps none; `play_def` fires every voice; an unknown id is nullptr.
//
// ge:summary Tests for sounds: the play's shape, play_def and the bank's lookup
// ge:keywords test sound shape shaped play_def find_sound bank
// ge:skill audio testing
#include <vector>

#include "audio/meter.hpp"
#include "audio/sound.hpp"
#include "check.hpp"

namespace {

using namespace ge::audio;

constexpr SoundVoice kVoices[] = {
    tone_voice({.wave = Wave::sine,
                .from_hz = 400,
                .duration_ms = 100,
                .volume = 0.05f,
                .pan = 0.2f,
                .filter = {.type = FilterType::lowpass, .hz = 2000, .to_hz = 500}}),
    noise_voice({.duration_ms = 60, .volume = 0.04f, .delay_ms = 20, .color = Color::pink}),
};
constexpr SoundDef kBank[] = {
    {"blip", "a small bright blip with a puff under it", kVoices},
};

}  // namespace

TEST(a_shape_scales_level_pitch_and_length) {
  const SoundVoice t = shaped(kVoices[0], {.gain = 2, .pitch = 0.5f, .stretch = 3});
  CHECK(t.tone.volume == 0.1f && t.tone.from_hz == 200 && t.tone.to_hz == 0);
  CHECK(t.tone.duration_ms == 300 && t.tone.filter.hz == 1000 && t.tone.filter.to_hz == 250);
  CHECK(t.tone.pan == 0.2f);
  const SoundVoice n = shaped(kVoices[1], {.stretch = 2, .place = true, .pan = -1});
  CHECK(n.noise.delay_ms == 40 && n.noise.duration_ms == 120 && n.noise.pan == -1);
}

TEST(play_def_fires_every_voice_and_find_sound_misses_quietly) {
  const SoundDef* def = find_sound(kBank, "blip");
  CHECK(def != nullptr);
  CHECK(find_sound(kBank, "nope") == nullptr);
  if (!def) return;
  Synth s;
  play_def(s, *def);
  std::vector<float> out(size_t{2} * 4800);
  s.render(out.data(), 4800);
  CHECK(s.stats().voices == 0);  // both ended within the tenth of a second
  CHECK(measure(out).loudest_db > -40);
  Synth louder;
  play_def(louder, *def, {.gain = 2});
  std::vector<float> out2(size_t{2} * 4800);
  louder.render(out2.data(), 4800);
  const float lift = measure(out2).loudest_db - measure(out).loudest_db;
  CHECK(lift > 5.9f && lift < 6.1f);
}

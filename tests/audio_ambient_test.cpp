// The ambient bed (engine/audio/ambient.hpp): a level walks to its target at the bed's rate and
// no faster, a target under the floor is silence, a layer sounds only while its level is above
// zero, its voice is as loud as its level, and silencing a bed stops every voice.
//
// ge:summary Tests for the ambient bed: the rate, the floor, voices started and stopped, the gain
// ge:keywords test ambient bed level target rate floor voice fade silence
// ge:skill audio testing
#include <string>
#include <vector>

#include "audio/ambient.hpp"
#include "audio/synth.hpp"
#include "check.hpp"

namespace {

using namespace ge::audio;

Pcm level(float v) {
  Pcm p;
  p.rate = kRate;
  p.samples.assign(static_cast<size_t>(kRate / 2), v);
  return p;
}

// The peak of the left side over `seconds` of the synth.
float peak(Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  float p = 0;
  for (size_t i = out.size() / 2; i < out.size(); i += 2) p = out[i] > p ? out[i] : p;
  return p;
}

}  // namespace

TEST(a_level_walks_to_its_target_at_the_bed_s_rate) {
  Synth s;
  const Pcm water = level(0.5f);
  AmbientBed bed(0.5f, 0.03f);
  const int w = bed.add(water);
  bed.target(w, 1);
  bed.step(s, 0.5f);
  CHECK(bed.layers()[0].level == 0.25f);
  bed.step(s, 1.0f);
  CHECK(bed.layers()[0].level == 0.75f);
  bed.step(s, 1.0f);
  CHECK(bed.layers()[0].level == 1.0f);  // it stops on the target
  bed.target(w, 0.6f);
  bed.step(s, 0.2f);
  CHECK(bed.layers()[0].level == 0.9f);
}

TEST(a_target_under_the_floor_is_silence_and_frees_the_voice) {
  Synth s;
  const Pcm water = level(0.5f);
  AmbientBed bed(10.0f, 0.03f);
  const int w = bed.add(water);
  bed.target(w, 0.02f);
  bed.step(s, 0.1f);
  CHECK(bed.layers()[0].level == 0);
  CHECK(!bed.layers()[0].voice.valid());
  bed.target(w, 0.5f);
  bed.step(s, 0.1f);
  CHECK(bed.layers()[0].voice.valid());
  peak(s, 0.05);
  CHECK(s.stats().samples == 1);
  bed.target(w, 0);
  bed.step(s, 0.1f);
  CHECK(!bed.layers()[0].voice.valid());
  peak(s, 0.5);  // the fade, then the slot freed
  CHECK(s.stats().samples == 0);
}

TEST(a_voice_is_as_loud_as_its_level) {
  Synth s;
  const Pcm sky = level(0.1f);  // quiet enough that the limiter stays out of it
  AmbientBed bed(100.0f, 0.03f);
  const int k = bed.add(sky, 0.8f);
  bed.target(k, 0.5f);
  bed.step(s, 0.1f);
  const float at_half = peak(s, 0.1);
  // 0.5 × 0.8 × 0.1
  CHECK_MSG(at_half > 0.039f && at_half < 0.041f, "peak " + std::to_string(at_half));
  bed.target(k, 1);
  for (int i = 0; i < 20; i++) {
    bed.step(s, 0.02f);
    peak(s, 0.02);
  }
  const float at_full = peak(s, 0.1);
  CHECK_MSG(at_full > 0.079f && at_full < 0.081f, "peak " + std::to_string(at_full));
}

TEST(silencing_a_bed_stops_every_voice) {
  Synth s;
  const Pcm a = level(0.5f), b = level(0.3f);
  AmbientBed bed(100.0f, 0.03f);
  bed.target(bed.add(a), 1);
  bed.target(bed.add(b), 1);
  bed.step(s, 0.1f);
  peak(s, 0.05);
  CHECK(s.stats().samples == 2);
  bed.silence(s);
  peak(s, 0.5);
  CHECK(s.stats().samples == 0);
  for (const AmbientLayer& l : bed.layers()) CHECK(l.level == 0 && !l.voice.valid());
}

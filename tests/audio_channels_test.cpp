// Sampled voices (engine/audio/synth.hpp's `sample`) and sounds in a world (channels.hpp): a
// recording plays at its own rate for its own length, exact on its frames and with no stair
// steps between them, loops where it is told, is steered and stopped like a layer, and a full
// pool steals the quietest; `hear` falls with distance and pans by the listener's right; the
// channel table replaces an entity's channel, stacks channel 0, steals the quietest, skips what
// nobody can hear, centres the listener's own sounds, and follows a moving source.
//
// ge:summary Tests for sampled voices and Channels: rate, loop, steer, steal, hear, replace
// ge:keywords test sample sampled-voice resample loop pitch steer steal channels hear pan
// ge:keywords attenuation replace stack listener move
// ge:skill audio testing
#include <string>
#include <vector>

#include "audio/channels.hpp"
#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "core/libm/libm.hpp"

namespace {

using namespace ge::audio;
using ge::core::Vec3;

std::vector<float> render(Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

// `seconds` of a sine at `hz`, recorded at `rate`, amplitude `a`.
Pcm sine(int rate, double hz, double seconds, float a = 0.5f) {
  Pcm p;
  p.rate = rate;
  p.samples.resize(static_cast<size_t>(seconds * rate));
  for (size_t i = 0; i < p.samples.size(); i++) {
    p.samples[i] = a * static_cast<float>(ge::core::libm::sin(6.283185307179586 * hz *
                                                              static_cast<double>(i) / rate));
  }
  return p;
}

Pcm level(int rate, double seconds, float v) {
  Pcm p;
  p.rate = rate;
  p.samples.assign(static_cast<size_t>(seconds * rate), v);
  return p;
}

float side_peak(const std::vector<float>& v, int side, size_t from = 0, size_t to = SIZE_MAX) {
  float p = 0;
  for (size_t i = 2 * from + side; i < v.size() && i < 2 * to; i += 2) {
    p = v[i] > p ? v[i] : -v[i] > p ? -v[i] : p;
  }
  return p;
}

}  // namespace

TEST(a_recording_plays_at_its_own_rate_for_its_own_length) {
  const Pcm p = sine(22050, 1000, 0.5);
  Synth s;
  CHECK(s.sample(p, {.gain = 0.2f}).valid());
  const Meter m = measure(render(s, 1.0));
  CHECK_MSG(m.length_s > 0.48f && m.length_s < 0.52f,
            "half a second at 22 050 Hz lasts " + std::to_string(m.length_s) + " s at 48 kHz");
  CHECK_MSG(m.centroid_hz > 800 && m.centroid_hz < 1250,
            "a 1 kHz recording's centroid is " + std::to_string(m.centroid_hz) + " Hz");
  int top = 0;
  for (int b = 1; b < kBands; b++) top = m.band_db[b] > m.band_db[top] ? b : top;
  CHECK(kBandHz[top] == 1000);
  // No stair steps: against the same sine computed at 48 kHz, the played one is off by less
  // than a hundredth of its level (RMS over the middle of the sound, -40 dB).
  Synth again;
  again.sample(p, {.gain = 0.2f});
  const std::vector<float> out = render(again, 0.5);
  double err = 0, sig = 0;
  for (size_t i = 2000; i < 22000; i++) {
    const double want =
        0.1 * ge::core::libm::sin(6.283185307179586 * 1000 * static_cast<double>(i) / kRate);
    err += (out[2 * i] - want) * (out[2 * i] - want);
    sig += want * want;
  }
  CHECK_MSG(err < sig * 1e-4,
            "the resampled sine is off by " + std::to_string(err / sig) + " of its power");
  Synth up;
  up.sample(p, {.gain = 0.2f, .pitch = 2});
  const Meter fast = measure(render(up, 1.0));
  CHECK(fast.length_s > 0.23f && fast.length_s < 0.27f);
  CHECK(fast.centroid_hz > 1600 && fast.centroid_hz < 2500);
}

TEST(a_level_plays_exact_on_both_sides_and_ends) {
  const Pcm p = level(22050, 0.1, 0.5f);
  Synth s;
  const SampleId id = s.sample(p, {.gain = 0.2f});
  CHECK(s.sample_live(id) && s.stats().samples == 0);
  std::vector<float> out = render(s, 0.05);
  CHECK(s.stats().samples == 1);
  // Catmull-Rom through four equal frames is that level: 0.5 x 0.2 on both sides, centred (and
  // under the limiter's knee, so it passes untouched).
  for (size_t i = 20; i < out.size(); i++) CHECK(out[i] == 0.1f);
  render(s, 0.1);
  CHECK(!s.sample_live(id) && s.stats().samples == 0);
}

TEST(loops_by_the_file_by_the_whole_or_never) {
  Pcm p = level(22050, 0.1, 0.5f);
  p.loop_start = 1000;
  p.loop_end = 2000;
  Synth s;
  const SampleId file = s.sample(p, {.gain = 0.1f});
  const SampleId once = s.sample(p, {.gain = 0.1f, .loop = Loop::once});
  Pcm plain = level(22050, 0.1, 0.5f);
  const SampleId whole = s.sample(plain, {.gain = 0.1f, .loop = Loop::whole});
  const SampleId never = s.sample(plain, {.gain = 0.1f});
  render(s, 0.5);
  CHECK(s.sample_live(file) && !s.sample_live(once) && s.sample_live(whole) &&
        !s.sample_live(never));
  s.sample_stop(file, 0.01f);
  s.sample_stop(whole, 0.01f);
  render(s, 0.1);
  CHECK(s.stats().samples == 0);
}

TEST(a_sampled_voice_is_steered_and_fades_when_stopped) {
  const Pcm p = level(48000, 2, 0.5f);
  Synth s;
  const SampleId id = s.sample(p, {.gain = 0.4f, .loop = Loop::whole});
  render(s, 0.1);
  s.sample_set(id, 0.4f, 1, 0.02f);
  const std::vector<float> right = render(s, 0.3);
  const size_t late = right.size() / 2 - 2000;
  CHECK_MSG(side_peak(right, 0, late) < 1e-4f && side_peak(right, 1, late) > 0.19f,
            "panned hard right, the left side is silent and the right holds the level");
  s.sample_stop(id, 0.02f);
  const std::vector<float> gone = render(s, 0.3);
  CHECK(side_peak(gone, 1, 9600) < 1e-4f);
  CHECK(!s.sample_live(id));
  s.sample_set(id, 1, 0);  // an ended voice ignores its steering
  CHECK(side_peak(render(s, 0.1), 1) == 0);
}

TEST(a_full_pool_steals_the_quietest) {
  const Pcm p = level(48000, 1, 0.1f);
  Synth s;
  std::vector<SampleId> ids;
  ids.reserve(kMaxSamples);
  for (int i = 0; i < kMaxSamples; i++) {
    ids.push_back(s.sample(p, {.gain = i == 5 ? 0.01f : 0.5f, .loop = Loop::whole}));
  }
  render(s, 0.01);
  const SampleId late = s.sample(p, {.gain = 0.5f, .loop = Loop::whole});
  render(s, 0.01);
  CHECK(s.stats().stolen == 1 && s.stats().samples == kMaxSamples);
  CHECK(!s.sample_live(ids[5]) && s.sample_live(late) && s.sample_live(ids[0]));
}

TEST(hearing_falls_with_distance_and_pans_by_the_right) {
  const Listener l{.at = {0, 0, 0}, .right = {1, 0, 0}};
  const Heard near = hear(l, {0, 0, 100}, 1, 0.001f);
  CHECK(near.gain > 0.8999f && near.gain < 0.9001f && near.pan == 0);
  const Heard right = hear(l, {500, 0, 0}, 0.5f, 0.001f);
  CHECK(right.gain > 0.2499f && right.gain < 0.2501f && right.pan == 1);
  const Heard left = hear(l, {-300, 0, 300}, 1, 0.001f);
  CHECK(left.pan < -0.707f && left.pan > -0.708f);
  CHECK(hear(l, {0, 1200, 0}, 1, 0.001f).gain == 0);
  CHECK(hear(l, {0, 0, 0}, 1, 0.001f).pan == 0);
  CHECK(hear(l, {9000, 0, 0}, 1, 0).gain == 1);
  CHECK(hear(l, {100, 0, 0}, 1, 0.001f, 0.5f).pan == 0.5f);
}

TEST(an_entity_channel_replaces_and_channel_zero_stacks) {
  const Pcm p = level(48000, 1, 0.1f);
  Synth s;
  Channels ch(s, 8);
  ch.update({});
  const SampleId a = ch.play(p, {.entity = 3, .channel = 1, .at = {10, 0, 0}});
  const SampleId b = ch.play(p, {.entity = 3, .channel = 1, .at = {10, 0, 0}});
  ch.play(p, {.entity = 3, .channel = 0});
  ch.play(p, {.entity = 3, .channel = 0});
  render(s, 0.1);
  ch.update({});
  CHECK(ch.playing() == 3 && !s.sample_live(a) && s.sample_live(b));
  ch.stop(3, 1);
  CHECK(ch.playing() == 2);
  ch.stop(3);
  CHECK(ch.playing() == 0);
}

TEST(a_full_table_steals_the_quietest_and_skips_the_inaudible) {
  const Pcm p = level(48000, 1, 0.1f);
  Synth s;
  Channels ch(s, 4);
  ch.update({});
  for (int i = 0; i < 4; i++) {
    ch.play(p, {.entity = i, .at = {0, 0, 100.0 + 200 * i}, .attenuation = 0.001f});
  }
  CHECK(!ch.play(p, {.entity = 9, .at = {0, 0, 5000}, .attenuation = 0.001f}).valid());
  CHECK(!ch.play(p, {.entity = 9, .at = {0, 0, 850}, .attenuation = 0.001f}).valid());
  CHECK(ch.skipped() == 2 && ch.stolen() == 0);
  const SampleId near = ch.play(p, {.entity = 9, .at = {0, 0, 50}, .attenuation = 0.001f});
  CHECK(near.valid() && ch.stolen() == 1);
  bool far_gone = true;
  for (const Channels::Slot& sl : ch.slots()) far_gone = far_gone && sl.entity != 3;
  CHECK_MSG(far_gone, "the farthest source (entity 3, 700 away) gave way");
}

TEST(the_listeners_own_sounds_are_centred_and_a_moving_source_is_followed) {
  const Pcm p = level(48000, 2, 0.1f);
  Synth s;
  Channels ch(s, 8);
  const Listener l{.at = {0, 0, 0}, .right = {1, 0, 0}, .entity = 1};
  ch.update(l);
  ch.play(p, {.entity = 1, .channel = 1, .at = {300, 0, 0}, .volume = 0.7f});
  ch.play(p, {.entity = 2, .channel = 1, .at = {-200, 0, 0}, .loop = Loop::whole});
  CHECK(ch.slots()[0].heard.gain == 0.7f && ch.slots()[0].heard.pan == 0);
  CHECK(ch.slots()[1].heard.pan == -1);
  ch.move(2, {200, 0, 0});
  ch.update(l);
  CHECK(ch.slots()[1].heard.pan == 1 && ch.slots()[1].heard.gain > 0.799f);
  // Rendered, the moving source ends up in the right ear only.
  const std::vector<float> out = render(s, 0.5);
  ch.stop(1);
  const std::vector<float> late = render(s, 0.3);
  CHECK(side_peak(late, 0, 12000) < 1e-4f && side_peak(late, 1, 12000) > 0.07f);
  CHECK(out.size() > 0);
}

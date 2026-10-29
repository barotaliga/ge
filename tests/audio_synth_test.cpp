// The synth (engine/audio/synth.hpp), the rack and the vocabulary's rules:
//   * THE BITS: a reference set of calls renders the same bytes twice in one process, and the
//     FNV-1a 64 of each render matches tests/golden/audio_synth.txt, which this test writes
//     itself under GE_GOLDEN_WRITE=1 (a deliberate change to the DSP rewrites it, and the PR
//     says why and shows the `synth` lab's meters before and after).
//   * THE SHAPE: an envelope starts at its delay, peaks near its volume and ends at its length;
//     a one-shot's onset is never a step; a pan, a bus's gain and the echo do what they say; a
//     layer glides to its target, holds silence at zero and frees its slot when stopped; the
//     limiter keeps a stack of loud voices under full scale; a full ring and a full pool count
//     what they lost.
//
// ge:summary Tests for the synth: golden render hashes, envelopes, pan, buses, layers, limiter
// ge:keywords test synth render golden hash envelope onset pan bus echo layer glide limiter
// ge:keywords ring steal rack determinism
// ge:skill audio testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/rack.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "golden.hpp"

namespace {

using namespace ge::audio;

std::vector<float> render(Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

uint64_t fnv(const std::vector<float>& v) {
  uint64_t h = 0xcbf29ce484222325ull;
  const auto* b = reinterpret_cast<const unsigned char*>(v.data());
  for (size_t i = 0; i < v.size() * sizeof(float); i++) {
    h ^= b[i];
    h *= 0x100000001b3ull;
  }
  return h;
}

float peak_of(const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX) {
  float p = 0;
  for (size_t i = from; i < v.size() && i < to; i++) p = v[i] > p ? v[i] : -v[i] > p ? -v[i] : p;
  return p;
}

// THE REFERENCE CALLS: one of everything the graph does.
struct Case {
  const char* name;
  void (*play)(Synth&);
  double seconds;
};

// A recording for the sampled voice's case: a falling saw at 22 050 Hz, looped over its back half.
const Pcm& saw_recording() {
  static const Pcm pcm = [] {
    Pcm p;
    p.rate = 22050;
    p.samples.resize(4410);
    for (size_t i = 0; i < p.samples.size(); i++) {
      p.samples[i] = 0.5f - static_cast<float>(i % 50) / 50.0f;
    }
    p.loop_start = 2205;
    p.loop_end = 4410;
    return p;
  }();
  return pcm;
}

const Case kCases[] = {
    {"sine", [](Synth& s) { s.tone({.wave = Wave::sine, .from_hz = 440, .duration_ms = 300}); },
     0.4},
    {"square-glide",
     [](Synth& s) {
       s.tone({.wave = Wave::square, .from_hz = 220, .to_hz = 880, .duration_ms = 300});
     },
     0.4},
    {"saw-pair-vibrato-drive",
     [](Synth& s) {
       s.tone({.wave = Wave::saw,
               .from_hz = 110,
               .duration_ms = 400,
               .attack_ms = 20,
               .hold_ms = 100,
               .detune_cents = 12,
               .vibrato = {.rate_hz = 6, .depth_cents = 30, .delay_ms = 50},
               .pan = 0.4f,
               .filter = {.type = FilterType::lowpass, .hz = 300, .to_hz = 4000, .q = 2},
               .drive = 0.6f});
     },
     0.5},
    {"noise-colours",
     [](Synth& s) {
       s.noise({.duration_ms = 200, .color = Color::white});
       s.noise({.duration_ms = 200, .delay_ms = 200, .color = Color::pink, .attack_ms = 30});
       s.noise({.duration_ms = 200,
                .delay_ms = 400,
                .color = Color::brown,
                .filter = {.type = FilterType::bandpass, .hz = 200, .q = 3},
                .hold_ms = 50,
                .echo = 0.5f});
     },
     1.0},
    {"layers",
     [](Synth& s) {
       const LayerId a = s.layer(
           {.kind = LayerSpec::Kind::noise, .color = Color::pink, .filter = FilterType::lowpass});
       const LayerId b = s.layer({.kind = LayerSpec::Kind::tone,
                                  .wave = Wave::triangle,
                                  .detune_cents = 8,
                                  .drive = true,
                                  .filter = FilterType::bandpass,
                                  .q = 2,
                                  .vibrato = {.rate_hz = 3, .depth_cents = 10}});
       s.set(a, {.level = 0.05f, .cutoff = 2000, .pan = -0.5f}, 0.1f);
       s.set(b, {.level = 0.04f, .hz = 90, .cutoff = 600, .grit = 0.8f}, 0.05f);
     },
     0.5},
    {"samples",
     [](Synth& s) {
       const SampleId a = s.sample(saw_recording(), {.gain = 0.1f, .pan = -0.3f, .echo = 0.2f});
       s.sample(saw_recording(), {.gain = 0.05f, .pitch = 1.5f, .delay_ms = 100});
       s.sample_set(a, 0.08f, 0.6f, 0.05f);
     },
     0.5},
};

constexpr const char* kGolden = "tests/golden/audio_synth.txt";

// Whether an environment variable is set (MSVC deprecates std::getenv in favour of its own).
bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

}  // namespace

TEST(the_reference_renders_are_deterministic_and_held_by_the_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  std::vector<std::vector<std::string>> want;
  if (!write) want = ge::test::golden_words(kGolden);
  std::string written = "# FNV-1a 64 of each reference render's float bytes (audio_synth_test)\n";
  size_t row = 0;
  for (const Case& c : kCases) {
    Synth a(7), b(7);
    c.play(a);
    c.play(b);
    const std::vector<float> x = render(a, c.seconds), y = render(b, c.seconds);
    CHECK_MSG(x == y, std::string(c.name) +
                          ": two synths given the same calls rendered "
                          "different bytes; the render reads state it does not own "
                          "(skill: audio, determinism)");
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(fnv(x)));
    written += std::string(c.name) + " " + hex + "\n";
    if (!write) {
      const bool ok = row < want.size() && want[row].size() == 2 && want[row][0] == c.name &&
                      want[row][1] == hex;
      CHECK_MSG(ok, std::string(c.name) + ": the render's hash " + hex + " is not " + kGolden +
                        "'s. If the DSP changed on purpose, rerun with GE_GOLDEN_WRITE=1 and put "
                        "the `synth` lab's --compare in the PR; otherwise a change moved the bits "
                        "(skill: audio).");
    }
    CHECK_MSG(peak_of(x) > 0.001f, std::string(c.name) + " rendered silence");
    row++;
  }
  if (write) std::ofstream(ge::test::root_path(kGolden)) << written;
}

TEST(a_tone_starts_at_its_delay_peaks_near_its_volume_and_ends_at_its_length) {
  Synth s;
  s.tone({.wave = Wave::sine, .from_hz = 1000, .duration_ms = 100, .volume = 0.1f, .delay_ms = 50});
  const std::vector<float> v = render(s, 0.3);
  const size_t start = 2 * static_cast<size_t>(0.05 * kRate);
  const size_t end = 2 * static_cast<size_t>(0.15 * kRate);
  CHECK(peak_of(v, 0, start) == 0);
  CHECK(peak_of(v, end + 2) == 0);
  const float p = peak_of(v);
  CHECK_MSG(p > 0.09f && p <= 0.1001f, "peak " + std::to_string(p) + ", want about 0.1");
  // An exponential tail: a tenth of the way in it is past its peak, by the end under -60 dB.
  CHECK(peak_of(v, end - 40, end) < 0.0002f);
}

TEST(no_onset_is_a_step) {
  Synth s;
  s.noise({.duration_ms = 200, .volume = 0.1f});  // a bare burst: the least attack there is
  const std::vector<float> v = render(s, 0.05);
  // Over the first 1.5 ms the level climbs rather than jumping: the first sample is tiny.
  CHECK(peak_of(v, 0, 2) < 0.005f);
  CHECK(peak_of(v, 0, 144) > 0.01f);
}

TEST(pan_and_balance) {
  Synth s;
  s.tone({.wave = Wave::sine, .from_hz = 500, .duration_ms = 200, .pan = -1});
  const Meter left = measure(render(s, 0.3));
  CHECK(left.balance < -0.99f);
  Synth c;
  c.tone({.wave = Wave::sine, .from_hz = 500, .duration_ms = 200});
  const Meter mid = measure(render(c, 0.3));
  CHECK(mid.balance > -0.01f && mid.balance < 0.01f);
}

TEST(a_bus_at_zero_is_silent_and_a_bus_fades) {
  Synth s;
  s.bus_gain(1, 0, 0);
  s.tone({.from_hz = 300, .duration_ms = 200}, 1);
  s.noise({.duration_ms = 200, .echo = 1}, 1);
  CHECK(peak_of(render(s, 0.5)) < 1e-6f);
  Synth f;
  f.tone({.wave = Wave::sine, .from_hz = 300, .duration_ms = 1000, .hold_ms = 1000}, 2);
  render(f, 0.2);
  f.bus_gain(2, 0, 0.05f);
  const std::vector<float> after = render(f, 0.6);
  CHECK(peak_of(after, after.size() / 2) < 0.0005f);
}

TEST(the_echo_answers_after_its_delay) {
  Synth s(1, EchoRoom{.delay_s = 0.2f, .feedback = 0.3f, .damp_hz = 3000});
  s.noise({.duration_ms = 20, .volume = 0.1f, .echo = 1});
  const std::vector<float> v = render(s, 0.5);
  const size_t gap0 = 2 * static_cast<size_t>(0.05 * kRate);
  const size_t gap1 = 2 * static_cast<size_t>(0.18 * kRate);
  const size_t back0 = 2 * static_cast<size_t>(0.2 * kRate);
  const size_t back1 = 2 * static_cast<size_t>(0.24 * kRate);
  CHECK(peak_of(v, gap0, gap1) < 1e-6f);
  CHECK(peak_of(v, back0, back1) > 0.005f);
}

TEST(a_layer_glides_holds_silence_and_frees_its_slot) {
  Synth s;
  const LayerId id = s.layer({.kind = LayerSpec::Kind::noise, .color = Color::pink});
  CHECK(id.valid());
  CHECK(peak_of(render(s, 0.1)) == 0);  // silent until steered
  s.set(id, {.level = 0.05f}, 0.1f);
  const Meter rising = measure(render(s, 1.0));
  // Over a 0.1 s glide it reaches its level: the loudest window is near 0.05's RMS.
  CHECK_MSG(rising.loudest_db > -30 && rising.loudest_db < -24,
            "loudest " + std::to_string(rising.loudest_db));
  CHECK(s.stats().layers == 1);
  s.stop(id);
  const std::vector<float> fade = render(s, 0.3);
  CHECK(peak_of(fade, fade.size() / 2) < 1e-4f);
  CHECK(s.stats().layers == 0);
  // Every slot can be taken, and the next one is refused.
  Synth full;
  int taken = 0;
  while (full.layer({}).valid()) taken++;
  CHECK(taken == kMaxLayers);
}

TEST(a_rack_builds_once_and_steers_every_layer) {
  Synth s;
  const LayerSpec specs[] = {{.kind = LayerSpec::Kind::noise, .color = Color::brown},
                             {.kind = LayerSpec::Kind::tone, .wave = Wave::sine}};
  const float glides[] = {0.1f, 0.05f};
  Rack rack{specs, glides, 0, {}, false};
  const LayerTarget targets[] = {{.level = 0.03f}, {.level = 0.02f, .hz = 200}};
  for (int frame = 0; frame < 10; frame++) {
    rack_apply(rack, s, targets);
    render(s, 1.0 / 60);
  }
  CHECK(s.stats().layers == 2);
  CHECK(s.stats().dropped == 0);
  rack_stop(rack, s);
  render(s, 0.3);
  CHECK(s.stats().layers == 0);
}

TEST(the_limiter_holds_a_stack_under_full_scale) {
  Synth s;
  for (int i = 0; i < 40; i++) {
    s.tone({.wave = Wave::square,
            .from_hz = 80.0f + 7.0f * static_cast<float>(i),
            .duration_ms = 300,
            .volume = 0.2f});
  }
  const std::vector<float> v = render(s, 0.4);
  CHECK(peak_of(v) <= 1.0f);
  const Meter m = measure(v);
  CHECK_MSG(m.loudest_db < -2, "a stack of 40 loud squares meters " + std::to_string(m.loudest_db) +
                                   " dB; the limiter let it by");
  CHECK(s.stats().stolen == 0);
}

TEST(a_full_pool_steals_and_a_full_ring_drops) {
  Synth s;
  for (int i = 0; i < kMaxVoices + 5; i++) s.noise({.duration_ms = 500, .volume = 0.001f});
  render(s, 0.01);
  CHECK(s.stats().stolen == 5);
  CHECK(s.stats().voices == kMaxVoices);
  Synth r;
  for (int i = 0; i < kRingSize + 3; i++) r.noise({.duration_ms = 1, .volume = 0.001f});
  CHECK(r.stats().dropped == 3);
}

TEST(safe_cutoff_and_the_shaper_rules) {
  CHECK(safe_cutoff(5) == 20);
  CHECK(safe_cutoff(30000) == static_cast<float>(kRate) * kMaxCutoffRatio);
  CHECK(shaper_steepness(0) == 1 && shaper_steepness(1) == 10 && shaper_steepness(2) == 10);
  CHECK(shaper_push(0.5f) == 2.5f);
}

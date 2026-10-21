// The synth's building blocks (engine/audio/dsp.hpp): the sine table against core/libm, the
// curves' shape, the noise pools' colours and levels, the oscillators' range and zero mean, the
// state-variable filter's response, and the glide and ramp coefficients.
//
// ge:summary Tests for the DSP blocks: tables, oscillators, the SVF, noise colours, coefficients
// ge:keywords test dsp sine-table curve noise pool oscillator polyblep svf filter glide ramp
// ge:skill audio testing
#include <cmath>
#include <string>
#include <vector>

#include "audio/dsp.hpp"
#include "check.hpp"
#include "core/libm/libm.hpp"

namespace {

using namespace ge::audio;

const Tables& tables() {
  static const Tables t = make_tables(1);
  return t;
}

float abs_of(float x) { return x < 0 ? -x : x; }

// The steady-state gain of `f` for a sine at `hz`: its output's RMS over the input's, after a
// settling time (a sampled peak misses a high sine's crest).
float gain_at(FilterType type, float cutoff, float hz, float q = 0) {
  Svf f;
  svf_init(f, type, q);
  svf_tune(f, cutoff);
  float phase = 0;
  double out = 0, in = 0;
  const float dt = hz / kRate;
  for (int i = 0; i < kRate / 2; i++) {
    const float x = sine_at(tables(), phase);
    const float y = svf_tick(f, x);
    phase = advance(phase, dt);
    if (i > kRate / 4) {
      out += static_cast<double>(y) * y;
      in += static_cast<double>(x) * x;
    }
  }
  return static_cast<float>(std::sqrt(out / in));
}

}  // namespace

TEST(the_sine_table_is_within_a_micro_of_libm) {
  float worst = 0;
  for (int i = 0; i < 10000; i++) {
    const float p = static_cast<float>(i) / 10000;
    const float want = static_cast<float>(ge::core::libm::sin(2 * kPi * p));
    const float d = abs_of(sine_at(tables(), p) - want);
    worst = d > worst ? d : worst;
  }
  CHECK_MSG(worst < 1e-6f, "worst error " + std::to_string(worst));
}

TEST(a_curve_is_odd_and_normalised) {
  for (const float drive : {0.0f, 0.5f, 1.0f}) {
    const float* c = curve_for(tables(), drive);
    CHECK(abs_of(shape(c, 1) - 1) < 1e-6f);
    CHECK(abs_of(shape(c, -1) + 1) < 1e-6f);
    CHECK(abs_of(shape(c, 0.3f) + shape(c, -0.3f)) < 1e-5f);
    CHECK(shape(c, 5) == shape(c, 1));  // clamped, as a shaper's input is
  }
  // Harder drive, steeper curve.
  CHECK(shape(curve_for(tables(), 1), 0.2f) > shape(curve_for(tables(), 0), 0.2f) + 0.3f);
}

TEST(the_noise_pools_are_full_scale_and_coloured) {
  for (int c = 0; c < 3; c++) {
    double sq = 0, mean = 0;
    for (const float x : tables().noise[c]) {
      sq += static_cast<double>(x) * x;
      mean += x;
    }
    sq /= kPoolFrames;
    mean /= kPoolFrames;
    CHECK_MSG(sq > 0.1 && sq < 1.0, "colour " + std::to_string(c) + " power " + std::to_string(sq));
    CHECK(mean > -0.02 && mean < 0.02);
  }
  // A colour is a tilt: brown's sample-to-sample step is far smaller than white's.
  auto roughness = [](const std::vector<float>& v) {
    double s = 0;
    for (size_t i = 1; i < v.size(); i++) s += abs_of(v[i] - v[i - 1]);
    return s;
  };
  CHECK(roughness(tables().noise[0]) > 1.4 * roughness(tables().noise[1]));
  CHECK(roughness(tables().noise[1]) > 1.6 * roughness(tables().noise[2]));
}

TEST(every_wave_spans_minus_one_to_one_with_no_offset) {
  for (const Wave w : {Wave::sine, Wave::square, Wave::saw, Wave::triangle}) {
    const float dt = 220.0f / kRate;
    float phase = 0, lo = 0, hi = 0;
    double sum = 0;
    const int n = kRate;  // a second: 220 whole cycles
    for (int i = 0; i < n; i++) {
      const float x = oscillate(tables(), w, phase, dt);
      phase = advance(phase, dt);
      lo = x < lo ? x : lo;
      hi = x > hi ? x : hi;
      sum += x;
    }
    CHECK(lo < -0.9f && lo >= -1.2f && hi > 0.9f && hi <= 1.2f);
    CHECK_MSG(sum / n > -0.01 && sum / n < 0.01, "a wave's mean is " + std::to_string(sum / n));
  }
}

TEST(the_svf_passes_and_stops_where_it_should) {
  CHECK(gain_at(FilterType::lowpass, 1000, 100) > 0.95f);
  CHECK(gain_at(FilterType::lowpass, 1000, 8000) < 0.03f);
  CHECK(gain_at(FilterType::highpass, 1000, 8000) > 0.95f);
  CHECK(gain_at(FilterType::highpass, 1000, 100) < 0.03f);
  const float centre = gain_at(FilterType::bandpass, 1000, 1000);
  CHECK_MSG(centre > 0.97f && centre < 1.03f,
            "a band pass at its centre: " + std::to_string(centre));
  CHECK(gain_at(FilterType::bandpass, 1000, 8000) < 0.2f);
  // Resonance: a q of 6 rings over unity at the cutoff.
  CHECK(gain_at(FilterType::lowpass, 1000, 1000, 6) > 4);
}

TEST(glides_ramps_and_cents) {
  // A one-pole reaches 63 % of its target in its time constant.
  float v = 0;
  const float c = glide_coeff(0.1f);
  for (int i = 0; i < kRate / 10; i++) v += (1 - v) * c;
  CHECK(v > 0.62f && v < 0.64f);
  CHECK(glide_coeff(0) == glide_coeff(0.005f));
  // A ramp ratio takes from to to in its frames.
  float x = 100;
  const float r = ramp_ratio(100, 1000, 4800);
  for (int i = 0; i < 4800; i++) x *= r;
  CHECK(x > 999 && x < 1001);
  CHECK(ramp_ratio(1, 2, 0) == 1);
  for (const float cents : {-100.0f, -12.0f, 0.0f, 30.0f, 100.0f}) {
    const float want = static_cast<float>(ge::core::libm::pow(2, cents / 1200.0));
    CHECK(abs_of(cents_ratio(cents) - want) < 1e-6f);
  }
}

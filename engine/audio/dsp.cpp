// The tables' construction and the coefficients that need a transcendental, through core/libm.
//
// ge:summary The synth's tables (sine, curves, noise pools) and the SVF and glide coefficients
// ge:keywords dsp tables make_tables svf_tune glide_coeff ramp_ratio noise-pool libm
// ge:skill audio
// ge:test tests/audio_dsp_test.cpp
#include "audio/dsp.hpp"

#include "core/libm/libm.hpp"
#include "core/prng.hpp"

namespace ge::audio {

namespace libm = core::libm;

Tables make_tables(uint32_t seed) {
  Tables t;
  t.sine.resize(kSineSize + 1);
  for (int i = 0; i <= kSineSize; i++) {
    t.sine[i] = static_cast<float>(libm::sin(2 * kPi * i / kSineSize));
  }
  t.curves.resize(static_cast<size_t>(kCurveCount) * kCurveSize);
  for (int c = 0; c < kCurveCount; c++) {
    const double k = shaper_steepness(static_cast<float>(c) / 20);
    const double norm = libm::tanh(k);
    for (int i = 0; i < kCurveSize; i++) {
      const double x = static_cast<double>(i) / (kCurveSize - 1) * 2 - 1;
      t.curves[static_cast<size_t>(c) * kCurveSize + i] =
          static_cast<float>(libm::tanh(k * x) / norm);
    }
  }
  core::Rng rng = core::make_rng(seed);
  auto white = [&rng] { return static_cast<float>(core::next(rng) * 2 - 1); };
  for (auto& pool : t.noise) pool.resize(kPoolFrames);
  for (int i = 0; i < kPoolFrames; i++) t.noise[0][i] = white();
  float last = 0;
  for (int i = 0; i < kPoolFrames; i++) {
    last = 0.75f * last + 0.25f * white();
    t.noise[1][i] = last * 3.2f;
  }
  const float hp = static_cast<float>(libm::exp(-2 * kPi * 8 / kRate));
  float brown = 0, dc_in = 0, dc_out = 0;
  for (int i = 0; i < kPoolFrames; i++) {
    brown = 0.96f * brown + 0.04f * white();
    const float sample = brown * 11;
    dc_out = hp * (dc_out + sample - dc_in);
    dc_in = sample;
    t.noise[2][i] = dc_out;
  }
  return t;
}

void svf_init(Svf& f, FilterType type, float q) {
  f.type = type;
  f.k = 1 / (q > 0 ? q : default_q(type));
  f.ic1 = f.ic2 = 0;
}

void svf_tune(Svf& f, float hz) {
  const float g = static_cast<float>(libm::tan(kPi * safe_cutoff(hz) / kRate));
  f.a1 = 1 / (1 + g * (g + f.k));
  f.a2 = g * f.a1;
  f.a3 = g * f.a2;
}

float glide_coeff(float tau_s) {
  const double tau = tau_s < 0.005f ? 0.005 : tau_s;
  return static_cast<float>(1 - libm::exp(-1 / (tau * kRate)));
}

float ramp_ratio(float from, float to, int64_t frames) {
  if (frames <= 0 || from <= 0 || to <= 0) return 1;
  return static_cast<float>(
      libm::exp(libm::log(static_cast<double>(to) / from) / static_cast<double>(frames)));
}

}  // namespace ge::audio

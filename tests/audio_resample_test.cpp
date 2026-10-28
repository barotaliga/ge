// Resampling (engine/audio/resample.hpp): the length follows the ratio; a tone well inside both
// bands keeps its frequency and level; going up adds nothing above the source's band; going down
// leaves no alias of a tone above the new band; and the bits are the same every run.
//
// ge:summary Tests the resampler: length, a tone kept, no images going up, no alias going down
// ge:keywords test resample upsample downsample alias image tone
// ge:skill audio testing
#include <cmath>
#include <vector>

#include "audio/fft.hpp"
#include "audio/resample.hpp"
#include "check.hpp"
#include "core/libm/libm.hpp"

namespace au = ge::audio;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> tone(double hz, double rate, size_t n) {
  std::vector<float> x(n);
  for (size_t i = 0; i < n; i++) x[i] = float(0.5 * ge::core::libm::sin(2 * kPi * hz * double(i) / rate));
  return x;
}

// The share of the middle 1024 samples' energy in bins [k0, k1], dB, through a full Hann window.
double band_db(const std::vector<float>& x, size_t k0, size_t k1) {
  const au::Fft fft(1024);
  const std::vector<double> win = au::stft_window(1024, false);
  std::vector<au::Bin> buf(1024);
  const size_t at = x.size() / 2 - 512;
  for (size_t i = 0; i < 1024; i++) buf[i] = au::Bin(double(x[at + i]) * win[i], 0.0);
  fft.transform(buf, false);
  double in = 0, all = 0;
  for (size_t k = 0; k <= 512; k++) {
    all += std::norm(buf[k]);
    if (k >= k0 && k <= k1) in += std::norm(buf[k]);
  }
  return 10 * ge::core::libm::log(in / all + 1e-30) / ge::core::libm::log(10.0);
}

}  // namespace

TEST(resample_length_follows_the_ratio) {
  CHECK(au::resample(tone(440, 11025, 11025), 11025, 48000).size() == 48000);
  CHECK(au::resample(tone(440, 48000, 4800), 48000, 22050).size() == 2205);
}

TEST(resample_up_keeps_a_tone_and_adds_no_images) {
  const std::vector<float> up = au::resample(tone(1000, 11025, 11025), 11025, 48000);
  // 1 kHz is bin 21.3 at 48 kHz and 1024 points: nearly all the energy within a few bins of it.
  CHECK(band_db(up, 18, 25) > -0.1);
  // The images of 1 kHz at 11 025 Hz would sit at 10 025 and 12 025 Hz.
  CHECK_MSG(band_db(up, 120, 512) < -85, std::to_string(band_db(up, 120, 512)));
  double peak = 0;
  for (size_t i = 10000; i < 38000; i++) peak = std::max(peak, double(std::fabs(up[i])));
  CHECK(peak > 0.49 && peak < 0.51);
}

TEST(resample_down_leaves_no_alias) {
  // 15 kHz at 48 kHz is above 22 050 Hz's band (11 025 Hz) and must vanish, not fold to 7 050 Hz.
  std::vector<float> x = tone(15000, 48000, 48000), lo = tone(500, 48000, 48000);
  for (size_t i = 0; i < x.size(); i++) x[i] = 0.5f * (x[i] + lo[i]);
  const std::vector<float> down = au::resample(x, 48000, 22050);
  CHECK(band_db(down, 300, 360) < -70);  // 7 050 Hz at 22 050 Hz is bin 327
  CHECK(down == au::resample(x, 48000, 22050));
}

// The FFT and the short-time transform (engine/audio/fft.hpp): a transform and its inverse give
// the signal back; a sine lands in its own bin; a signal through `stft` and `istft` untouched comes
// back to within rounding; and the bits are the same every run.
//
// ge:summary Tests the FFT and the STFT: round trips, a sine's bin, determinism
// ge:keywords test fft stft istft round-trip sine bin
// ge:skill audio testing
#include <cmath>
#include <vector>

#include "audio/fft.hpp"
#include "check.hpp"
#include "core/libm/libm.hpp"

namespace au = ge::audio;

TEST(fft_round_trip) {
  const au::Fft fft(64);
  std::vector<au::Bin> x(64), y;
  for (size_t i = 0; i < 64; i++) x[i] = au::Bin(ge::core::libm::sin(0.3 * double(i)), 0.1 * double(i % 7));
  y = x;
  fft.transform(y, false);
  fft.transform(y, true);
  double worst = 0;
  for (size_t i = 0; i < 64; i++) worst = std::max(worst, std::abs(y[i] - x[i]));
  CHECK_MSG(worst < 1e-12, "the inverse misses by " + std::to_string(worst));
}

TEST(fft_sine_lands_in_its_bin) {
  const au::Fft fft(256);
  std::vector<au::Bin> x(256);
  for (size_t i = 0; i < 256; i++) x[i] = au::Bin(ge::core::libm::cos(2.0 * 3.14159265358979323846 * 10.0 * double(i) / 256.0), 0);
  fft.transform(x, false);
  CHECK(std::abs(x[10]) > 127.9 && std::abs(x[10]) < 128.1);
  CHECK(std::abs(x[11]) < 1e-9 && std::abs(x[9]) < 1e-9);
}

TEST(stft_round_trip) {
  std::vector<float> x(5000);
  for (size_t i = 0; i < x.size(); i++) x[i] = float(0.5 * ge::core::libm::sin(0.01 * double(i * i % 9973)));
  const std::vector<float> y = au::istft(au::stft(x, 512, 128));
  CHECK(y.size() == x.size());
  double worst = 0;
  for (size_t i = 0; i < x.size(); i++) worst = std::max(worst, double(std::fabs(y[i] - x[i])));
  CHECK_MSG(worst < 1e-5, "the round trip misses by " + std::to_string(worst));
  const std::vector<float> z = au::istft(au::stft(x, 512, 128));
  CHECK(z == y);
}

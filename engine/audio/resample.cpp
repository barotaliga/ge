// The kernel's table and the sum under it.
//
// ge:summary The resampler's Kaiser-windowed sinc table and its convolution
// ge:keywords resample kaiser bessel sinc table convolution
// ge:skill audio
// ge:test tests/audio_resample_test.cpp
#include "audio/resample.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/num.hpp"

namespace ge::audio {

namespace {

constexpr double kPi = 3.14159265358979323846;

// The modified Bessel function of the first kind, order zero, by its series.
double bessel_i0(double x) {
  double sum = 1.0, term = 1.0;
  const double q = x * x / 4.0;
  for (int k = 1; k < 64; k++) {
    term *= q / (double(k) * double(k));
    sum += term;
    if (term < sum * 1e-17) break;
  }
  return sum;
}

}  // namespace

std::vector<float> resample(std::span<const float> in, double from, double to,
                            const ResampleOptions& opts) {
  const size_t out_len = size_t(core::num::round(double(in.size()) * to / from));
  std::vector<float> out(out_len, 0.0f);
  if (in.empty() || out_len == 0) return out;
  const double lower = std::min(from, to);
  const double fc = opts.cutoff * lower / 2.0 / from;  // cycles per input sample
  const double width = double(opts.taps) * from / lower;  // half-width, input samples
  const int phases = opts.phases;
  const size_t count = size_t(std::ceil(width * phases)) + 2;
  std::vector<double> table(count);  // h at x = j / phases, x >= 0
  const double i0b = bessel_i0(opts.beta);
  for (size_t j = 0; j < count; j++) {
    const double x = double(j) / phases;
    if (x >= width) {
      table[j] = 0.0;
      continue;
    }
    const double a = 2.0 * kPi * fc * x;
    const double sinc = x == 0.0 ? 1.0 : core::libm::sin(a) / a;
    const double u = x / width;
    table[j] = 2.0 * fc * sinc * bessel_i0(opts.beta * std::sqrt(1.0 - u * u)) / i0b;
  }
  auto kernel = [&](double x) {
    const double p = std::fabs(x) * phases;
    const size_t j = size_t(p);
    if (j + 1 >= count) return 0.0;
    const double f = p - double(j);
    return table[j] + (table[j + 1] - table[j]) * f;
  };
  const double step = from / to;
  const long last = long(in.size()) - 1;
  for (size_t n = 0; n < out_len; n++) {
    const double t = double(n) * step;
    const long lo = long(std::ceil(t - width)), hi = long(std::floor(t + width));
    double acc = 0.0, wsum = 0.0;
    for (long i = lo; i <= hi; i++) {
      const double h = kernel(t - double(i));
      wsum += h;
      if (i < 0 || i > last) continue;  // zeros outside the signal
      acc += double(in[size_t(i)]) * h;
    }
    out[n] = wsum > 1e-12 ? float(acc / wsum) : 0.0f;
  }
  return out;
}

}  // namespace ge::audio

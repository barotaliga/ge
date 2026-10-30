// The iterative radix-2 transform and the frame loop around it.
//
// ge:summary The FFT's butterflies and the STFT's framing and overlap-add
// ge:keywords fft butterfly twiddle stft istft overlap-add
// ge:skill audio
// ge:test tests/audio_fft_test.cpp
#include "audio/fft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/libm/libm.hpp"

namespace ge::audio {

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

Fft::Fft(size_t size) : n_(size), twiddle_(size / 2), reverse_(size) {
  if (size < 2 || (size & (size - 1)) != 0) std::abort();  // a power of two, from the caller
  for (size_t k = 0; k < size / 2; k++) {
    const double a = -2.0 * kPi * double(k) / double(size);
    twiddle_[k] = Bin(core::libm::cos(a), core::libm::sin(a));
  }
  int bits = 0;
  while ((size_t{1} << bits) < size) bits++;
  for (size_t i = 0; i < size; i++) {
    size_t r = 0;
    for (int b = 0; b < bits; b++) r |= ((i >> b) & 1) << (bits - 1 - b);
    reverse_[i] = r;
  }
}

void Fft::transform(std::span<Bin> data, bool inverse) const {
  for (size_t i = 0; i < n_; i++) {
    if (i < reverse_[i]) std::swap(data[i], data[reverse_[i]]);
  }
  for (size_t len = 2; len <= n_; len *= 2) {
    const size_t half = len / 2, step = n_ / len;
    for (size_t i = 0; i < n_; i += len) {
      for (size_t j = 0; j < half; j++) {
        Bin w = twiddle_[j * step];
        if (inverse) w = std::conj(w);
        const Bin t = w * data[i + j + half];
        data[i + j + half] = data[i + j] - t;
        data[i + j] += t;
      }
    }
  }
  if (inverse) {
    const double s = 1.0 / double(n_);
    for (Bin& v : data) v *= s;
  }
}

std::vector<double> stft_window(size_t size, bool root) {
  std::vector<double> w(size);
  for (size_t i = 0; i < size; i++) {
    const double h = 0.5 - 0.5 * core::libm::cos(2.0 * kPi * double(i) / double(size));
    w[i] = root ? std::sqrt(h) : h;
  }
  return w;
}

Stft stft(std::span<const float> signal, size_t size, size_t hop) {
  Stft s;
  s.size = size;
  s.hop = hop;
  s.length = signal.size();
  const Fft fft(size);
  const std::vector<double> win = stft_window(size, true);
  const size_t count = signal.size() / hop + 1;
  s.frames.resize(count);
  std::vector<Bin> buf(size);
  const long half = long(size / 2);
  for (size_t f = 0; f < count; f++) {
    const long start = long(f * hop) - half;
    for (size_t i = 0; i < size; i++) {
      const long at = start + long(i);
      const double x = at >= 0 && at < long(signal.size()) ? double(signal[size_t(at)]) : 0.0;
      buf[i] = Bin(x * win[i], 0.0);
    }
    fft.transform(buf, false);
    s.frames[f].assign(buf.begin(), buf.begin() + long(size / 2 + 1));
  }
  return s;
}

std::vector<float> istft(const Stft& s) {
  const Fft fft(s.size);
  const std::vector<double> win = stft_window(s.size, true);
  const long half = long(s.size / 2);
  std::vector<double> out(s.length, 0.0), norm(s.length, 0.0);
  std::vector<Bin> buf(s.size);
  for (size_t f = 0; f < s.frames.size(); f++) {
    const std::vector<Bin>& fr = s.frames[f];
    for (size_t k = 0; k <= s.size / 2; k++) buf[k] = fr[k];
    for (size_t k = 1; k < s.size / 2; k++) buf[s.size - k] = std::conj(fr[k]);
    buf[0] = Bin(buf[0].real(), 0.0);
    buf[s.size / 2] = Bin(buf[s.size / 2].real(), 0.0);
    fft.transform(buf, true);
    const long start = long(f * s.hop) - half;
    for (size_t i = 0; i < s.size; i++) {
      const long at = start + long(i);
      if (at < 0 || at >= long(s.length)) continue;
      out[size_t(at)] += buf[i].real() * win[i];
      norm[size_t(at)] += win[i] * win[i];
    }
  }
  std::vector<float> r(s.length);
  for (size_t i = 0; i < s.length; i++) {
    r[i] = norm[i] > 1e-9 ? float(out[i] / norm[i]) : 0.0f;
  }
  return r;
}

}  // namespace ge::audio

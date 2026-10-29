// THE SPECTRUM: a radix-2 complex FFT and the short-time transform built on it, for offline sound
// work (noise reduction, bandwidth extension, spectrograms in a lab). Twiddles come from
// `core::libm`, so a transform is the same bits on every compiler. Not for the audio thread: a
// plan allocates.
//
// `Stft` cuts a signal into frames of `size` samples every `hop` samples under a periodic Hann
// window (sqrt-Hann when `analysis_synthesis` is set, so analysing and resynthesising with the
// same window sums to one at a hop of a quarter of the size), and `istft` overlap-adds frames
// back. A frame holds the non-negative bins, 0 to size / 2.
//
// ge:summary A deterministic radix-2 FFT and the short-time transform (STFT and its inverse)
// ge:keywords fft spectrum stft istft frame hann window bins offline denoise spectrogram
// ge:skill audio
// ge:test tests/audio_fft_test.cpp
#pragma once

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace ge::audio {

using Bin = std::complex<double>;

// One size's twiddles and bit reversal; size a power of two from 2 up.
class Fft {
 public:
  explicit Fft(size_t size);
  size_t size() const { return n_; }
  // In place; `inverse` scales by 1/size.
  void transform(std::span<Bin> data, bool inverse) const;

 private:
  size_t n_;
  std::vector<Bin> twiddle_;
  std::vector<size_t> reverse_;
};

struct Stft {
  size_t size = 1024, hop = 256;
  std::vector<std::vector<Bin>> frames;  // frames[f][k], k in 0..size/2
  size_t length = 0;                     // the signal's length in samples
};

// The window `stft` and `istft` use: periodic Hann, or its square root.
std::vector<double> stft_window(size_t size, bool root);

// The signal's frames. The first frame is centred on sample 0 (the signal is padded with zeros
// half a frame each side), so frame f is centred on f * hop.
Stft stft(std::span<const float> signal, size_t size, size_t hop);
// The signal back from (possibly altered) frames, `length` samples long.
std::vector<float> istft(const Stft& s);

}  // namespace ge::audio

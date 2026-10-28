// The WAV reader and writer, and the enhancer's three steps.
//
// ge:summary The sound enhancer's WAV codec, Wiener noise reduction and band copy-up
// ge:keywords wav riff cue wiener decision-directed noise-floor band-replication soft-knee
// ge:skill classicenhance
// ge:test tests/classicenhance_sound_test.cpp
#include "classicenhance/sound.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "audio/fft.hpp"
#include "audio/resample.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"

namespace ge::classicenhance {

namespace {

uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i)));
}
void put16(std::vector<uint8_t>& b, uint16_t v) {
  b.push_back(uint8_t(v));
  b.push_back(uint8_t(v >> 8));
}
void tag(std::vector<uint8_t>& b, const char* t) { b.insert(b.end(), t, t + 4); }

double db(double ratio) { return 10.0 * core::libm::log(std::max(ratio, 1e-30)) / core::libm::log(10.0); }
double from_db(double d) { return core::libm::pow(10.0, d / 10.0); }

// A look-ahead limiter: the gain each sample needs to stay under `ceiling`, held at its least
// over 12 ms either side, then averaged twice over 3 ms either side. Every sample an average
// reaches lies within the hold of the peak it serves, so the peak still comes under the ceiling,
// and the gain moves smoothly enough to add no clipped harmonics.
void limit_peaks(std::vector<float>& x, double ceiling, int rate) {
  const size_t hold = size_t(std::max(2, rate * 12 / 1000)), half = hold / 4;
  std::vector<double> g(x.size(), 1.0);
  bool any = false;
  for (size_t i = 0; i < x.size(); i++) {
    const double a = std::fabs(double(x[i]));
    if (a > ceiling) {
      g[i] = ceiling / a;
      any = true;
    }
  }
  if (!any) return;
  std::vector<double> held(x.size());
  for (size_t i = 0; i < x.size(); i++) {
    const size_t lo = i > hold ? i - hold : 0, hi = std::min(x.size(), i + hold + 1);
    held[i] = *std::min_element(g.begin() + long(lo), g.begin() + long(hi));
  }
  std::vector<double> sum(x.size() + 1);
  for (int pass = 0; pass < 2; pass++) {
    sum[0] = 0;
    for (size_t i = 0; i < x.size(); i++) sum[i + 1] = sum[i] + held[i];
    for (size_t i = 0; i < x.size(); i++) {
      const size_t lo = i > half ? i - half : 0, hi = std::min(x.size(), i + half + 1);
      held[i] = (sum[hi] - sum[lo]) / double(hi - lo);
    }
  }
  for (size_t i = 0; i < x.size(); i++) x[i] = float(double(x[i]) * held[i]);
}

}  // namespace

bool read_wav(std::span<const uint8_t> b, Pcm* out, std::string* error) {
  const std::string next = "; next: give a PCM WAV (8 or 16 bits) or skip this file (skill: classicenhance)";
  if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0) {
    *error = "not a RIFF WAVE file (" + std::to_string(b.size()) + " bytes)" + next;
    return false;
  }
  int channels = 0, bits = 0, format = 0;
  long rate = 0, loop = -1;
  const uint8_t* data = nullptr;
  size_t data_size = 0;
  size_t at = 12;
  while (at + 8 <= b.size()) {
    const uint8_t* c = b.data() + at;
    const size_t size = le32(c + 4);
    const size_t body = at + 8;
    const size_t have = std::min(size, b.size() - body);  // a short last chunk is read as far as it goes
    if (std::memcmp(c, "fmt ", 4) == 0 && have >= 16) {
      format = le16(c + 8);
      channels = le16(c + 10);
      rate = long(le32(c + 12));
      bits = le16(c + 22);
    } else if (std::memcmp(c, "data", 4) == 0) {
      data = c + 8;
      data_size = have;
    } else if (std::memcmp(c, "cue ", 4) == 0 && have >= 28 && le32(c + 8) > 0) {
      loop = long(le32(c + 8 + 4 + 20));  // the first cue point's sample offset
    }
    at = body + size + (size & 1);
  }
  if (format != 1 || (bits != 8 && bits != 16) || channels < 1 || channels > 2 || rate <= 0) {
    *error = "format " + std::to_string(format) + ", " + std::to_string(bits) + " bits, " +
             std::to_string(channels) + " channels at " + std::to_string(rate) + " Hz" + next;
    return false;
  }
  if (data == nullptr) {
    *error = "no data chunk" + next;
    return false;
  }
  const size_t bytes = size_t(bits / 8), frame = bytes * size_t(channels);
  const size_t count = data_size / frame;
  out->rate = int(rate);
  out->bits = bits;
  out->samples.assign(count, 0.0f);
  for (size_t i = 0; i < count; i++) {
    double sum = 0;
    for (int ch = 0; ch < channels; ch++) {
      const uint8_t* p = data + i * frame + size_t(ch) * bytes;
      sum += bits == 8 ? (double(p[0]) - 128.0) / 128.0 : double(int16_t(le16(p))) / 32768.0;
    }
    out->samples[i] = float(sum / channels);
  }
  out->loop_start = loop >= 0 && size_t(loop) < count ? loop : -1;
  return true;
}

std::vector<uint8_t> write_wav(const Pcm& pcm) {
  std::vector<uint8_t> b;
  const uint32_t data = uint32_t(pcm.samples.size() * 2);
  const bool cue = pcm.loop_start >= 0;
  tag(b, "RIFF");
  put32(b, 4 + 8 + 16 + 8 + data + (cue ? 8 + 28 : 0));
  tag(b, "WAVE");
  tag(b, "fmt ");
  put32(b, 16);
  put16(b, 1);
  put16(b, 1);
  put32(b, uint32_t(pcm.rate));
  put32(b, uint32_t(pcm.rate) * 2);
  put16(b, 2);
  put16(b, 16);
  if (cue) {
    tag(b, "cue ");
    put32(b, 28);
    put32(b, 1);                          // one cue point
    put32(b, 1);                          // its id
    put32(b, uint32_t(pcm.loop_start));   // its position
    tag(b, "data");
    put32(b, 0);
    put32(b, 0);
    put32(b, uint32_t(pcm.loop_start));   // its sample offset: the loop start
  }
  tag(b, "data");
  put32(b, data);
  for (float v : pcm.samples) {
    const double s = core::num::round(double(std::clamp(v, -1.0f, 1.0f)) * 32767.0);
    put16(b, uint16_t(int16_t(s)));
  }
  return b;
}

std::vector<float> reduce_noise(std::span<const float> x, int bits, const SoundOptions& opts,
                                SoundReport* report) {
  if (opts.denoise <= 0 || x.size() < 64) return {x.begin(), x.end()};
  constexpr size_t kSize = 256, kHop = 64;
  audio::Stft s = audio::stft(x, kSize, kHop);
  const size_t bins = kSize / 2 + 1, frames = s.frames.size();
  // The quantisation floor: a uniform step's variance through the window's energy (sqrt-Hann:
  // sum of w^2 = size / 2). 16-bit sources sit far under any hiss, so their floor is a hiss's.
  const double step = bits == 8 ? 2.0 / 256.0 : 2.0 / 65536.0;
  constexpr double kQuantShare = 0.25, kMostOverQuantDb = 24.0;
  const double quant = std::max(step * step / 12.0, 1e-9) * double(kSize) / 2.0;
  // The noise is measured in the sound's quiet frames: those within 6 dB of the quietest frame
  // that is not digital silence, when that frame is at least 30 dB under the loudest. A sound
  // with no such gap (a steady hum, a held voice) is only held to the quantisation floor, so its
  // body is never taken for noise. A hiss is broadband, so each bin is then held to the median
  // of its neighbours, which keeps a quiet tone in the gap out of the noise.
  std::vector<double> fe(frames, 0.0);
  for (size_t f = 0; f < frames; f++) {
    for (size_t k = 0; k < bins; k++) fe[f] += std::norm(s.frames[f][k]);
  }
  double loud = 0, quiet = 1e300;
  for (double e : fe) {
    loud = std::max(loud, e);
    if (e > quant * 0.01 * double(bins)) quiet = std::min(quiet, e);
  }
  std::vector<double> est(bins, 0.0), noise(bins, quant * kQuantShare);
  size_t used = 0;
  if (quiet < loud * from_db(-30.0)) {
    for (size_t f = 0; f < frames; f++) {
      if (fe[f] < quant * 0.01 * double(bins) || fe[f] > quiet * from_db(6.0)) continue;
      for (size_t k = 0; k < bins; k++) est[k] += std::norm(s.frames[f][k]);
      used++;
    }
  }
  if (used > 0) {
    std::vector<double> near;
    for (size_t k = 0; k < bins; k++) est[k] /= double(used);
    for (size_t k = 0; k < bins; k++) {
      near.assign(est.begin() + long(k > 6 ? k - 6 : 0), est.begin() + long(std::min(bins, k + 7)));
      std::nth_element(near.begin(), near.begin() + long(near.size() / 2), near.end());
      noise[k] = std::clamp(std::min(est[k], near[near.size() / 2]), quant * kQuantShare,
                            quant * from_db(kMostOverQuantDb));
    }
  }
  const double over = 1.0 + 0.5 * opts.denoise;
  const double gmin = std::sqrt(from_db(opts.floor_db * std::min(opts.denoise, 2.0)));
  constexpr double kAlpha = 0.98;
  std::vector<double> prev_clean(bins, 0.0);
  double before = 0, after = 0;
  std::vector<double> gain(bins);
  for (size_t f = 0; f < frames; f++) {
    for (size_t k = 0; k < bins; k++) {
      const double p = std::norm(s.frames[f][k]);
      const double n = noise[k] * over;
      const double post = p / n;
      const double prior = kAlpha * prev_clean[k] / n + (1.0 - kAlpha) * std::max(post - 1.0, 0.0);
      gain[k] = std::max(prior / (1.0 + prior), gmin);
    }
    for (size_t k = 0; k < bins; k++) {  // a light smoothing across bins against isolated tones
      const double l = gain[k > 0 ? k - 1 : k], r = gain[k + 1 < bins ? k + 1 : k];
      const double g = 0.25 * l + 0.5 * gain[k] + 0.25 * r;
      const double p = std::norm(s.frames[f][k]);
      before += p;
      s.frames[f][k] *= g;
      prev_clean[k] = p * g * g;
      after += p * g * g;
    }
  }
  if (report != nullptr) {
    double mean = 0;
    for (double v : noise) mean += v;
    report->noise_db = db(mean / double(bins) / (double(kSize) / 2.0));
    report->removed_db = db(after / std::max(before, 1e-30));
  }
  return audio::istft(s);
}

std::vector<float> extend_band(std::span<const float> x, int rate, double source_hz,
                               const SoundOptions& opts, SoundReport* report) {
  if (opts.extend <= 0 || x.size() < 256) return {x.begin(), x.end()};
  constexpr size_t kSize = 1024, kHop = 256;
  audio::Stft s = audio::stft(x, kSize, kHop);
  const double bin_hz = double(rate) / double(kSize);
  const size_t kc = size_t(source_hz / bin_hz);
  const size_t ktop = std::min(size_t(opts.extend_to_hz / bin_hz), kSize / 2);
  if (kc < 16 || ktop <= kc) return {x.begin(), x.end()};
  // The shift: half the source band, rounded to a multiple of size / hop so a copied bin's phase
  // advances a hop exactly as a real one at its new frequency would.
  const size_t lock = kSize / kHop;
  const size_t shift = std::max(lock, (kc / 2) / lock * lock);
  double whole = 0, added = 0;
  for (auto& fr : s.frames) {
    // The frame's slope, dB an octave, from the octave under the top half-octave to it.
    double lo = 0, hi = 0;
    for (size_t k = kc / 4; k < kc / 2; k++) lo += std::norm(fr[k]);
    for (size_t k = kc / 2; k < kc; k++) hi += std::norm(fr[k]);
    lo /= double(kc / 4);
    hi /= double(kc - kc / 2);
    const double slope = std::clamp(db(hi / std::max(lo, 1e-30)), -30.0, -3.0) - opts.rolloff_db;
    // A tonal frame's partials copied up land off its harmonic series and ring like metal, so
    // the copy is scaled by the top band's flatness (geometric over arithmetic mean): noise and
    // transients are copied in full, a held tone at a fifth.
    double logs = 0;
    for (size_t k = kc / 2; k < kc; k++) logs += core::libm::log(std::norm(fr[k]) + 1e-20);
    const double flat = core::libm::exp(logs / double(kc - kc / 2)) / std::max(hi, 1e-20);
    const double tonal = std::clamp(flat / 0.3, 0.2, 1.0);
    for (size_t k = 0; k < kc; k++) whole += std::norm(fr[k]);
    for (size_t k = kc; k <= ktop; k++) {
      size_t src = k;
      while (src >= kc) src -= shift;
      const double octaves = core::libm::log2(double(k) / double(std::max<size_t>(src, 1)));
      const double g = opts.extend * tonal * std::sqrt(from_db(slope * octaves));
      // The edge itself fades in over a few bins so the join has no step.
      const double fade = std::min(1.0, double(k - kc + 1) / 4.0);
      fr[k] = fr[k] + fr[src] * (g * fade);
      added += std::norm(fr[k]);
    }
    for (size_t k = ktop + 1; k <= kSize / 2; k++) fr[k] = 0;
  }
  if (report != nullptr) report->added_db = db(added / std::max(whole, 1e-30));
  return audio::istft(s);
}

Pcm enhance_sound(const Pcm& in, const SoundOptions& opts, SoundReport* report) {
  SoundReport local;
  SoundReport* r = report != nullptr ? report : &local;
  const std::vector<float> clean = reduce_noise(in.samples, in.bits, opts, r);
  audio::ResampleOptions ro;
  std::vector<float> up = audio::resample(clean, in.rate, opts.rate, ro);
  r->source_hz = ro.cutoff * std::min(in.rate, opts.rate) / 2.0;
  std::vector<float> wide =
      in.rate < opts.rate ? extend_band(up, opts.rate, r->source_hz, opts, r) : std::move(up);
  limit_peaks(wide, opts.knee, opts.rate);
  Pcm out;
  out.rate = opts.rate;
  out.bits = 16;
  out.samples = std::move(wide);
  out.loop_start = in.loop_start < 0
                       ? -1
                       : long(core::num::round(double(in.loop_start) * opts.rate / in.rate));
  return out;
}

}  // namespace ge::classicenhance

// The room's reverb: the glides, the early line's taps and the feedback delay network.
//
// ge:summary The room's reverb: glides, early taps, the Householder FDN and its damped loops
// ge:keywords audio reverb fdn householder early-reflections loop-gain damping glide
// ge:skill audio
// ge:test tests/audio_reverb_test.cpp
#include "audio/reverb.hpp"

#include <algorithm>
#include <cmath>

#include "audio/dsp.hpp"
#include "core/libm/libm.hpp"

namespace ge::audio {

namespace libm = ge::core::libm;

namespace {

// The lines' lengths in frames, prime (no two share a factor, so their echoes never line up
// into a pitch), 31 to 73 ms at 48 kHz.
constexpr std::array<int, kLines> kLengths = {1489, 1789, 2017, 2269, 2557, 2879, 3209, 3511};
// The input diffuser: four allpasses in series, prime lengths 3 to 11 ms, each at kDiffuse.
constexpr std::array<int, 4> kDiffuseLengths = {151, 233, 359, 541};
constexpr float kDiffuse = 0.62f;
constexpr float kLn10 = 2.30258509f;
constexpr float kTailScale = 0.5f;  // 1 / sqrt(lines a side)
constexpr float kSilent = 1e-7f;    // a peak under this is silence

int early_frames() {
  const float ms = kMaxEarlyMs > kMaxPredelayMs ? kMaxEarlyMs : kMaxPredelayMs;
  return static_cast<int>(ms * 0.001f * kRate) + 4;
}

float lowpass_coeff(float hz) {
  return static_cast<float>(1 - libm::exp(-2 * kPi * static_cast<double>(hz) / kRate));
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// The line read `delay` frames behind `write`, linearly between frames.
float read_at(const std::vector<float>& buf, int write, float delay) {
  const int size = static_cast<int>(buf.size());
  const int whole = static_cast<int>(delay);
  const float frac = delay - static_cast<float>(whole);
  int a = write - whole;
  if (a < 0) a += size;
  int b = a - 1;
  if (b < 0) b += size;
  return buf[static_cast<size_t>(a)] +
         (buf[static_cast<size_t>(b)] - buf[static_cast<size_t>(a)]) * frac;
}

bool dry(const RoomSound& r) { return r.wet <= 0 && r.taps <= 0 && r.echo == 1; }

}  // namespace

Reverb::Reverb() {
  early_.assign(static_cast<size_t>(early_frames()), 0);
  for (size_t k = 0; k < kLines; ++k) line_[k].assign(static_cast<size_t>(kLengths[k]), 0);
  for (size_t k = 0; k < diffuse_.size(); ++k) {
    diffuse_[k].assign(static_cast<size_t>(kDiffuseLengths[k]), 0);
  }
  tap_damp_.fill(8000);
  control();
}

void Reverb::set(const RoomSound& room, float glide_s) {
  target_ = room;
  target_.taps = room.taps < 0 ? 0 : room.taps > kEarlyTaps ? kEarlyTaps : room.taps;
  if (glide_s > 0) {
    const double per_sample = glide_coeff(glide_s);
    glide_ = static_cast<float>(1 - libm::pow(1 - per_sample, kControl));
  } else {
    glide_ = 1;
  }
  if (!dry(target_)) active_ = true;
  quiet_ = 0;
  if (glide_s <= 0) control();
}

void Reverb::control() {
  const RoomSound& t = target_;
  const float g = glide_;
  wet_ += (clampf(t.wet, 0, 4) - wet_) * g;
  decay_ += (clampf(t.decay_s, 0.05f, 20) - decay_) * g;
  damp_ += (clampf(t.damp_hz, 200, 20000) - damp_) * g;
  predelay_ += (clampf(t.predelay_ms, 0, kMaxPredelayMs) - predelay_) * g;
  echo_ += (clampf(t.echo, 0, 1) - echo_) * g;
  if (g == 1) echo_ = clampf(t.echo, 0, 1);
  for (int k = 0; k < kEarlyTaps; ++k) {
    const bool on = k < t.taps;
    const EarlyTap& e = t.tap[static_cast<size_t>(k)];
    const size_t i = static_cast<size_t>(k);
    const float d = clampf(e.delay_ms, 0, kMaxEarlyMs);
    // A tap moving to another wall fades out first, then takes its new place (below).
    const bool jump =
        on && g < 1 && tap_gain_[i] >= 1e-4f && std::fabs(d - tap_delay_[i]) > kTapJumpMs;
    tap_gain_[i] += ((on && !jump ? clampf(e.gain, 0, 1) : 0) - tap_gain_[i]) * g;
    if (on) {
      // A tap that was silent takes its new place at once, so it never sweeps into it.
      const bool fresh = tap_gain_[i] < 1e-4f;
      tap_delay_[i] = fresh ? d : tap_delay_[i] + (d - tap_delay_[i]) * g;
      tap_pan_[i] =
          fresh ? clampf(e.pan, -1, 1) : tap_pan_[i] + (clampf(e.pan, -1, 1) - tap_pan_[i]) * g;
      tap_damp_[i] += (clampf(e.damp_hz, 200, 20000) - tap_damp_[i]) * g;
    }
    tap_coeff_[i] = lowpass_coeff(tap_damp_[i]);
  }
  for (size_t i = 0; i < kLines; ++i) {
    loop_gain_[i] = static_cast<float>(libm::exp(-3.0 * kLn10 * static_cast<double>(kLengths[i]) /
                                                 (kRate * static_cast<double>(decay_))));
  }
  loop_coeff_ = damp_ >= 20000 ? 1 : lowpass_coeff(damp_);  // the most: the loops undamped
  // The tail's level held to `wet`: a pass of the mean line keeps 10^(-6 L / (rate decay)) of
  // the network's energy, so what a side hears of a unit pulse grows as rate decay / L; its
  // constant measured (tests/audio_reverb_test.cpp, within a dB or two over decays from 0.5 to
  // 2 s), `wet` 1 is a tail as loud as the send, whatever the room.
  float mean = 0;
  for (const int len : kLengths) mean += static_cast<float>(len);
  mean /= kLines;
  norm_ = std::sqrt(mean / (0.0472f * kRate * decay_));
}

void Reverb::process(const float* in, float* l, float* r, int n) {
  if (!active_) return;
  const int esize = static_cast<int>(early_.size());
  constexpr float ms = 0.001f * kRate;
  for (int f = 0; f < n; ++f) {
    if (ctl_ == 0) control();
    if (++ctl_ == kControl) ctl_ = 0;
    early_[static_cast<size_t>(early_write_)] = in[f];
    float el = 0, er = 0;
    for (size_t k = 0; k < kEarlyTaps; ++k) {
      if (tap_gain_[k] < 1e-6f) continue;
      const float s = read_at(early_, early_write_, tap_delay_[k] * ms);
      tap_lp_[k] += (s - tap_lp_[k]) * tap_coeff_[k];
      const float v = tap_lp_[k] * tap_gain_[k];
      const float p = tap_pan_[k];
      el += v * (p > 0 ? 1 - p : 1);
      er += v * (p < 0 ? 1 + p : 1);
    }
    float u = read_at(early_, early_write_, predelay_ * ms);
    if (++early_write_ == esize) early_write_ = 0;
    for (size_t k = 0; k < diffuse_.size(); ++k) {
      std::vector<float>& d = diffuse_[k];
      int& at = diffuse_at_[k];
      const float held = d[static_cast<size_t>(at)];
      const float v = u + kDiffuse * held;
      d[static_cast<size_t>(at)] = v;
      if (++at == static_cast<int>(d.size())) at = 0;
      u = held - kDiffuse * v;
    }
    std::array<float, kLines> o{};
    float sum = 0;
    for (size_t k = 0; k < kLines; ++k) {
      const float s = line_[k][static_cast<size_t>(line_at_[k])];
      lp_[k] += (s - lp_[k]) * loop_coeff_;
      o[k] = lp_[k] * loop_gain_[k];
      sum += o[k];
    }
    const float h = sum * (2.0f / kLines);
    float tl = 0, tr = 0;
    for (size_t k = 0; k < kLines; ++k) {
      const float sign = (k & 2) ? -1.0f : 1.0f;
      line_[k][static_cast<size_t>(line_at_[k])] = o[k] - h + u * sign;
      if (++line_at_[k] == kLengths[k]) line_at_[k] = 0;
      if (k & 1) {
        tr += o[k] * sign;
      } else {
        tl += o[k] * sign;
      }
    }
    const float wl = el + tl * kTailScale * wet_ * norm_;
    const float wr = er + tr * kTailScale * wet_ * norm_;
    l[f] += wl;
    r[f] += wr;
    const float a = wl < 0 ? -wl : wl, b = wr < 0 ? -wr : wr;
    if (a > peak_) peak_ = a;
    if (b > peak_) peak_ = b;
    if (ctl_ == 0) {
      // A dry room whose tail has died stops running, its lines cleared for the next.
      if (dry(target_) && peak_ < kSilent && wet_ < 1e-5f && echo_ > 0.99999f) {
        if (++quiet_ > kRate / kControl / 2) {
          active_ = false;
          echo_ = 1;
          for (auto& line : line_) std::fill(line.begin(), line.end(), 0.0f);
          std::fill(early_.begin(), early_.end(), 0.0f);
          for (auto& d : diffuse_) std::fill(d.begin(), d.end(), 0.0f);
          lp_.fill(0);
          tap_lp_.fill(0);
          tap_gain_.fill(0);
          return;
        }
      } else {
        quiet_ = 0;
      }
      peak_ = 0;
    }
  }
}

}  // namespace ge::audio

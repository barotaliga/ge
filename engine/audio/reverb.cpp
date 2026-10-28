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

// The lines' lengths over the mean free path's travel time: mutually prime in effect (no two
// share a small ratio), so their echoes never line up into a pitch.
constexpr std::array<float, kLines> kRatios = {0.613f, 0.701f, 0.787f, 0.871f,
                                               0.953f, 1.049f, 1.153f, 1.297f};
constexpr float kLn10 = 2.30258509f;
constexpr float kTailScale = 0.5f;  // 1 / sqrt(lines a side)
constexpr float kSilent = 1e-7f;    // a peak under this is silence

int line_frames() {
  return static_cast<int>(kMaxPathM / kSoundSpeed * kRate * kRatios[kLines - 1]) + 4;
}
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
  for (auto& l : line_) l.assign(static_cast<size_t>(line_frames()), 0);
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
  size_ += (clampf(t.size_m, 1, kMaxPathM) - size_) * g;
  damp_ += (clampf(t.damp_hz, 200, 20000) - damp_) * g;
  predelay_ += (clampf(t.predelay_ms, 0, kMaxPredelayMs) - predelay_) * g;
  echo_ += (clampf(t.echo, 0, 1) - echo_) * g;
  if (g == 1) echo_ = clampf(t.echo, 0, 1);
  for (int k = 0; k < kEarlyTaps; ++k) {
    const bool on = k < t.taps;
    const EarlyTap& e = t.tap[static_cast<size_t>(k)];
    const size_t i = static_cast<size_t>(k);
    tap_gain_[i] += ((on ? clampf(e.gain, 0, 1) : 0) - tap_gain_[i]) * g;
    if (on) {
      // A tap that was silent takes its new place at once, so it never sweeps into it.
      const bool fresh = tap_gain_[i] < 1e-4f;
      const float d = clampf(e.delay_ms, 0, kMaxEarlyMs);
      tap_delay_[i] = fresh ? d : tap_delay_[i] + (d - tap_delay_[i]) * g;
      tap_pan_[i] =
          fresh ? clampf(e.pan, -1, 1) : tap_pan_[i] + (clampf(e.pan, -1, 1) - tap_pan_[i]) * g;
      tap_damp_[i] += (clampf(e.damp_hz, 200, 20000) - tap_damp_[i]) * g;
    }
    tap_coeff_[i] = lowpass_coeff(tap_damp_[i]);
  }
  const float travel = size_ / kSoundSpeed * kRate;
  for (int k = 0; k < kLines; ++k) {
    const size_t i = static_cast<size_t>(k);
    length_[i] = travel * kRatios[i];
    loop_gain_[i] = static_cast<float>(libm::exp(-3.0 * kLn10 * static_cast<double>(length_[i]) /
                                                 (kRate * static_cast<double>(decay_))));
  }
  loop_coeff_ = lowpass_coeff(damp_);
  // The tail's level held to `wet`: a pass of the mean line keeps 10^(-6 L / (rate decay)) of
  // the network's energy, so what a side hears of a unit pulse grows as rate decay / L; its
  // constant measured (tests/audio_reverb_test.cpp, within a dB or two over rooms from 4 to 20 m
  // and decays from 0.5 to 2 s), `wet` 1 is a tail as loud as the send, whatever the room.
  float mean = 0;
  for (const float len : length_) mean += len;
  mean /= kLines;
  norm_ = std::sqrt(mean / (0.0232f * kRate * decay_));
}

void Reverb::process(const float* in, float* l, float* r, int n) {
  if (!active_) return;
  const int esize = static_cast<int>(early_.size());
  const int lsize = static_cast<int>(line_[0].size());
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
    const float u = read_at(early_, early_write_, predelay_ * ms);
    if (++early_write_ == esize) early_write_ = 0;
    std::array<float, kLines> o{};
    float sum = 0;
    for (size_t k = 0; k < kLines; ++k) {
      const float s = read_at(line_[k], line_write_, length_[k]);
      lp_[k] += (s - lp_[k]) * loop_coeff_;
      o[k] = lp_[k] * loop_gain_[k];
      sum += o[k];
    }
    const float h = sum * (2.0f / kLines);
    float tl = 0, tr = 0;
    for (size_t k = 0; k < kLines; ++k) {
      const float sign = (k & 2) ? -1.0f : 1.0f;
      line_[k][static_cast<size_t>(line_write_)] = o[k] - h + u * sign;
      if (k & 1) {
        tr += o[k] * sign;
      } else {
        tl += o[k] * sign;
      }
    }
    if (++line_write_ == lsize) line_write_ = 0;
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

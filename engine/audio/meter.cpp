// The meter's passes over a buffer: the peak and the RMS, the windows, the bands.
//
// ge:summary The meter's passes: peak, RMS, 10 ms windows, the octave bands through the SVF
// ge:keywords meter measure to_db bands windows
// ge:skill audio labs
// ge:test tests/audio_meter_test.cpp
#include "audio/meter.hpp"

#include <cmath>

#include "audio/dsp.hpp"
#include "core/libm/libm.hpp"

namespace ge::audio {

float to_db(float level) {
  if (level <= 0) return kSilenceDb;
  const float db =
      static_cast<float>(8.68588963806503655 * core::libm::log(static_cast<double>(level)));
  return db < kSilenceDb ? kSilenceDb : db;
}

namespace {

float rms_db(double sum_sq, size_t n) {
  if (n == 0) return kSilenceDb;
  return to_db(static_cast<float>(std::sqrt(sum_sq / static_cast<double>(n))));
}

}  // namespace

Meter measure(std::span<const float> stereo) {
  Meter m;
  const size_t frames = stereo.size() / 2;
  if (frames == 0) return m;
  double all = 0, left = 0, right = 0;
  float peak = 0;
  const size_t window = static_cast<size_t>(kWindowS * kRate);
  double win = 0;
  size_t in_win = 0;
  std::array<Svf, kBands> bands;
  for (int b = 0; b < kBands; b++) {
    svf_init(bands[b], FilterType::bandpass, 1.41421356f);
    svf_tune(bands[b], kBandHz[b]);
  }
  std::array<double, kBands> band_sq{};
  for (size_t i = 0; i < frames; i++) {
    const float l = stereo[2 * i], r = stereo[2 * i + 1];
    const float al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
    if (al > peak) peak = al;
    if (ar > peak) peak = ar;
    left += static_cast<double>(l) * l;
    right += static_cast<double>(r) * r;
    win += static_cast<double>(l) * l + static_cast<double>(r) * r;
    if (++in_win == window) {
      m.envelope_db.push_back(rms_db(win, 2 * window));
      win = 0;
      in_win = 0;
    }
    const float mono = (l + r) * 0.5f;
    for (int b = 0; b < kBands; b++) {
      const float y = svf_tick(bands[b], mono);
      band_sq[b] += static_cast<double>(y) * y;
    }
  }
  if (in_win > 0) m.envelope_db.push_back(rms_db(win, 2 * in_win));
  all = left + right;
  m.peak_db = to_db(peak);
  m.rms_db = rms_db(all, 2 * frames);
  m.balance = all > 0 ? static_cast<float>((right - left) / all) : 0;
  double weight = 0, centre = 0;
  for (int b = 0; b < kBands; b++) {
    m.band_db[b] = rms_db(band_sq[b], frames);
    weight += band_sq[b];
    centre += band_sq[b] * kBandHz[b];
  }
  m.centroid_hz = weight > 0 ? static_cast<float>(centre / weight) : 0;
  for (float w : m.envelope_db) {
    if (w > m.loudest_db) m.loudest_db = w;
  }
  const float floor = m.loudest_db - 40;
  size_t first = m.envelope_db.size(), last = 0;
  for (size_t i = 0; i < m.envelope_db.size(); i++) {
    if (m.envelope_db[i] > floor && m.envelope_db[i] > kSilenceDb) {
      if (first == m.envelope_db.size()) first = i;
      last = i;
    }
  }
  if (first < m.envelope_db.size()) {
    m.onset_s = static_cast<float>(first) * kWindowS;
    m.length_s = static_cast<float>(last + 1 - first) * kWindowS;
  }
  return m;
}

}  // namespace ge::audio

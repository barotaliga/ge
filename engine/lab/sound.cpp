// The sound's picture and its metrics.
//
// ge:summary The sound's picture (waveform, envelope, bands) and the metrics sound_shot writes
// ge:keywords lab sound sound_picture waveform bands
// ge:skill labs audio
// ge:test tests/audio_meter_test.cpp
#include "lab/sound.hpp"

#include <cstdio>

#include "audio/voice.hpp"
#include "audio/wav.hpp"
#include "core/num.hpp"

namespace ge::lab {

namespace {

constexpr int kWave = 600;  // the waveform's width, px
constexpr int kBandsW = 200;
constexpr int kHeight = 220;
constexpr int kPad = 10;
constexpr float kFloorDb = -80;

const Rgba kBack{18, 22, 30};
const Rgba kAxis{60, 68, 84};
const Rgba kLeft{120, 190, 255};
const Rgba kRight{255, 170, 110};
const Rgba kLevel{240, 240, 120};
const Rgba kBar{150, 220, 160};
const Rgba kText{210, 214, 222};

double y_of_db(float db, double top, double h) {
  const float d = db < kFloorDb ? kFloorDb : db > 0 ? 0 : db;
  return top + h * (d / kFloorDb);
}

std::string fmt(double v, const char* f) {
  char b[32];
  std::snprintf(b, sizeof b, f, v);
  return b;
}

}  // namespace

Picture sound_picture(const audio::Meter& m, std::span<const float> stereo) {
  Picture p = create_picture(kWave + kBandsW + 3 * kPad, kHeight + 2 * kPad, kBack);
  const double top = kPad, h = kHeight, mid = top + h / 2;
  fill_rect(p, kPad, top, kWave, h, Rgba{24, 30, 40});
  draw_line(p, kPad, mid, kPad + kWave, mid, kAxis);
  const size_t frames = stereo.size() / 2;
  // The waveform is drawn scaled up so its peak fills nine tenths of its half (the level is the
  // meter's, written over it); a buffer under -60 dBFS is drawn as it is.
  float most = 0;
  for (const float x : stereo) most = x > most ? x : -x > most ? -x : most;
  const float zoom = most > 0.001f ? 0.9f / most : 1;
  for (int c = 0; c < kWave && frames > 0; c++) {
    const size_t a = frames * static_cast<size_t>(c) / kWave;
    size_t b = frames * static_cast<size_t>(c + 1) / kWave;
    if (b <= a) b = a + 1;
    float lmax = 0, rmax = 0;
    for (size_t i = a; i < b && i < frames; i++) {
      const float l = stereo[2 * i] < 0 ? -stereo[2 * i] : stereo[2 * i];
      const float r = stereo[2 * i + 1] < 0 ? -stereo[2 * i + 1] : stereo[2 * i + 1];
      lmax = l > lmax ? l : lmax;
      rmax = r > rmax ? r : rmax;
    }
    const double x = kPad + c;
    draw_line(p, x, mid, x, mid - lmax * zoom * (h / 2), kLeft);
    draw_line(p, x, mid, x, mid + rmax * zoom * (h / 2), kRight);
  }
  // The level envelope, every window across the same width.
  const size_t n = m.envelope_db.size();
  for (size_t i = 1; i < n; i++) {
    const double x0 = kPad + kWave * static_cast<double>(i - 1) / static_cast<double>(n);
    const double x1 = kPad + kWave * static_cast<double>(i) / static_cast<double>(n);
    draw_line(p, x0, y_of_db(m.envelope_db[i - 1], top, h), x1, y_of_db(m.envelope_db[i], top, h),
              kLevel, 2);
  }
  // The bands.
  const double bx = 2 * kPad + kWave;
  fill_rect(p, bx, top, kBandsW, h, Rgba{24, 30, 40});
  for (int db = -20; db > static_cast<int>(kFloorDb); db -= 20) {
    const double y = y_of_db(static_cast<float>(db), top, h);
    draw_line(p, bx, y, bx + kBandsW, y, kAxis);
  }
  const double w = static_cast<double>(kBandsW) / audio::kBands;
  for (int b = 0; b < audio::kBands; b++) {
    const double y = y_of_db(m.band_db[b], top, h - 12);
    fill_rect(p, bx + b * w + 2, y, w - 4, top + h - 12 - y, kBar);
  }
  const char* labels[audio::kBands] = {"31", "", "125", "", "500", "", "2K", "", "8K", ""};
  for (int b = 0; b < audio::kBands; b++)
    draw_text(p, bx + b * w + 2, top + h - 9, labels[b], kText);
  draw_text(p, kPad + 4, top + 4,
            "LOUDEST " + fmt(m.loudest_db, "%.1f") + " DB  PEAK " + fmt(m.peak_db, "%.1f") +
                " DB  CENTROID " + fmt(m.centroid_hz, "%.0f") + " HZ  LENGTH " +
                fmt(m.length_s, "%.2f") + " S",
            kText);
  draw_text(p, kPad + 4, top + h - 10, "WAVE x" + fmt(zoom, "%.1f") + "  LEVEL -80..0 DB", kText);
  return p;
}

audio::Meter sound_shot(Lab& lab, Shot shot, std::span<const float> stereo) {
  const audio::Meter m = audio::measure(stereo);
  const std::string& id = shot.id;
  // Rounded to a hundredth of a dB (a cent of a hertz, a millisecond), so the printed metric
  // reads the same as the one hashed.
  auto r = [](double v, double step) { return core::num::round(v / step) * step; };
  lab.metric(id + ".loudest-db", r(m.loudest_db, 0.01), "dB");
  lab.metric(id + ".peak-db", r(m.peak_db, 0.01), "dB");
  lab.metric(id + ".centroid-hz", r(m.centroid_hz, 1), "Hz");
  lab.metric(id + ".onset-s", r(m.onset_s, 0.001), "s");
  lab.metric(id + ".length-s", r(m.length_s, 0.001), "s");
  lab.attach(id + ".wav", audio::wav_bytes(stereo));
  shot.inputs.push_back({"rate", std::to_string(audio::kRate) + " Hz"});
  lab.shot(std::move(shot), sound_picture(m, stereo));
  return m;
}

}  // namespace ge::lab

// LAB `enhance-sound`: what the sound enhancer makes of a classic sound. Three sounds are made
// here at 48 kHz with their full band (a decaying noise burst, a voice-like buzz gliding in pitch
// with a quiet hiss after it, a held hum), then brought down to what the classic pack holds
// (11 025 Hz, 8 bits) and enhanced. Each is shown three times as a metered sound shot (its
// waveform, level envelope and octave bands, its WAV beside it): the full-band original, the
// classic copy played back at 48 kHz unchanged (resampled, nothing added) and the enhanced one.
//
// The numbers, per sound: the share of energy above 6 kHz in each version (the classic copy has
// none; the enhanced one should come near the original's), the log-spectral distance from the
// original over 6–16 kHz, and the level of the hiss gap. Everything is made here, so the pictures,
// the WAVs and the digest are the same everywhere and may be published; the player's own pack's
// sounds are only ever heard from their cache.
//
// ge:summary Lab: synthetic classic sounds before and after the enhancer, metered, with WAVs
// ge:keywords lab enhance sound wav denoise bandwidth-extension upsample spectral-distance hiss
// ge:skill classicenhance labs
// ge:test tests/classicenhance_sound_test.cpp
#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

#include "audio/fft.hpp"
#include "audio/resample.hpp"
#include "classicenhance/sound.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"

namespace ce = ge::classicenhance;
namespace libm = ge::core::libm;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kRate = 48000;

struct Made {
  const char* id;
  const char* title;
  const char* what;
  std::vector<float> full;  // 48 kHz, full band
  size_t gap_from = 0, gap_to = 0;  // a stretch of hiss alone (0, 0: none)
};

std::vector<Made> made_sounds() {
  std::vector<Made> out;
  ge::core::Rng rng{41};
  {  // A burst: white noise through a falling one-pole low-pass, decaying.
    Made m{"burst", "A NOISE BURST", "white noise decaying over half a second, its brightness falling", {}};
    m.full.resize(size_t(kRate) * 6 / 10);
    double lp = 0;
    for (size_t i = 0; i < m.full.size(); i++) {
      const double t = double(i) / kRate;
      const double a = 0.15 + 0.8 * libm::exp(-t * 6);
      lp += a * ((ge::core::next(rng) * 2 - 1) - lp);
      m.full[i] = float(0.6 * libm::exp(-t * 5) * lp);
    }
    out.push_back(std::move(m));
  }
  {  // A voice-like buzz: a sawtooth's harmonics gliding 140 -> 220 Hz, then hiss alone.
    Made m{"voice", "A GLIDING BUZZ, THEN HISS",
           "harmonics to 16 kHz on a pitch rising 140 to 220 Hz with vibrato, then a gap of faint hiss",
           {}};
    m.full.resize(size_t(kRate) * 12 / 10);
    double phase = 0;
    const size_t body = size_t(kRate) * 7 / 10;
    for (size_t i = 0; i < m.full.size(); i++) {
      const double t = double(i) / kRate;
      const double f0 = 140 + 80 * t / 0.7 + 4 * libm::sin(2 * kPi * 5.5 * t);
      phase += f0 / kRate;
      double v = 0;
      if (i < body) {
        for (int h = 1; double(h) * f0 < 16000; h++) {
          v += libm::sin(2 * kPi * h * phase) / h;
        }
        v *= 0.25 * libm::sin(kPi * double(i) / double(body));
      }
      m.full[i] = float(v + 0.006 * (ge::core::next(rng) * 2 - 1));
    }
    m.gap_from = body + size_t(kRate) / 20;
    m.gap_to = m.full.size();
    out.push_back(std::move(m));
  }
  {  // A hum: a held low tone with two partials; nothing above should be added.
    Made m{"hum", "A HELD HUM", "a 110 Hz tone and its second and third partials, held", {}};
    m.full.resize(size_t(kRate) * 8 / 10);
    for (size_t i = 0; i < m.full.size(); i++) {
      const double t = double(i) / kRate;
      m.full[i] = float(0.3 * libm::sin(2 * kPi * 110 * t) + 0.1 * libm::sin(2 * kPi * 220 * t) +
                        0.05 * libm::sin(2 * kPi * 330 * t));
    }
    out.push_back(std::move(m));
  }
  return out;
}

// What the pack holds: 11 025 Hz, 8 bits.
ce::Pcm classic(const std::vector<float>& full) {
  ce::Pcm p;
  p.rate = 11025;
  p.bits = 8;
  p.samples = ge::audio::resample(full, kRate, 11025);
  for (float& v : p.samples) {
    v = float(std::clamp(ge::core::num::round(double(v) * 128.0), -128.0, 127.0) / 128.0);
  }
  return p;
}

// Mean power spectra over 1024-sample Hann frames, hop 512.
std::vector<double> spectrum(const std::vector<float>& x) {
  const ge::audio::Fft fft(1024);
  const std::vector<double> win = ge::audio::stft_window(1024, false);
  std::vector<ge::audio::Bin> buf(1024);
  std::vector<double> p(513, 0.0);
  for (size_t at = 0; at + 1024 <= x.size(); at += 512) {
    for (size_t i = 0; i < 1024; i++) buf[i] = ge::audio::Bin(double(x[at + i]) * win[i], 0.0);
    fft.transform(buf, false);
    for (size_t k = 0; k <= 512; k++) p[k] += std::norm(buf[k]);
  }
  return p;
}

double db(double v) { return 10 * libm::log(v + 1e-30) / libm::log(10.0); }

size_t bin_of(double hz) { return static_cast<size_t>(hz / (double(kRate) / 1024.0)); }

double share_above_db(const std::vector<double>& p, double hz) {
  double hi = 0, all = 0;
  for (size_t k = 0; k < p.size(); k++) {
    all += p[k];
    if (k >= bin_of(hz)) hi += p[k];
  }
  return db(hi / (all + 1e-30));
}

// The log-spectral distance over [lo, hi] Hz, dB, of the mean spectra.
double lsd_db(const std::vector<double>& a, const std::vector<double>& b, double lo, double hi) {
  double s = 0;
  int n = 0;
  for (size_t k = bin_of(lo); k <= bin_of(hi); k++, n++) {
    const double d = db(a[k]) - db(b[k]);
    s += d * d;
  }
  return n > 0 ? std::sqrt(s / n) : 0.0;
}

double level_db(const std::vector<float>& x, size_t from, size_t to) {
  double s = 0;
  for (size_t i = from; i < to && i < x.size(); i++) s += double(x[i]) * x[i];
  return db(s / double(std::max<size_t>(1, to - from)));
}

std::vector<float> stereo(const std::vector<float>& mono) {
  std::vector<float> out(mono.size() * 2);
  for (size_t i = 0; i < mono.size(); i++) out[i * 2] = out[i * 2 + 1] = mono[i];
  return out;
}

std::string f1(double v) { return ge::core::num::to_fixed(v, 1); }

void run(ge::lab::Lab& lab) {
  ge::lab::LabTable table{"each sound: energy above 6 kHz, distance from the original, hiss",
                          {"sound", "above 6k: original", "classic", "enhanced",
                           "6-16k distance: classic", "enhanced", "hiss gap: classic", "enhanced"},
                          {}};
  for (const Made& m : made_sounds()) {
    const ce::Pcm cl = classic(m.full);
    ce::SoundOptions plain;
    plain.denoise = 0;
    plain.extend = 0;
    const ce::Pcm before = ce::enhance_sound(cl, plain);
    ce::SoundReport report;
    const ce::Pcm after = ce::enhance_sound(cl, {}, &report);
    if (after.rate != kRate) {
      lab.fail(std::string(m.id) + ": the enhancer returned " + std::to_string(after.rate) + " Hz",
               "next: check SoundOptions::rate reaches audio::resample in sound.cpp");
      continue;
    }
    const std::vector<double> po = spectrum(m.full), pb = spectrum(before.samples),
                              pa = spectrum(after.samples);
    const std::string id = m.id;
    const double ao = share_above_db(po, 6000), ab = share_above_db(pb, 6000),
                 aa = share_above_db(pa, 6000);
    const double lb = lsd_db(po, pb, 6000, 16000), la = lsd_db(po, pa, 6000, 16000);
    lab.metric(id + ".above-6k-original", ao, "dB");
    lab.metric(id + ".above-6k-classic", ab, "dB");
    lab.metric(id + ".above-6k-enhanced", aa, "dB");
    // A distance from nothing means nothing: a sound with no top band of its own (the hum) is
    // judged by its share above 6 kHz alone, which should stay far under the others'.
    const bool has_top = ao > -60;
    if (has_top) {
      lab.metric(id + ".lsd-classic", lb, "dB");
      lab.metric(id + ".lsd-enhanced", la, "dB");
    }
    std::string gap_b = "-", gap_a = "-";
    if (m.gap_to > m.gap_from) {
      const double gb = level_db(before.samples, m.gap_from, m.gap_to);
      const double ga = level_db(after.samples, m.gap_from, m.gap_to);
      lab.metric(id + ".hiss-classic", gb, "dBFS");
      lab.metric(id + ".hiss-enhanced", ga, "dBFS");
      gap_b = f1(gb);
      gap_a = f1(ga);
    }
    table.rows.push_back({id, f1(ao), f1(ab), f1(aa), has_top ? f1(lb) : "-",
                          has_top ? f1(la) : "-", gap_b, gap_a});

    const std::vector<std::pair<std::string, std::string>> inputs = {
        {"source", "11025 Hz, 8 bits"}, {"out", "48000 Hz"}};
    ge::lab::sound_shot(lab,
                        {id + "-original", std::string(m.title) + ": THE ORIGINAL",
                         std::string(m.what) + ", made at 48 kHz with its whole band.",
                         "the octave bands up to 16 kHz: what the classic copy lost", inputs},
                        stereo(m.full));
    ge::lab::sound_shot(lab,
                        {id + "-classic", std::string(m.title) + ": AS THE PACK HOLDS IT",
                         "The same brought to 11 025 Hz and 8 bits, played back at 48 kHz unchanged.",
                         "nothing above about 5 kHz; quantisation hiss under the quiet parts",
                         inputs},
                        stereo(before.samples));
    ge::lab::sound_shot(lab,
                        {id + "-enhanced", std::string(m.title) + ": ENHANCED",
                         "The classic copy denoised, resampled and its top band rebuilt.",
                         "bands above 5 kHz back near the original's; the hum gets almost none; "
                         "the hiss gap lower",
                         inputs},
                        stereo(after.samples));
  }
  lab.table(std::move(table));
}

const ge::lab::LabSpec kSpec{
    "enhance-sound",
    ge::lab::LabKind::sim,
    "plugin:classicenhance",
    "plugins/classicenhance/sound",
    "classicenhance",
    "What does the sound enhancer make of a classic sound: is its lost top band back near the "
    "original's, without adding to a pure tone, and is the hiss lower?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace

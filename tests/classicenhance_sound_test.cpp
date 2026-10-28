// The sound enhancer (plugins/classicenhance/sound.hpp), on sounds made here: a WAV round trip
// keeps the samples and the loop; damage is refused with a next step; an 8-bit tone comes out at
// 48 kHz with its loop start moved to the new rate; the band above the source's edge is filled
// for a noisy sound and left nearly empty for a pure tone; a quiet hiss in a sound's gaps comes
// down; peaks stay under the knee; and the bits are the same every run.
//
// ge:summary Tests the sound enhancer: WAV codec, rate and loop, band extension, hiss, peaks
// ge:keywords test classicenhance sound wav loop cue denoise bandwidth-extension limiter
// ge:skill classicenhance testing
#include <cmath>
#include <vector>

#include "audio/fft.hpp"
#include "check.hpp"
#include "classicenhance/sound.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"

namespace ce = ge::classicenhance;

namespace {

constexpr double kPi = 3.14159265358979323846;

// An 8-bit 11 025 Hz sound as the pack holds one, quantised the way its bytes are.
ce::Pcm eight_bit(std::vector<float> x, long loop = -1) {
  for (float& v : x) v = float(std::round(double(v) * 128.0) / 128.0);
  ce::Pcm p;
  p.rate = 11025;
  p.bits = 8;
  p.samples = std::move(x);
  p.loop_start = loop;
  return p;
}

// The energy share at and above `hz` across the whole sound, dB, through a full Hann window (its
// sidelobes fall fast enough not to smear the low band upwards).
double above_db(const std::vector<float>& x, int rate, double hz) {
  const ge::audio::Fft fft(1024);
  const std::vector<double> win = ge::audio::stft_window(1024, false);
  const size_t k0 = size_t(hz / (double(rate) / 1024.0));
  std::vector<ge::audio::Bin> buf(1024);
  double hi = 0, all = 0;
  for (size_t at = 0; at + 1024 <= x.size(); at += 512) {
    for (size_t i = 0; i < 1024; i++) buf[i] = ge::audio::Bin(double(x[at + i]) * win[i], 0.0);
    fft.transform(buf, false);
    for (size_t k = 0; k <= 512; k++) {
      all += std::norm(buf[k]);
      if (k >= k0) hi += std::norm(buf[k]);
    }
  }
  return 10 * ge::core::libm::log(hi / all + 1e-30) / ge::core::libm::log(10.0);
}

}  // namespace

TEST(sound_wav_round_trip_keeps_samples_and_loop) {
  ce::Pcm p;
  p.rate = 22050;
  p.samples = {0.0f, 0.5f, -0.5f, 0.25f};
  p.loop_start = 2;
  const std::vector<uint8_t> bytes = ce::write_wav(p);
  ce::Pcm back;
  std::string error;
  CHECK_MSG(ce::read_wav(bytes, &back, &error), error);
  CHECK(back.rate == 22050 && back.bits == 16 && back.loop_start == 2);
  CHECK(back.samples.size() == 4 && std::fabs(back.samples[1] - 0.5f) < 1e-4f);
  std::vector<uint8_t> bad = bytes;
  bad[8] = 'X';
  CHECK(!ce::read_wav(bad, &back, &error));
  CHECK(error.find("next:") != std::string::npos);
}

TEST(sound_comes_out_at_48k_with_its_loop) {
  std::vector<float> x(11025);
  for (size_t i = 0; i < x.size(); i++)
    x[i] = float(0.6 * ge::core::libm::sin(2 * kPi * 440 * double(i) / 11025));
  const ce::Pcm out = ce::enhance_sound(eight_bit(x, 5000), {});
  CHECK(out.rate == 48000 && out.samples.size() == 48000);
  CHECK(out.loop_start == 21769);  // 5000 * 48000 / 11025, rounded
  CHECK(out.samples == ce::enhance_sound(eight_bit(x, 5000), {}).samples);
}

TEST(sound_band_is_filled_for_noise_not_for_a_tone) {
  ge::core::Rng rng{3};
  std::vector<float> noise(11025), tone(11025);
  for (size_t i = 0; i < noise.size(); i++) {
    noise[i] =
        float(0.4 * (ge::core::next(rng) * 2 - 1) * ge::core::libm::exp(-double(i) / 4000.0));
    tone[i] = float(0.5 * ge::core::libm::sin(2 * kPi * 330 * double(i) / 11025));
  }
  const ce::Pcm n = ce::enhance_sound(eight_bit(noise), {});
  const ce::Pcm t = ce::enhance_sound(eight_bit(tone), {});
  ce::SoundOptions off;
  off.extend = 0;
  const ce::Pcm plain = ce::enhance_sound(eight_bit(noise), off);
  CHECK_MSG(above_db(n.samples, 48000, 6000) > -15,
            "noise above 6 kHz: " + std::to_string(above_db(n.samples, 48000, 6000)));
  CHECK_MSG(above_db(plain.samples, 48000, 7000) < -60,
            std::to_string(above_db(plain.samples, 48000, 7000)));
  CHECK_MSG(above_db(t.samples, 48000, 6000) < -40,
            "a tone above 6 kHz: " + std::to_string(above_db(t.samples, 48000, 6000)));
}

TEST(sound_hiss_in_the_gaps_comes_down) {
  // A burst, then a gap of hiss: the gap's level falls; the burst's barely moves.
  ge::core::Rng rng{9};
  std::vector<float> x(22050);
  for (size_t i = 0; i < x.size(); i++) {
    const double hiss = 0.01 * (ge::core::next(rng) * 2 - 1);
    const double body =
        i < 5000 ? 0.5 * ge::core::libm::sin(2 * kPi * 300 * double(i) / 11025) : 0.0;
    x[i] = float(body + hiss);
  }
  ce::SoundOptions only;
  only.extend = 0;
  ce::SoundReport report;
  const std::vector<float> y = ce::reduce_noise(eight_bit(x).samples, 8, only, &report);
  double gap_in = 0, gap_out = 0, body_in = 0, body_out = 0;
  for (size_t i = 0; i < 4000; i++) body_in += double(x[i]) * x[i], body_out += double(y[i]) * y[i];
  for (size_t i = 10000; i < 20000; i++)
    gap_in += double(x[i]) * x[i], gap_out += double(y[i]) * y[i];
  CHECK_MSG(gap_out < gap_in * 0.3, "the gap kept " + std::to_string(gap_out / gap_in));
  CHECK(body_out > body_in * 0.9);
}

TEST(sound_peaks_stay_under_the_knee) {
  ge::core::Rng rng{5};
  std::vector<float> x(11025);
  for (size_t i = 0; i < x.size(); i++) x[i] = float(ge::core::next(rng) * 2 - 1);  // full scale
  const ce::Pcm out = ce::enhance_sound(eight_bit(x), {});
  float peak = 0;
  for (float v : out.samples) peak = std::max(peak, std::fabs(v));
  CHECK_MSG(peak <= 0.9f + 1e-6f, "peak " + std::to_string(peak));
}

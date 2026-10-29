// engine/cli/sound.hpp's arithmetic, held without ffmpeg on synthetic sounds:
//
//   * THE TRIM: silence off both ends; the pieces of one recording found at its gaps.
//   * THE LOUDNESS: a full-scale 1 kHz sine reads BS.1770's -3.01 LUFS at 48 kHz and at a game's
//     22.05 kHz; a shaped sound lands on the loudness asked for, or stops at the peak ceiling.
//   * THE MATCH: a sound against itself, or quieter, scores 0; a sound of another length and
//     shape scores worse than one close to it.
//   * THE FILES: the ffmpeg line, and a mono WAV's header at its own rate.
//
// ge:summary Tests for ge sound: trim, pieces, BS.1770 loudness, shaping, match score, WAV header
// ge:keywords test cli sound loudness lufs trim split match features wav
// ge:skill agent-interface audio testing
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "audio/wav.hpp"
#include "check.hpp"
#include "cli/sound.hpp"

namespace c = ge::cli;

namespace {

std::vector<float> sine(double hz, double seconds, int rate, double amp) {
  std::vector<float> x(static_cast<size_t>(seconds * rate));
  for (size_t i = 0; i < x.size(); i++) {
    x[i] = static_cast<float>(amp *
                              std::sin(2 * std::numbers::pi * hz * static_cast<double>(i) / rate));
  }
  return x;
}

std::vector<float> silence(double seconds, int rate) {
  return std::vector<float>(static_cast<size_t>(seconds * rate), 0.0f);
}

void append(std::vector<float>& a, const std::vector<float>& b) {
  a.insert(a.end(), b.begin(), b.end());
}

// A decaying noise burst: a door's slam, a footstep.
std::vector<float> burst(double seconds, int rate, uint32_t seed) {
  std::vector<float> x(static_cast<size_t>(seconds * rate));
  uint32_t s = seed;
  for (size_t i = 0; i < x.size(); i++) {
    s = s * 1664525u + 1013904223u;
    const double n = static_cast<double>(s >> 8) / 8388608.0 - 1.0;
    x[i] = static_cast<float>(0.8 * n * std::exp(-6.0 * static_cast<double>(i) / rate));
  }
  return x;
}

}  // namespace

TEST(silence_is_trimmed_off_both_ends) {
  const int rate = 22050;
  std::vector<float> x = silence(0.3, rate);
  append(x, sine(440, 0.5, rate, 0.5));
  append(x, silence(0.2, rate));
  const c::SampleSpan s = c::loud_span(x, -50);
  CHECK_MSG(
      s.begin >= static_cast<size_t>(0.3 * rate) && s.begin < static_cast<size_t>(0.3 * rate) + 5,
      std::to_string(s.begin));
  CHECK_MSG(
      s.end <= static_cast<size_t>(0.8 * rate) && s.end > static_cast<size_t>(0.8 * rate) - 60,
      std::to_string(s.end));
  const c::SampleSpan none = c::loud_span(silence(0.1, rate), -50);
  CHECK(none.begin == 0 && none.end == 0);
}

TEST(a_recording_splits_at_its_gaps) {
  const int rate = 22050;
  std::vector<float> x = silence(0.1, rate);
  for (int i = 0; i < 3; i++) {
    append(x, burst(0.25, rate, 7u + static_cast<uint32_t>(i)));
    append(x, silence(0.3, rate));
  }
  append(x, sine(1000, 0.01, rate, 0.5));  // a click too short to keep
  const auto pieces = c::sound_pieces(x, rate, -40, 0.15, 0.05);
  CHECK_MSG(pieces.size() == 3, std::to_string(pieces.size()));
  // A gap shorter than --gap joins two bursts into one piece.
  CHECK(c::sound_pieces(x, rate, -40, 0.5, 0.05).size() == 1);
}

TEST(a_full_scale_sine_reads_bs1770s_loudness) {
  for (const int rate : {48000, 22050}) {
    const double l = c::loudness_lufs(sine(997, 2.0, rate, 1.0), rate);
    CHECK_MSG(std::abs(l - -3.01) < 0.15, std::to_string(rate) + " Hz: " + std::to_string(l));
  }
  CHECK(c::loudness_lufs(silence(1, 22050), 22050) <= -199);
  CHECK(std::abs(c::peak_dbfs(sine(440, 0.5, 22050, 0.5)) - -6.02) < 0.05);
}

TEST(a_shaped_sound_lands_on_its_loudness_or_its_ceiling) {
  const int rate = 22050;
  std::vector<float> x = silence(0.2, rate);
  append(x, sine(440, 1.5, rate, 0.05));
  append(x, silence(0.4, rate));
  c::Shape s;
  const c::Shaped quiet = c::shape_sound(x, rate, s);
  CHECK_MSG(std::abs(quiet.lufs - -16) < 0.2, std::to_string(quiet.lufs));
  CHECK(!quiet.peak_limited && quiet.peak_db <= -1.5 + 1e-6);
  CHECK_MSG(std::abs(quiet.seconds - 1.5) < 0.01, std::to_string(quiet.seconds));
  // A spike's loudness would need its peak far over the ceiling: it stops there.
  std::vector<float> click = burst(0.005, rate, 3);
  append(click, sine(300, 0.6, rate, 0.004));
  const c::Shaped spike = c::shape_sound(click, rate, s);
  CHECK(spike.peak_limited && std::abs(spike.peak_db - -1.5) < 0.01);
  // --max cuts it; the tail is faded to nothing.
  s.max_s = 0.5;
  const c::Shaped cut = c::shape_sound(x, rate, s);
  CHECK(std::abs(cut.seconds - 0.5) < 0.001 && std::abs(cut.samples.back()) < 1e-6f);
}

TEST(the_match_ranks_a_close_sound_over_a_far_one) {
  const int rate = 22050;
  const std::vector<float> ref = burst(0.4, rate, 11);
  const c::SoundFeatures rf = c::sound_features(ref, rate);
  CHECK(std::abs(rf.seconds - 0.4) < 0.05);
  CHECK(c::match_score(rf, rf) == 0);
  std::vector<float> half = ref;
  for (float& v : half) v *= 0.5f;
  CHECK_MSG(c::match_score(rf, c::sound_features(half, rate)) < 0.01,
            std::to_string(c::match_score(rf, c::sound_features(half, rate))));
  const double near = c::match_score(rf, c::sound_features(burst(0.45, rate, 12), rate));
  const double far = c::match_score(rf, c::sound_features(sine(220, 3.0, rate, 0.5), rate));
  CHECK_MSG(near < far, std::to_string(near) + " vs " + std::to_string(far));
}

TEST(the_files_ffmpeg_and_a_mono_wav) {
  const std::string line = c::ffmpeg_decode_line("a b/door.ogg", "/tmp/x.f32", 22050);
  CHECK_MSG(
      line.find("-i \"a b/door.ogg\" -ac 1 -ar 22050 -f f32le \"/tmp/x.f32\"") != std::string::npos,
      line);
  const std::vector<float> x = {0.0f, 0.5f, -1.0f};
  const std::vector<uint8_t> w = ge::audio::wav_bytes(x, 1, 22050);
  CHECK(w.size() == 44 + 6);
  CHECK(w[22] == 1 && w[23] == 0);  // channels
  const uint32_t rate = w[24] | (w[25] << 8) | (w[26] << 16) | (static_cast<uint32_t>(w[27]) << 24);
  CHECK(rate == 22050);
  const uint32_t bps = w[28] | (w[29] << 8) | (w[30] << 16) | (static_cast<uint32_t>(w[31]) << 24);
  CHECK(bps == 44100);
  CHECK(w[46] == 0x00 && w[47] == 0x40);  // 0.5 x 32767 rounded half up: 16384
}

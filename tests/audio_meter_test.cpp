// The meter (engine/audio/meter.hpp), the WAV encoder (wav.hpp) and a sound in a lab
// (engine/lab/sound.hpp): a full-scale sine meters -3 dB with its energy in its own band, silence
// meters as silence, onset and length are where the sound is, the WAV is a valid 16-bit header
// over the samples, and `sound_shot` writes its metrics, its picture and its file.
//
// ge:summary Tests for the meter, the WAV bytes and the lab's sound shot
// ge:keywords test meter level band centroid onset length balance wav header sound-shot
// ge:skill audio labs testing
#include <string>
#include <vector>

#include "audio/dsp.hpp"
#include "audio/meter.hpp"
#include "audio/wav.hpp"
#include "check.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"

namespace {

using namespace ge::audio;

// `seconds` of a sine at `hz` and amplitude `a`, starting `delay` in, both sides.
std::vector<float> sine(float hz, float a, double seconds, double delay = 0) {
  const Tables t = make_tables(1);
  std::vector<float> v(2 * static_cast<size_t>((seconds + delay) * kRate), 0);
  float phase = 0;
  for (size_t i = static_cast<size_t>(delay * kRate); i < v.size() / 2; i++) {
    v[2 * i] = v[2 * i + 1] = a * sine_at(t, phase);
    phase = advance(phase, hz / kRate);
  }
  return v;
}

}  // namespace

TEST(a_full_scale_sine_meters_minus_three_in_its_own_band) {
  const Meter m = measure(sine(1000, 1, 0.5));
  CHECK(m.peak_db > -0.01f && m.peak_db <= 0);
  CHECK(m.loudest_db > -3.1f && m.loudest_db < -2.9f);
  int top = 0;
  for (int b = 1; b < kBands; b++) top = m.band_db[b] > m.band_db[top] ? b : top;
  CHECK(kBandHz[top] == 1000);
  CHECK(m.centroid_hz > 700 && m.centroid_hz < 1400);
  CHECK(m.balance > -0.001f && m.balance < 0.001f);
}

TEST(silence_onset_and_length) {
  const Meter quiet = measure(std::vector<float>(9600, 0));
  CHECK(quiet.loudest_db == kSilenceDb && quiet.length_s == 0);
  const Meter m = measure(sine(500, 0.1f, 0.3, 0.2));
  CHECK(m.onset_s > 0.18f && m.onset_s < 0.21f);
  CHECK(m.length_s > 0.28f && m.length_s < 0.32f);
  CHECK(m.envelope_db.size() == 50);
}

TEST(the_wav_header_and_samples) {
  const std::vector<float> v = {0, 1, -1, 0.5f};
  const std::vector<uint8_t> b = wav_bytes(v);
  CHECK(b.size() == 44 + 8);
  CHECK(std::string(b.begin(), b.begin() + 4) == "RIFF");
  CHECK(std::string(b.begin() + 8, b.begin() + 12) == "WAVE");
  CHECK(b[22] == 2 && b[34] == 16);
  CHECK((b[24] | b[25] << 8 | b[26] << 16) == kRate);
  // 1.0 is 32767, -1.0 is -32767, 0.5 rounds to 16384.
  CHECK(b[46] == 0xff && b[47] == 0x7f);
  CHECK(b[48] == 0x01 && b[49] == 0x80);
  CHECK(b[50] == 0x00 && b[51] == 0x40);
}

TEST(a_sound_shot_writes_metrics_a_picture_and_a_file) {
  static const ge::lab::LabSpec spec{
      "t", ge::lab::LabKind::sim, "engine", "", "audio", "?", {}, "", nullptr};
  ge::lab::Lab lab(spec, {});
  const Meter m = ge::lab::sound_shot(
      lab, {.id = "beep", .title = "A BEEP", .caption = "", .look_for = "", .inputs = {}},
      sine(440, 0.1f, 0.2));
  CHECK(m.loudest_db < -19 && m.loudest_db > -24);
  CHECK(lab.metrics().size() == 5);
  CHECK(lab.metrics()[0].key == "beep.loudest-db");
  CHECK(lab.shots().size() == 1);
  CHECK(lab.files().size() == 1 && lab.files()[0].name == "beep.wav");
}

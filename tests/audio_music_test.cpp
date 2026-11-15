// Music (engine/audio/music.hpp): a stereo FLAC track decoded off the game thread plays its left
// channel on the left and its right on the right at the music level; paused, it is silent and
// plays on from where it stopped; stopped, it fades out; a file that is not there is reported.
//
// ge:summary Tests the music player: stereo placement, pause and resume in place, stop, errors
// ge:keywords test music player flac stereo pause resume stop fade error
// ge:skill audio testing
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "audio/flac.hpp"
#include "audio/music.hpp"
#include "audio/synth.hpp"
#include "check.hpp"

namespace audio = ge::audio;
namespace fs = std::filesystem;

namespace {

// Left 4000 for the first half second and 12000 after; right always -6000; at 48 kHz.
std::string write_track() {
  std::vector<int32_t> s;
  for (int i = 0; i < 96000; i++) {
    s.push_back(i < 24000 ? 4000 : 12000);
    s.push_back(-6000);
  }
  const fs::path p = fs::temp_directory_path() / "ge_music_test.flac";
  const std::vector<uint8_t> b = audio::flac_encode(s, 2, 48000, 16);
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  return p.string();
}

void until_sounding(audio::MusicPlayer& m, audio::Synth& synth) {
  for (int i = 0; i < 400 && !m.sounding(); i++) {
    m.frame(synth);
    if (!m.sounding()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  CHECK(m.sounding());
}

// The last frame of `frames` rendered: left and right.
std::pair<float, float> render(audio::Synth& synth, int frames) {
  std::vector<float> out(static_cast<size_t>(frames) * 2);
  synth.render(out.data(), frames);
  return {out[out.size() - 2], out[out.size() - 1]};
}

bool near(float a, float b) { return std::fabs(a - b) < 0.004f; }

}  // namespace

TEST(music_plays_left_and_right_pauses_in_place_and_stops) {
  const std::string path = write_track();
  audio::Synth synth;
  audio::MusicPlayer m;
  m.play(path, false);
  until_sounding(m, synth);
  const float lo = 4000.0f / 32768 * audio::kMusicLevel, hi = 12000.0f / 32768 * audio::kMusicLevel;
  const float right = -6000.0f / 32768 * audio::kMusicLevel;
  auto [l, r] = render(synth, 12000);
  CHECK_MSG(near(l, lo) && near(r, right), std::to_string(l) + " " + std::to_string(r));

  m.pause();
  m.frame(synth);
  std::tie(l, r) = render(synth, 9600);
  CHECK(near(l, 0) && near(r, 0));
  m.resume();
  m.frame(synth);
  std::tie(l, r) = render(synth, 4800);  // 16800 frames in: still the first half
  CHECK_MSG(near(l, lo), std::to_string(l));
  std::tie(l, r) = render(synth, 9600);  // 26400: the second
  CHECK_MSG(near(l, hi), std::to_string(l));

  m.gain(0.5f);
  m.frame(synth);
  std::tie(l, r) = render(synth, 14400);
  CHECK_MSG(near(l, hi * 0.5f), std::to_string(l));

  m.play(path, false);  // asked again while it plays: it carries on
  m.frame(synth);
  CHECK(m.sounding());
  m.stop();
  m.frame(synth);
  CHECK(!m.sounding() && m.playing().empty());
  std::tie(l, r) = render(synth, 48000);
  CHECK_MSG(near(l, 0) && near(r, 0), std::to_string(l) + " " + std::to_string(r));
  m.frame(synth);  // nothing reads the track any more: freed

  m.play((fs::temp_directory_path() / "ge_music_test_missing.flac").string(), true);
  for (int i = 0; i < 400 && m.playing().size(); i++) {
    m.frame(synth);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(m.take_error().find("could not be opened") != std::string::npos);
}

TEST(music_from_flac_spreads_a_mono_file_on_both_sides) {
  const std::vector<uint8_t> b = audio::flac_encode(std::vector<int32_t>(100, 16384), 1, 22050, 16);
  audio::MusicTrack t;
  std::string error;
  CHECK_MSG(audio::music_from_flac(b, "mono.flac", &t, &error), error);
  CHECK(t.left.frames() == 100 && t.right.samples == t.left.samples && t.left.rate == 22050);
  CHECK(t.left.samples[0] == 0.5f);
  CHECK(!audio::music_from_flac(std::vector<uint8_t>{1, 2, 3}, "bad.flac", &t, &error));
  CHECK(error.rfind("bad.flac: ", 0) == 0);
}

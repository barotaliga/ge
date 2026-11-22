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
#include "audio/music_console.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "console/console.hpp"

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

TEST(the_music_switch_off_stops_the_track_and_on_plays_it_again) {
  // `enabled` (cd on|off, a game's own switch bound to it) followed by music_frame.
  const std::string path = write_track();
  audio::Synth synth;
  ge::console::Console con;
  audio::MusicConsole m;
  audio::add_music_commands(con, m);
  m.track_of = [&path](int track) { return track == 3 ? path : std::string{}; };
  audio::play_level_music(m, 3);
  audio::music_frame(m, synth, con);
  CHECK(m.track == 3 && !m.player.playing().empty());
  m.enabled = false;  // switched off: stopped, the track kept
  audio::music_frame(m, synth, con);
  CHECK(m.player.playing().empty() && m.track == 3);
  audio::play_level_music(m, 3);  // a level starting while off: nothing
  CHECK(m.player.playing().empty());
  m.enabled = true;  // on again: the same track
  audio::music_frame(m, synth, con);
  CHECK(!m.player.playing().empty());
  m.player.stop();
  audio::music_frame(m, synth, con);
}

TEST(shuffled_music_plays_the_games_tracks_and_the_menu_has_its_own) {
  const std::string path = write_track();
  audio::MusicConsole m;
  m.track_of = [&path](int track) { return track == 2 || track == 5 ? path : std::string{}; };
  m.shuffle = true;  // a level's own track 9 is passed over for one the game has
  audio::play_level_music(m, 9);
  CHECK(m.track == 2 || m.track == 5);
  CHECK(!m.player.playing().empty() && !m.player.loops());  // once through, then the next
  m.player.stop();
  m.shuffle = false;
  m.menu_track = 5;  // no game running: the menu's track, started once
  audio::play_menu_music(m);
  CHECK(m.track == 5 && m.player.loops());
  audio::play_level_music(m, 2);
  CHECK(m.track == 2);
  audio::play_menu_music(m);  // back at the menu: its track again
  CHECK(m.track == 5);
  m.player.stop();
  m.track = 0;
  audio::play_menu_music(m);  // asked already: the music left as it is
  CHECK(m.track == 0);
}

// `music` (Quake II's `ogg`): a track played by hand once through, then the level's again; info,
// pause and toggle, next, stop and the refusals.
TEST(the_music_command_plays_a_track_by_hand_then_the_levels_again) {
  const std::string path = write_track();
  audio::Synth synth;
  ge::console::Console con;
  audio::MusicConsole m;
  audio::add_music_commands(con, m);
  m.track_of = [&path](int track) { return track == 3 || track == 4 ? path : std::string{}; };
  con.execute("music info");
  CHECK(con.output().back() == "Playback stopped.");
  con.execute("music pause");
  CHECK(con.output().back() == "Playback stopped.");
  con.execute("music");
  CHECK(con.output().back() == "Usage: music <info|play|stop|next|pause>");
  con.execute("music play");
  CHECK(con.output().back() == "Usage: music play <track>");
  con.execute("music play 9");
  CHECK(con.output().back() == "No such track: 9");
  con.execute("music play foo");
  CHECK(con.output().back() == "No such track: foo");
  audio::play_level_music(m, 3);
  con.execute("music play 4");
  CHECK(m.manual == 4 && m.track == 3 && !m.player.loops());
  con.execute("music info");
  CHECK(con.output().back().rfind("Playing track 4 (", 0) == 0);
  con.execute("music toggle");
  CHECK(m.player.paused());
  con.execute("music pause 0");
  CHECK(!m.player.paused());
  m.player.stop();  // the hand's track ended: the level's own again, looping
  audio::music_frame(m, synth, con);
  CHECK(m.manual == 0 && m.track == 3 && m.player.loops());
  con.execute("music next");  // the level's track from its start
  CHECK(m.track == 3 && !m.player.playing().empty());
  con.execute("music stop");
  CHECK(m.player.playing().empty() && m.track == 3);
  audio::music_frame(m, synth, con);
}

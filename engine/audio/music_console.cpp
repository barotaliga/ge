// The music's console: see music_console.hpp.
//
// ge:summary The bgmvolume cvar and the cd command over a music player
// ge:keywords music bgmvolume cd console
// ge:skill audio console
// ge:test tests/audio_music_test.cpp
#include "audio/music_console.hpp"

#include <cstdlib>

namespace ge::audio {

namespace {

bool start(console::Console& con, MusicConsole& m, const std::string& word, bool loop) {
  const int n = std::atoi(word.c_str());
  const std::string path = m.track_of && n > 0 ? m.track_of(n) : std::string{};
  if (path.empty()) {
    con.print(n > 0 ? "There is no music for track " + std::to_string(n) +
                          ". Take your game in from its disc or install to hear its soundtrack."
                    : "Which track? For example: cd play 2");
    return false;
  }
  m.track = n;
  m.player.play(path, loop);
  return true;
}

}  // namespace

void add_music_commands(console::Console& con, MusicConsole& m) {
  con.cvars().bind({.name = "bgmvolume",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "The music's loudness, beside the game's own sound.",
                    .kind = console::CvarKind::number,
                    .min = 0,
                    .max = 1,
                    .values = "0: no music; 0..1: that loudness (1 the default)"},
                   &m.volume);
  con.add_command(
      "cd", "cd play|loop <track>, cd stop, cd pause, cd resume, cd info: the game's music",
      [&m](console::Console& c, const console::Args& a) {
        const std::string& what = a[1];
        if (what == "play" || what == "loop") {
          start(c, m, a[2], what == "loop");
        } else if (what == "stop") {
          m.player.stop();
          m.track = 0;
        } else if (what == "pause") {
          m.player.pause();
        } else if (what == "resume") {
          m.player.resume();
        } else if (what == "on" || what == "off") {
          m.enabled = what == "on";
          if (!m.enabled) m.player.stop();
        } else if (what == "info" || what.empty()) {
          if (m.player.playing().empty()) {
            c.print("No music is playing.");
          } else {
            c.print(std::string(m.player.paused() ? "Paused on" : "Playing") + " track " +
                    std::to_string(m.track) + (m.player.loops() ? ", looping." : "."));
          }
        } else {
          c.print("The cd command takes play, loop, stop, pause, resume, on, off or info.");
        }
      });
}

namespace {

// A random track the game has other than the one playing (0: none it has).
int shuffled_track(MusicConsole& m) {
  int have[99];
  int n = 0;
  for (int t = 1; t <= 99 && m.track_of; t++)
    if (t != m.track && !m.track_of(t).empty()) have[n++] = t;
  if (n == 0) return m.track > 0 && m.track_of && !m.track_of(m.track).empty() ? m.track : 0;
  return have[core::pick_index(m.shuffle_rng, static_cast<size_t>(n))];
}

}  // namespace

void play_menu_music(MusicConsole& m) {
  if (!m.enabled || m.menu_track == m.menu_played) return;  // asked once: `cd` may change it
  const int track = m.menu_track;
  if (track > 0) play_level_music(m, track);
  m.menu_played = track;
}

void play_level_music(MusicConsole& m, int track) {
  m.menu_played = 0;  // a level's: back at the menu, its track again
  if (!m.enabled) return;
  if (m.shuffle && track > 0) {  // the game's tracks in a random order, each once through
    if (!m.player.playing().empty() && !m.player.loops()) return;  // a shuffled one plays
    const int t = shuffled_track(m);
    if (t == 0) return;
    m.track = t;
    m.player.play(m.track_of(t), false);
    return;
  }
  const std::string path = m.track_of && track > 0 ? m.track_of(track) : std::string{};
  if (path.empty()) {
    m.player.stop();
    m.track = 0;
    return;
  }
  m.track = track;
  m.player.play(path, true);
}

void music_frame(MusicConsole& m, Synth& synth, console::Console& con) {
  if (m.enabled != m.was_enabled) {
    m.was_enabled = m.enabled;
    if (!m.enabled)
      m.player.stop();
    else if (m.track > 0)
      play_level_music(m, m.track);
  }
  if (m.enabled && m.shuffle && m.track > 0 && m.player.playing().empty()) {
    const int t = shuffled_track(m);  // one ended: the next
    if (t > 0) {
      m.track = t;
      m.player.play(m.track_of(t), false);
    }
  }
  m.player.gain(static_cast<float>(m.volume));
  m.player.frame(synth);
  if (const std::string e = m.player.take_error(); !e.empty()) {
    con.print("The music for track " + std::to_string(m.track) + " could not be played.");
  }
}

}  // namespace ge::audio

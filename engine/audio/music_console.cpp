// The music's console: see music_console.hpp.
//
// ge:summary The bgmvolume cvar, the cd and music commands over a music player
// ge:keywords music bgmvolume cd console ogg music-command manual-track
// ge:skill audio console
// ge:test tests/audio_music_test.cpp
#include "audio/music_console.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

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

// `music <info|play|stop|next|pause|toggle>`, the player's buttons over the level's own music.
void music_command(console::Console& c, MusicConsole& m, const console::Args& a) {
  const std::string& what = a[1];
  const std::string usage = "Usage: " + a[0] + " <info|play|stop|next|pause>";
  if (what == "info") {
    if (m.player.playing().empty()) {
      c.print("Playback stopped.");
      return;
    }
    const int n = m.manual > 0 ? m.manual : m.track;
    const std::string which = m.manual < 0              ? m.manual_name
                              : m.manual == 0 && n == 0 ? m.named
                                                        : "track " + std::to_string(n);
    c.print(std::string(m.player.paused() ? "Paused on " : "Playing ") + which + " (" +
            m.player.playing() + ")" + (m.player.loops() ? ", looping." : "."));
  } else if (what == "play") {
    if (m.held) {
      c.print("Can't play music in cinematic mode.");
      return;
    }
    if (a.size() < 3) {
      c.print("Usage: " + a[0] + " play <track>");
      return;
    }
    const std::string& word = a[2];
    const bool digits = !word.empty() && word.find_first_not_of("0123456789") == std::string::npos;
    const int n = digits ? std::atoi(word.c_str()) : 0;
    const std::string path = digits ? (m.track_of && n > 0 ? m.track_of(n) : std::string{})
                                    : (m.file_of ? m.file_of(word) : std::string{});
    if (path.empty()) {
      c.print("No such track: " + word);
      return;
    }
    if (a[3] != "soft") m.player.stop();  // soft: the new track takes over as the old fades
    m.manual = digits ? n : -1;
    m.manual_name = digits ? std::string{} : word;
    m.player.play(path, false);  // once through, then the level's own again
  } else if (what == "stop") {
    m.player.stop();
    m.manual = 0;
  } else if (what == "next") {
    m.manual = 0;
    m.player.stop();
    if (m.enabled && m.track > 0)
      play_level_music(m, m.track);  // shuffled: the next one
    else if (m.enabled && !m.named.empty())
      play_level_named(m, std::exchange(m.named, {}));
  } else if (what == "pause" || what == "toggle") {
    if (m.player.playing().empty()) {
      c.print("Playback stopped.");
      return;
    }
    const bool pause = a.size() > 2 ? std::atoi(a[2].c_str()) != 0 : !m.player.paused();
    if (pause)
      m.player.pause();
    else
      m.player.resume();
  } else {
    c.print(usage);
  }
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
  con.add_command("music",
                  "music info|play <track> [soft]|stop|next|pause [0|1]: the music player, a track "
                  "played by hand once through before the level's own again",
                  [&m](console::Console& c, const console::Args& a) { music_command(c, m, a); });
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
  m.named.clear();
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

void play_level_named(MusicConsole& m, const std::string& name) {
  // Asked again each frame while the level lasts: the same name (without case) plays on.
  const bool same = name.size() == m.named.size() &&
                    std::equal(name.begin(), name.end(), m.named.begin(), [](char a, char b) {
                      return std::tolower(static_cast<unsigned char>(a)) ==
                             std::tolower(static_cast<unsigned char>(b));
                    });
  if (same && !name.empty()) return;
  m.menu_played = 0;
  m.named = name;
  m.track = 0;
  const std::string path = m.file_of && !name.empty() ? m.file_of(name) : std::string{};
  if (!m.enabled || m.manual != 0) return;  // a track played by hand plays on until it ends
  if (path.empty()) {
    m.player.stop();
    return;
  }
  m.player.play(path, true);
}

namespace {

// The level's own track again (a number or a name), when it has one.
void level_again(MusicConsole& m) {
  if (m.track > 0)
    play_level_music(m, m.track);
  else if (!m.named.empty())
    play_level_named(m, std::exchange(m.named, {}));
}

}  // namespace

void music_frame(MusicConsole& m, Synth& synth, console::Console& con) {
  if (m.held != m.was_held) {  // a movie: the music stopped and forgotten, the next level's anew
    m.was_held = m.held;
    if (m.held) {
      m.player.stop();
      m.manual = 0;
      m.track = 0;
      m.named.clear();
    }
  }
  if (m.manual != 0 && m.player.playing().empty()) {  // a track played by hand ended
    m.manual = 0;
    if (m.enabled) level_again(m);
  }
  if (m.enabled != m.was_enabled && !m.held) {
    m.was_enabled = m.enabled;
    if (!m.enabled && m.off_pauses && !m.player.playing().empty())
      m.player.pause();  // paused where it is: on again resumes it there
    else if (!m.enabled)
      m.player.stop();
    else if (m.off_pauses && m.player.paused())
      m.player.resume();
    else
      level_again(m);
  }
  if (m.enabled && m.shuffle && m.manual == 0 && m.track > 0 && m.player.playing().empty()) {
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

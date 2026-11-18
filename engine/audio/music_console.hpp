// THE MUSIC'S CONSOLE, for any game with a soundtrack: the music's loudness (`bgmvolume`, 0..1, on
// its own fader beside the game's `volume`), the CD player's buttons (`cd play <n>`, `cd loop
// <n>`, `cd stop`, `cd pause`, `cd resume`, `cd info`) and the level's own track. A game binds them
// once with the way a track number becomes a kept file (`track_of`: "" when the game has none),
// calls `music_frame` each frame and `play_level_music` when a level starts or a server names a
// track. The words are the player's (docs/architecture.md § Player text).
//
// ge:summary The music's console for any game: bgmvolume, cd play/loop/stop/pause/resume/info
// ge:keywords music bgmvolume cd cdtrack console track level-music play loop pause resume
// ge:skill audio console
// ge:test tests/audio_music_test.cpp
#pragma once

#include <functional>
#include <string>

#include "audio/music.hpp"
#include "console/console.hpp"

namespace ge::audio {

struct MusicConsole {
  MusicPlayer player;
  double volume = 1;  // bgmvolume
  bool enabled = true;
  std::function<std::string(int)> track_of;  // a track number's kept file, "" when none
  int track = 0;                             // the track asked last (0 none)
};

// The cvar and the `cd` command over `m` (it must outlive `con`).
void add_music_commands(console::Console& con, MusicConsole& m);

// A level started, or its server named `track` (0: none): played looping, unless it already
// plays. A track the game does not have stops the music and says nothing.
void play_level_music(MusicConsole& m, int track);

// Once a frame: the volume followed, a decode finished started, a failure printed to `con`.
void music_frame(MusicConsole& m, Synth& synth, console::Console& con);

}  // namespace ge::audio

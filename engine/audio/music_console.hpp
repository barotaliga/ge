// THE MUSIC'S CONSOLE, for any game with a soundtrack: the music's loudness (`bgmvolume`, 0..1, on
// its own fader beside the game's `volume`), the CD player's buttons (`cd play <n>`, `cd loop
// <n>`, `cd stop`, `cd pause`, `cd resume`, `cd info`) and the level's own track. A game binds them
// once with the way a track number becomes a kept file (`track_of`: "" when the game has none),
// calls `music_frame` each frame and `play_level_music` when a level starts or a server names a
// track. The words are the player's (docs/architecture.md § Player text). With `shuffle` (a game's
// own switch bound to it) a level plays every track the game has in a random order instead of its
// own, each once through, a new one as one ends (never the same twice running when there are two).
// `menu_track` (a game's own setting bound to it, 0 none) is the track `play_menu_music` starts
// while no game runs. A track may also be a NAME (`file_of`: a name's kept file, "" none): a
// server that names its level's music by file (`play_level_named`), or `music play <name>`.
// `off_pauses` (a game that pauses its music when switched off, as Quake II does): `enabled` off
// pauses the music where it is and on resumes it there; otherwise off stops it. While a game's
// movie plays (`held`) the music is stopped and forgotten: the next level starts its track afresh.
//
// THE PLAYER'S BUTTONS (`music`, which a game may also spell its own way): `music info` names the
// track playing ("Playback stopped." when none), `music play <n|name> [soft]` plays track n (or the
// file `name` names under the music folder) once through
// and then the level's own track again (soft: without cutting the one playing first), `music stop`,
// `music next` (the level's track from its start, or the next shuffled one) and `music pause [0|1]`
// (`toggle` the same: without a number it flips). Refusals: "Usage: music <info|play|stop|next|
// pause>", "Usage: music play <track>", "No such track: <n>".
//
// ge:summary The music's console for any game: bgmvolume, cd play/loop/stop/pause/resume/info,
// ge:summary music info/play/stop/next/pause
// ge:keywords music bgmvolume cd cdtrack console track level-music play loop pause resume
// ge:keywords shuffle menu-track play_menu_music music-command ogg manual-track toggle
// ge:skill audio console
// ge:test tests/audio_music_test.cpp
#pragma once

#include <functional>
#include <string>

#include "audio/music.hpp"
#include "console/console.hpp"
#include "core/prng.hpp"

namespace ge::audio {

struct MusicConsole {
  MusicPlayer player;
  double volume = 1;        // bgmvolume
  bool enabled = true;      // `cd on|off`, or a game's own switch bound to it
  bool was_enabled = true;  // music_frame's: turned off, the music stops; on, the track again
  std::function<std::string(int)> track_of;  // a track number's kept file, "" when none
  // A track's name's kept file (a path under the music folder, its extension optional), "" none.
  std::function<std::string(const std::string&)> file_of;
  std::string named;     // the level's track asked by name ("" none: `track`'s number)
  int track = 0;         // the track asked last (0 none)
  bool shuffle = false;  // every track in a random order (a game's switch)
  int menu_track = 0;    // the track while no game runs (0 none)
  int menu_played = 0;   // play_menu_music's: the menu track started
  int manual = 0;  // `music play`'s track, playing once through (0 none: the level's; -1 a name)
  std::string manual_name;  // `music play <name>`'s name while it plays
  bool held = false;        // a game's movie plays with its own sound: `play` refused while it does
  bool was_held = false;    // music_frame's: a movie began, the music stopped once
  bool off_pauses = false;  // `enabled` off pauses (and on resumes) instead of stopping
  core::Rng shuffle_rng = core::make_rng(0x5eed7ac);
};

// The cvar and the `cd` command over `m` (it must outlive `con`).
void add_music_commands(console::Console& con, MusicConsole& m);

// A level started, or its server named `track` (0: none): played looping, unless it already
// plays. A track the game does not have stops the music and says nothing.
void play_level_music(MusicConsole& m, int track);

// A level's server named its track `name` (a file under the music folder, through `file_of`):
// played looping unless it already plays; a name with no file (or "") stops the music.
void play_level_named(MusicConsole& m, const std::string& name);

// No game runs (a title, a menu), called each frame: `menu_track` started looping once (0: the
// music left as it is), again only after a level's track or when `menu_track` changes.
void play_menu_music(MusicConsole& m);

// Once a frame: the volume followed, `enabled` followed (off stops the music, on again plays the
// last track asked), a decode finished started, a failure printed to `con`.
void music_frame(MusicConsole& m, Synth& synth, console::Console& con);

}  // namespace ge::audio

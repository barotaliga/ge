// A GAME POINTED AT IS TAKEN IN, NEVER PLAYED FROM WHERE IT IS: a folder, a mounted disc or USB
// stick, or a disc image named by `-basedir`, LOAD GAME, SELECT GAME's folder chooser, the
// console's `basedir` or `import`, is copied into the game's own data folder by the shooter content
// layer (plugins/classic/content/import.hpp: the packs, the loose game files, the soundtrack as
// music/trackNN.flac), and the copy is what opens. Pointing at a game already in the data folder
// opens it as it is.
//
// The data folder is `data/` in the player's own settings folder, which outlives the game's
// program (a rebuild, a new version, the app moved or deleted): on a Mac
// `~/Library/Application Support/gameengine2/shooters_edge/data/`, on Linux
// `~/.local/share/gameengine2/shooters_edge/data/`, on Windows
// `%APPDATA%\gameengine2\shooters_edge\data\`. Only when that cannot be written is it `data/`
// beside the game (the folder it was placed in: the one holding the app on a Mac). A game taken in
// once is not copied again: a second launch pointed at the same unchanged source opens the copy at
// once (import.hpp § Taken in once). A game the window was started inside, or placed above, is
// still played where it is (client/install_search.hpp): that install is the game's home, its
// configs and place names with it.
//
// THE KEPT MUSIC plays from there too: `add_take_in_commands` points the engine's music console
// (engine/audio/music_console.hpp: `bgmvolume`, `cd`, bound by the window before the game's own
// settings) at `<basedir>/music/trackNN.flac`, and
// `level_music` plays the track a level's world names when the level starts.
//
// ge:summary The window's games taken in: a folder, volume or disc image copied into data/ first
// ge:keywords import take-in data-folder copy iso cue disc volume basedir load-game data_root
// ge:keywords take_in import-command soundtrack take_in_basedir take_in_ask add_take_in_commands
// ge:keywords level_music cd bgmvolume prefs taken-once adopt_placed_settings
// ge:skill shooter
// ge:test tests/shooters_edge_take_in_test.cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "audio/music_console.hpp"
#include "console/console.hpp"

namespace shooters_edge::client {

// `<prefs>/data` when it can be written, else `<placed>/data`; "" when neither can.
std::string data_root(const std::string& placed, const std::string& prefs);

// The series' settings (settings.hpp) a build that kept its data beside the game wrote there,
// copied into `root` when `root` has none yet; true when one was copied.
bool adopt_placed_settings(const std::string& placed, const std::string& root);

// Whether `path` lies inside `root` (both as they resolve on disk).
bool inside(const std::string& path, const std::string& root);

// `from` taken in: true with the folder to open in `*basedir` (`from` itself when it is already in
// `root`, else the game's copy under it) and what was done in `*summary` (a line for the console);
// false with what failed in the game's words in `*error`. `progress` gets a line before each file.
bool take_in(const std::string& from, const std::string& root, std::string* basedir,
             std::string* summary, std::string* error,
             const std::function<void(const std::string&)>& progress = {});

// `-basedir` taken in before the window chooses its install: `*basedir` becomes the copy (left as
// it is when empty or already in `root`); false, said on the error stream, when it cannot be.
bool take_in_basedir(std::string* basedir, const std::string& root);

// The console's `basedir <folder>` (no folder: the one playing, `*basedir`) and `import <folder or
// disc image>`: each sets `*ask`, which the window takes in and opens. With `music`, its tracks
// read from the game playing's folder (`*basedir`).
void add_take_in_commands(ge::console::Console& con, std::string* ask, const std::string* basedir,
                          ge::audio::MusicConsole* music = nullptr);

// A level started: the track its world's `sounds` names (`entities`, the map's entity text) loops.
void level_music(ge::audio::MusicConsole& music, std::string_view entities);

// The folder or image in `*ask` taken in, `*ask` set to the copy; `print` gets each step and the
// result. False with the refusal in `*error` (printed too).
bool take_in_ask(std::string* ask, const std::string& root, std::string* error,
                 const std::function<void(const std::string&)>& print);

}  // namespace shooters_edge::client

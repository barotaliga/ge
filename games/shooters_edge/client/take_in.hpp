// A GAME POINTED AT IS TAKEN IN, NEVER PLAYED FROM WHERE IT IS: a folder, a mounted disc or USB
// stick, or a disc image named by `-basedir`, LOAD GAME, SELECT GAME's folder chooser, the
// console's `basedir` or `import`, is copied into the game's own data folder by the shooter content
// layer (plugins/shooter/content/import.hpp: the packs, the loose game files, the soundtrack as
// music/trackNN.flac), and the copy is what opens. Pointing at a game already in the data folder
// opens it as it is.
//
// The data folder is `data/` beside the game (the folder it was placed in: the one holding the app
// on a Mac), else `data/` under the player's settings folder when that one cannot be written (an
// app in /Applications). A game the window was started inside, or placed above, is still played
// where it is (client/install_search.hpp): that install is the game's home, its configs and place
// names with it.
//
// ge:summary The window's games taken in: a folder, volume or disc image copied into data/ first
// ge:keywords import take-in data-folder copy iso cue disc volume basedir load-game data_root
// ge:keywords take_in import-command soundtrack take_in_basedir take_in_ask add_take_in_commands
// ge:skill shooter
// ge:test tests/shooters_edge_take_in_test.cpp
#pragma once

#include <functional>
#include <string>

#include "console/console.hpp"

namespace shooters_edge::client {

// `<placed>/data` when it can be written, else `<prefs>/data`; "" when neither can.
std::string data_root(const std::string& placed, const std::string& prefs);

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
// disc image>`: each sets `*ask`, which the window takes in and opens.
void add_take_in_commands(ge::console::Console& con, std::string* ask, const std::string* basedir);

// The folder or image in `*ask` taken in, `*ask` set to the copy; `print` gets each step and the
// result. False with the refusal in `*error` (printed too).
bool take_in_ask(std::string* ask, const std::string& root, std::string* error,
                 const std::function<void(const std::string&)>& print);

}  // namespace shooters_edge::client

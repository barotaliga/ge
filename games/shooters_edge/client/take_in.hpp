// A GAME POINTED AT IS TAKEN IN, NEVER PLAYED FROM WHERE IT IS: a folder, a mounted disc or USB
// stick, or a disc image named by `fs_basedir_<id>` (client/installs.hpp), LOAD GAME, SELECT
// GAME's folder chooser or the console's `import`, is copied into the game's own data folder by
// the shooter content layer (plugins/classic/content/import.hpp: the packs, the loose game files,
// the soundtrack as music/trackNN.flac), and the copy is what opens. Pointing at a game already in
// the data folder opens it as it is.
//
// The data folder is `data/` in the player's own settings folder, which outlives the game's
// program (a rebuild, a new version, the app moved or deleted): on a Mac
// `~/Library/Application Support/gameengine2/shooters_edge/data/`, on Linux
// `~/.local/share/gameengine2/shooters_edge/data/`, on Windows
// `%APPDATA%\gameengine2\shooters_edge\data\`. Only when that cannot be written is it `data/`
// beside the game (the folder it was placed in: the one holding the app on a Mac). A game taken in
// once is not copied again: a second launch pointed at the same unchanged source opens the copy at
// once (import.hpp § Taken in once). An install found beside the game (client/install_search.hpp)
// is taken in the same way when `games <n>` loads it.
//
// THE COPY RUNS ON ITS OWN THREAD (`TakeIn`): the window goes on drawing, the console held down
// with the copy's bar on its foot ("Copying Quake", the share of the files' bytes; then "Taking
// Quake's music", the share of its tracks), as a map downloading is shown.
//
// A MISSION PACK comes in with its game (beside its base folder) or alone (its disc, its folder):
// the line says which, how to start it (`-game <folder>`), and when the game itself is still to be
// added. A pack's own music (its disc's tracks, `<basedir>/<pack>/music/`) plays before the
// game's when the pack is the game folder playing.
//
// THE KEPT MUSIC plays from there too: `add_take_in_commands` points the engine's music console
// (engine/audio/music_console.hpp: `bgmvolume`, `cd`, bound by the window before the game's own
// settings) at `<basedir>/music/trackNN.flac`, and
// `level_music` plays the track a level's world names when the level starts.
//
// ge:summary The window's games taken in: a folder, volume or disc image copied into data/ first
// ge:keywords import take-in data-folder copy iso cue disc volume basedir load-game data_root
// ge:keywords take_in import-command soundtrack take_in_basedir take_in_ask add_take_in_commands
// ge:keywords level_music cd bgmvolume prefs taken-once homedir home-dir
// ge:keywords mission-pack take_in_words pack-music offer_words find_offers offered
// ge:skill shooter
// ge:test tests/shooters_edge_take_in_test.cpp
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "audio/music_console.hpp"
#include "classic/content/import.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "shooters_edge/client/installs.hpp"

namespace shooters_edge::client {

struct DemoState;

// `<prefs>/data` when it can be written, else `<placed>/data`; "" when neither can. With `home`
// (`homedir`, below) the player's own folder, when it can be written, before both.
std::string data_root(const std::string& placed, const std::string& prefs,
                      const std::string& home = "");

// THE PLAYER'S OWN DATA FOLDER, `homedir` (Quake II's spelling, any game's here): `+set homedir
// <folder>` on the command line puts everything the game writes there (the games taken in, the
// settings, screenshots, recordings, downloads), in place of the settings folder above. Read from
// `*plus` and taken out of it (the setting is read only once the game runs); "" when not given.
std::string take_home_dir(std::vector<std::string>* plus);

// `homedir` in the console: the data folder in use (`root`), read only.
void add_home_dir(ge::console::Console& con, const std::string& root);

// Whether `path` lies inside `root` (both as they resolve on disk).
bool inside(const std::string& path, const std::string& root);

// `from` taken in: true with the folder to open in `*basedir` (`from` itself when it is already in
// `root`, else the game's copy under it) and what was done in `*summary` (a line for the console);
// false with what failed in the game's words in `*error`. `progress` gets a line before each file,
// `share` what is being done and its share done for a bar (import.hpp's `ImportOptions::share`).
bool take_in(const std::string& from, const std::string& root, std::string* basedir,
             std::string* summary, std::string* error,
             const std::function<void(const std::string&)>& progress = {},
             const std::function<void(const std::string&, double)>& share = {});

// The console line saying what `take_in` did (`r`, from the source `from`): the game, the game
// with its mission packs, or the packs alone; how a pack is started (`-game <folder>`); and, for
// packs taken in without their game, that the game must be added too.
std::string take_in_words(const ge::classic::content::ImportReport& r, const std::string& from);

// A mission pack found on this machine and not yet taken in (`find_offers`), offered in the
// console at the start: "Found The Reckoning, a mission pack for Quake II, in <folder>. Type
// import "<folder>" to take it in."
std::string offer_words(const ge::classic::content::Offer& o);
// Each pack `find_offers` finds in the stores' libraries and on the mounted volumes, printed in
// `con` (none when `data_root` is empty).
void offer_packs(ge::console::Console& con, const std::string& data_root);

// The played game's `fs_basedir_<id>` (`base`'s, none for Shooter's Edge's own) before the window
// chooses its install: left as it is when empty or already in `root`, else the copy when it was
// taken in before and has not changed since. Any other folder, with `later` (a window a player sits
// at), is returned, still to be taken in (`TakeIn`, with its bar, once the window is up); without,
// it is taken in now, a line a file on the output, left as it is when it cannot be. "" when nothing
// is left to take in.
std::string take_in_basedir(BaseGame* base, const std::string& root, bool later);

// The console's `import <folder or disc image>`: sets `*ask`, which the window takes in and opens.
// With `music`, its tracks read from the game playing's folder (`*basedir`); with `remap` true
// (Quake II's `ogg_remap_tracks`), the mission pack in `*game` asks the digital release's numbers.
void add_take_in_commands(ge::console::Console& con, std::string* ask, const std::string* basedir,
                          ge::audio::MusicConsole* music = nullptr,
                          const std::string* game = nullptr, const bool* remap = nullptr);

// A level started: the track its world's `sounds` names (`entities`, the map's entity text) loops.
void level_music(ge::audio::MusicConsole& music, std::string_view entities);
// A recording's or a joined Quake II server's music: the track its server names, once named (0
// stops it); `play_level_music` leaves a track already playing alone.
void demo_music(ge::audio::MusicConsole& music, const DemoState& s);

// The folder or image in `*ask` taken in, `*ask` set to the copy; `print` gets each step and the
// result, `share` the bar's words and share (`take_in`). False with the refusal in `*error`
// (printed too).
bool take_in_ask(std::string* ask, const std::string& root, std::string* error,
                 const std::function<void(const std::string&)>& print,
                 const std::function<void(const std::string&, double)>& share = {});

// A take-in on its own thread, the window's frame asking how far it is.
class TakeIn {
 public:
  TakeIn() = default;
  TakeIn(const TakeIn&) = delete;
  TakeIn& operator=(const TakeIn&) = delete;
  ~TakeIn();  // waits for a copy running: a copy is never left half written by the window closing
  // `*from` taken into `root` (`take_in_ask`'s work) and cleared, started when none is running;
  // false when one is or `*from` is empty. `from_menu`: the menu was open when it was asked, kept
  // for the window's start once done.
  bool start(std::string* from, const std::string& root, bool from_menu);
  bool running() const { return thread_.joinable(); }
  // The window's frame: the lines said since the last call into `con`; once the copy has ended,
  // true with the folder to open in `*folder` ("" with the refusal in `*error`), and nothing runs.
  bool finished(ge::console::Console& con, std::string* folder, std::string* error);
  // The copy's words and share on the console's foot, which is held down while it runs.
  void show(ge::console::ConsoleView& view);
  bool from_menu() const { return from_menu_; }

 private:
  std::thread thread_;
  mutable std::mutex mu_;
  std::vector<std::string> lines_;
  std::string what_, folder_, error_;
  double share_ = -1;
  bool done_ = false, ok_ = false, from_menu_ = false, held_ = false;
};

}  // namespace shooters_edge::client

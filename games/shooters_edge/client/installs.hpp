// THE CLASSIC INSTALLS AROUND THE GAME: the classic game the window was started inside, and the
// installs in the folder it was placed in, so a player who drops the game into an install (or
// beside several) plays them with no folder to pick.
//
// - **Started inside one** (`install_around`): the folder the game runs in, or the folder its
//   executable was placed in, is an install the content profiles know, or lies inside one (its base
//   folder, its multiplayer folder, the client's own folder beside them: up to `kInstallDepth`
//   folders up). That install is played at once, ahead of the game loaded last.
// - **Placed above several** (`installs_below`): the folders right under it named like a classic
//   install, in any case (`kInstallNames`, in that order), then any other folder under it that is
//   one; each checked as an install by its own folders (client/game_folder.hpp's `install_of`), so
//   a folder of either game is found by what it holds, not by its name.
// - **Which release** (`release_of`): the first numbered pack and the full game's pack told by
//   their sizes (`kKnownReleasePacks`): the first game's shareware or registered packs, the
//   sequel's demo or full game; a pack of another size is the player's own copy, taken as a full
//   game. Sizes, not digests: a start reads no pack.
// - **Which is played** (`pick_install`): of the installs found, the game loaded last when it is
//   one of them, else the first full game, else the first.
//
// The console's `games` lists the installs found and `games <n>` loads one (client/main.cpp).
//
// ge:summary The classic installs around the game: started inside one, placed above several
// ge:keywords installs install-detection started-inside placed-above parent-folder nquake
// ge:keywords install_around installs_below release_of pick_install known-release shareware
// ge:keywords registered demo full-game install-names games-command app-folder
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_installs_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/profile.hpp"

namespace shooters_edge::client {

// How many folders up from where the game runs an install is still looked for.
inline constexpr int kInstallDepth = 4;

// The names a classic install's folder goes by, matched in any case, in the order they are tried.
inline constexpr std::array<std::string_view, 9> kInstallNames = {
    "quake", "qw", "quakeworld", "nquake", "quake2", "quake 2", "nquake2", "q2", "quake ii"};

// A numbered pack whose size tells a release.
struct KnownReleasePack {
  std::string_view profile;  // the content profile's id
  std::string_view pack;     // its file name, matched in any case
  uint64_t size;             // bytes
  std::string_view release;  // in a player's words
  bool full;                 // the whole game, not a sample of it
};

inline constexpr KnownReleasePack kKnownReleasePacks[] = {
    {"classic", "pak0.pak", 18689235, "shareware", false},
    {"classic", "pak1.pak", 34257856, "registered", true},
    {"sequel", "pak0.pak", 49951322, "demo", false},
    {"sequel", "pak0.pak", 183997730, "full game", true},
};

// A classic install found.
struct FoundInstall {
  const ge::classicenhance::Profile* content = nullptr;
  std::string basedir;  // the install's folder
  std::string release;  // "shareware", "registered", "demo", "full game", or "your own copy"
  bool full = false;    // the whole game
};

// The release the install at `basedir` holds, played as `content`, into `out`.
void release_of(const ge::classicenhance::Profile& content, const std::string& basedir,
                FoundInstall& out);

// The install `folder` is or lies inside (up to kInstallDepth folders up): true with it.
bool install_around(const std::string& folder, FoundInstall& out);

// The installs right under `parent`: the folders named in kInstallNames first, in that order, then
// the others, by name. None when `parent` is a file system's root.
std::vector<FoundInstall> installs_below(const std::string& parent);

// The folder the game was placed in, from its executable's folder: the folder holding the app
// bundle when it runs from one (`<folder>/Game.app/Contents/MacOS`), else the executable's own.
std::string placed_folder(const std::string& exe_dir);

// Which of `found` is played: the one at `kept` (the game loaded last), else the first full game,
// else the first; -1 when `found` is empty.
int pick_install(const std::vector<FoundInstall>& found, const std::string& kept);

// One line a found install, as `games` prints it: "1  the classic game (registered)  <folder>".
std::string install_line(const FoundInstall& f, int n);

}  // namespace shooters_edge::client

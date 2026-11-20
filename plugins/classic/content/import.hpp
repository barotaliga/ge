// A CLASSIC GAME TAKEN IN: the files a game needs COPIED out of wherever the player points (an
// installed game's folder, a store's install, a mounted disc or USB stick, or a disc image) into
// the game's own data folder, which the game then plays from; the source is never played from,
// written to or needed again.
//
//     <data>/<profile id>/<base folder>/pak0.pak ...   (e.g. data/classic/id1/pak0.pak, pak1.pak)
//     <data>/<profile id>/<multiplayer folder>/...      (when the source has one)
//     <data>/<profile id>/music/track02.flac ...        (soundtrack.hpp: one layout for every
//     source)
//
// **Where it looks** (`open_tree`): a folder is walked (six levels deep); a disc image (`.iso`, a
// `.cue` with its `.bin`, a `.bin` or `.img` alone: engine/vfs/disc.hpp) is read in place, its
// audio tracks kept as tracks. An installer's LHA archive (`resource.1` and the like, as
// Quake's disc ships the game) is opened and its files joined to the tree under the archive's
// folder.
//
// **Which game** (`find_game`): the first content profile whose base folder with its first pack
// (or the content zip standing in for it) is in the tree, at any depth (a disc keeps it under an
// install folder); the shallowest wins.
//
// **What is copied**: the base folder's and the multiplayer folder's archives (`*.pak`, `*.pk3`,
// `*.pkz`) and the loose files under the folders a game reads (maps, sound, music, progs, models,
// gfx, pics, textures, env, sprites, players, skins); never programs, libraries, saves, configs or
// demos. A file already there with the same size and SHA-256 is left alone; one that differs is
// replaced (each written through a `.part` file).
//
// **The music** (`import_game`'s `music`): the disc image's audio tracks (disc track N is track N);
// music files found anywhere in the tree (any folder named `music`, `cdtracks` or `soundtrack`, or
// a pack's `music/` entries), numbered by `track_numbers`; each kept by the soundtrack store.
//
// **A store's installs** (`store_folders`): the stores' library folders on this machine (each
// store's default `steamapps/common` and the libraries `libraryfolders.vdf` adds), and the mounted
// volumes (`/Volumes` on a Mac, `/media/<user>` and `/run/media/<user>` on Linux, drive letters on
// Windows), the folders the games page offers to take a game in from.
//
// ge:summary Copies a classic game out of a folder, mounted disc or disc image into its data folder
// ge:keywords import copy install data-folder iso cue bin disc image mounted volume usb cd steam
// ge:keywords gog store-install libraryfolders find_game open_tree import_game store_folders
// ge:keywords resource lha installer soundtrack music
// ge:skill classic vfs shooter
// ge:test tests/classic_content_import_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "classic/content/soundtrack.hpp"
#include "classicenhance/profile.hpp"
#include "vfs/disc.hpp"

namespace ge::classic::content {

// One file of a source, read on demand.
struct TreeFile {
  std::string path;  // '/' separated from the source's top
  uint64_t size = 0;
  std::function<bool(std::vector<uint8_t>*, std::string*)> read;
};

struct Tree {
  std::string source;  // what the player pointed at
  std::vector<TreeFile> files;
  std::shared_ptr<vfs::Disc> disc;  // a disc image's, for its audio tracks; null otherwise
};

// The source at `path` (a folder or a disc image) listed; false with the reason and the next step.
bool open_tree(const std::string& path, Tree* out, std::string* error);

// The game the tree holds: its content profile and the folder above its base folder ("" the top);
// null when none.
const classicenhance::Profile* find_game(const Tree& tree, std::string* prefix);

struct ImportReport {
  const classicenhance::Profile* game = nullptr;
  std::string game_dir;             // <data>/<profile id>: the install the game opens
  std::vector<std::string> copied;  // written, relative to game_dir
  std::vector<std::string> same;    // already there, unchanged
  std::vector<StoredTrack> tracks;  // the music kept (soundtrack.hpp)
  std::vector<std::string> notes;   // what was found and left (a track that did not decode ...)
};

struct ImportOptions {
  bool music = true;
  // Called before each file and track ("copying id1/pak0.pak"), for a progress line.
  std::function<void(const std::string&)> progress;
};

// The game in the source at `from` copied into `<data_root>/<profile id>/`; false with what failed
// and the next step (no game found, a folder that cannot be written).
bool import_game(const std::string& from, const std::string& data_root, ImportReport* out,
                 std::string* error, const ImportOptions& opts = {});

// The folders on this machine a game may be taken in from: the stores' libraries and the mounted
// volumes (those that exist).
std::vector<std::string> store_folders();

}  // namespace ge::classic::content

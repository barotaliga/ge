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
// **Release archives** and **mission packs**: below, at `open_tree` and `find_packs`.
//
// **What is copied**: the base folder's and the multiplayer folder's archives (`*.pak`, `*.pk3`,
// `*.pkz`) and the loose files under the folders a game reads (maps, sound, music, progs, models,
// gfx, pics, textures, env, sprites, players, skins, and video: Quake II's movies); never
// programs, libraries, saves, configs or demos. A release that prints its words by key keeps their
// table (`localization/loc_english.txt`) in its own archives beside the base folder (`*.kpf`):
// that one file is taken out and kept loose in the base folder. A file already there with the same
// size and SHA-256 is left alone; one that differs is replaced (each written through a `.part`
// file).
//
// **Taken in once** (`<data>/<profile id>/taken.txt`, or `taken-<pack folders>.txt` for a source
// of packs alone, beside it): a finished take-in writes the source's
// fingerprint (where it is, and every file's path, size and last write time; a disc image's own
// size and time) and each file it put in place with its size. Pointing at the same source again
// (a second launch with `-basedir`) only lists it: when the fingerprint is the same and every file
// is still there at its size, nothing is read, hashed, copied or decoded
// (`ImportReport::unchanged`). A source that changed, or a copy that lost a file, is taken in again
// as above.
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
// ge:keywords resource lha installer soundtrack music kpf localization loc_english mission-pack
// ge:keywords find_packs add-on expansion release-zip 7z mdf patch movies video
// ge:skill classic vfs shooter
// ge:test tests/classic_content_import_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "classic/content/soundtrack.hpp"
#include "classicenhance/profile.hpp"
#include "vfs/disc.hpp"

namespace ge::classic::content {

// One file of a source, read on demand.
struct TreeFile {
  std::string path;  // '/' separated from the source's top
  uint64_t size = 0;
  int64_t stamp = 0;  // its last write time on disk (the clock's ticks); 0 inside an image
  std::function<bool(std::vector<uint8_t>*, std::string*)> read;
};

struct Tree {
  std::string source;       // what the player pointed at
  std::string image_stamp;  // a disc image's size and last write time; empty for a folder
  std::vector<TreeFile> files;
  std::shared_ptr<vfs::Disc> disc;  // a disc image's, for its audio tracks; null otherwise
  std::string unpacked;  // the disc image a 7z held, unpacked on disk for the take-in; "" none
};

// The source at `path` (a folder, a disc image, a release zip, or a 7z of a disc image, which is
// unpacked into the folder `scratch`) listed; false with the reason and the next step.
bool open_tree(const std::string& path, Tree* out, std::string* error,
               const std::string& scratch = "");

// The game the tree holds: its content profile and the folder above its base folder ("" the top);
// null when none.
const classicenhance::Profile* find_game(const Tree& tree, std::string* prefix);

// Which mission pack, of which game, a first pack of `size` bytes with SHA-256 `sha256` is; null
// when none (`classicenhance::known_pack` by default).
using PackIdentifier = std::function<const classicenhance::MissionPack*(
    uint64_t size, std::string_view sha256, const classicenhance::Profile** game)>;

// A mission pack found in a tree.
struct FoundPack {
  const classicenhance::Profile* game = nullptr;
  const classicenhance::MissionPack* pack = nullptr;
  std::string root;       // its own folder, lower case with '/' at the end ("" the tree's top)
  std::string pack_file;  // its first pack's path when it lies under another folder's name
  std::string loose;      // another folder named after it whose content folders go with it
  std::string patch;      // a `patch/<base folder>/` folder whose content folders go to the base
  bool named = false;     // found by its folder's name (`root`), not by its digest alone
};

// **Release archives**: a release `.zip` (an install zipped, as an archive keeps a game) is read
// whole into memory and its entries taken as the tree; a `.7z` holding one disc image (`.mdf`,
// `.iso`, `.img` or `.bin`) has it unpacked into `<data>/.unpacking/`, read as a disc image above,
// and removed when the take-in ends (a 7z is unpacked again on each take-in, unchanged or not).
//
// **Mission packs** (`find_packs`): a game's official add-ons (classicenhance/profile.hpp's
// `MissionPack`) are found beside its base folder or alone, and copied into
// `<data>/<profile id>/<pack folder>/` beside the base folder, where the game plays them over it:
// - by name: a `pak0.pak` in a folder named after one of the game's packs (`hipnotic` beside
//   `id1`), its archives and content folders copied like the base folder's; a name two games' packs
//   share is told by the base folder beside it, then by the pack's SHA-256, then by the one game
//   with a pack of that name; the folder pointed at is the pack's own when the tree's top holds it;
// - by digest: a `pak0.pak` in any other folder (a disc's `Data/all/`) whose size and SHA-256 are
//   a known pack's, with the content folders of the shallowest folder named after the pack (its
//   movies, `Data/max/<pack>/video`) and of a `patch/<base folder>/` folder (players' models of
//   the pack's guns), the latter into the base folder.
// A source of packs alone (a pack's disc) is taken in the same way and says that the base game
// must be taken in too (`ImportReport::base` false); its disc's audio tracks are that pack's
// music, kept in `<data>/<profile id>/<pack folder>/music/`. Only the base game's own source
// reads its music files and its packs' music.
//
// The mission packs in the tree, of `game` when it is not null (as above).
std::vector<FoundPack> find_packs(const Tree& tree, const classicenhance::Profile* game,
                                  const PackIdentifier& identify);

struct ImportReport {
  const classicenhance::Profile* game = nullptr;
  std::string game_dir;             // <data>/<profile id>: the install the game opens
  std::vector<std::string> copied;  // written, relative to game_dir
  std::vector<std::string> same;    // already there, unchanged
  std::vector<StoredTrack> tracks;  // the music kept (soundtrack.hpp)
  std::vector<std::string> notes;   // what was found and left (a track that did not decode ...)
  std::vector<const classicenhance::MissionPack*> packs;  // the mission packs taken in with it
  bool base = true;          // the source held the base game itself, not only its packs
  std::string music_folder;  // the tracks' folder under game_dir: "" the game's, else a pack's
  // The source was taken in before and has not changed since (`taken.txt` below): nothing was
  // read, copied or decoded, and `copied`, `same` and `tracks` are empty.
  bool unchanged = false;
};

struct ImportOptions {
  bool music = true;
  // Called before each file and track ("copying id1/pak0.pak"), for a progress line.
  std::function<void(const std::string&)> progress;
  // Tells a first pack under another folder's name by its digest (a test's fixtures); empty: the
  // content profiles' own mission packs.
  PackIdentifier identify;
};

// The game in the source at `from` copied into `<data_root>/<profile id>/`; false with what failed
// and the next step (no game found, a folder that cannot be written).
bool import_game(const std::string& from, const std::string& data_root, ImportReport* out,
                 std::string* error, const ImportOptions& opts = {});

// The folders on this machine a game may be taken in from: the stores' libraries and the mounted
// volumes (those that exist).
std::vector<std::string> store_folders();

// A mission pack found on this machine and not yet in the data folder: offered to the player.
struct Offer {
  std::string source;  // what to take in: the install holding it, its own folder, or the volume
  const classicenhance::Profile* game = nullptr;
  const classicenhance::MissionPack* pack = nullptr;
};

// THE PACKS OFFERED WITHOUT BEING ASKED (`find_offers`): under each of `roots` (`store_folders`),
// four folders deep and at most `kOfferFolders` folders in all, a folder named after a pack
// holding its first pack (offered with the install above it when the base folder is beside it,
// else alone), or a first pack of a known pack's size under any other name (a pack's disc, offered
// as the folder under the root that holds it; its digest is read only when taken in). A pack
// already in `<data_root>/<id>/<pack folder>/` is not offered; each pack once, the first found.
inline constexpr int kOfferFolders = 4000;
std::vector<Offer> find_offers(const std::vector<std::string>& roots, const std::string& data_root);

}  // namespace ge::classic::content

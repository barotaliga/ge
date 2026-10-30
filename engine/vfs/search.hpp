// THE SEARCH PATH: every file a game can open, from the directories and archives it mounted, each
// name answered by the one source that wins it. A game mounts its BASE game directory, then a
// game directory per mod over it; a file in a later source hides the same name in every earlier
// one, so a mod replaces a file by shipping one of the same name.
//
// A GAME DIRECTORY (`add_game`: `<root>/<game>`) mounts, lowest first:
// 1. its packs `pak<n>.pak` in number order (pak0, pak1, ... pak10);
// 2. its zips `*.pk3` and unpacked zips `*.pk3dir` (a directory standing in for one while a mod is
//    worked on, its files named from inside it), together in name order;
// 3. its loose files, every file under the directory but those archives and what is inside a
//    `.pk3dir`, which win over all of them.
// Names (directory entries, archive entries, the files' own) are compared as the archives' systems
// did: ASCII case folded and '\\' read as '/' (`same_name`). A directory is listed when it is
// mounted and read from disk when a file of it is read; an archive is read whole at mount and
// checked (pak.hpp, zip.hpp), so a damaged one is refused there, naming it.
//
// THE LOOKUP (`find`) is a hash table of every folded name built at mount: a name is hashed and
// compared as it is folded, so a lookup never allocates. `read` hands a file's bytes (a pack's
// and a stored zip entry's by copy, a deflated one unpacked and checked, a loose file from disk);
// `view` hands a pack's or a stored zip entry's bytes without a copy, and nothing for the others.
// `list` is every name and its winner, in name order (what `ge pak ls` prints).
//
// Mounting is deterministic: the same files give the same table on every platform.
//
// ge:summary The search path: game directories, packs, pk3s and pk3dirs, the winning file, lookup
// ge:keywords vfs search-path gamedir game-directory mount pak pk3 pk3dir zip loose-files override
// ge:keywords find lookup hash no-allocation read view list priority add_game
// ge:skill vfs
// ge:test tests/vfs_search_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ge::vfs {

enum class SourceKind : uint8_t { directory, pack, zip };

// One mounted source: a directory (a game's loose files, or a .pk3dir), a pack or a zip.
struct Source {
  SourceKind kind = SourceKind::directory;
  std::string path;  // on disk (the directory or the archive), or the name it was mounted under
  std::string game;  // the game directory it belongs to
  std::vector<uint8_t> bytes;        // a pack's or a zip's whole file
  Pak pak;                           // a pack's directory
  Zip zip;                           // a zip's directory
  std::vector<std::string> files;    // a directory's files, '/' separated, in name order
  std::vector<uint64_t> file_sizes;  // a directory's files' sizes
};

// A file as the search path found it: which source and which of its entries.
struct FileRef {
  uint32_t source = 0;
  uint32_t entry = 0;
  uint64_t size = 0;
};

struct SearchPath {
  std::vector<Source> sources;  // in mount order: the last mounted wins
  // The index: open addressing over a power-of-two table, each slot a folded name in `names`.
  struct Slot {
    uint64_t hash = 0;
    uint32_t name_at = 0, name_len = 0;
    FileRef ref;
    bool used = false;
  };
  std::vector<Slot> slots;
  std::string names;
  size_t files = 0;
};

// Mounts the game directory `<root>/<game>`; false with the reason (naming the file) in `*error`.
bool add_game(SearchPath* sp, const std::string& root, const std::string& game, std::string* error);
// Mounts one archive from memory (a pack or a zip, told by its first bytes) under `name`.
bool add_archive(SearchPath* sp, const std::string& name, std::vector<uint8_t> bytes,
                 const std::string& game, std::string* error);
// Mounts the files of a directory on disk (every file under it) as one source.
bool add_directory(SearchPath* sp, const std::string& dir, const std::string& game,
                   std::string* error);

// The file that wins `path`, or nullptr. Never allocates.
const FileRef* find(const SearchPath& sp, std::string_view path);
// The file's bytes; false with the reason.
bool read(const SearchPath& sp, const FileRef& file, std::vector<uint8_t>* out, std::string* error);
// The file's bytes without a copy (a pack's file, a stored zip entry); empty for the others.
std::span<const uint8_t> view(const SearchPath& sp, const FileRef& file);
// The name the file is stored under in its source (its case kept).
std::string_view stored_name(const SearchPath& sp, const FileRef& file);

struct Listed {
  std::string_view name;  // folded
  FileRef ref;
};
// Every file and its winner, in name order.
std::vector<Listed> list(const SearchPath& sp);

}  // namespace ge::vfs

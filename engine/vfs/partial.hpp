// A PARTIAL ARCHIVE: files fetched one at a time from a server put back into the archive they
// came from. A file a server sends out of its `pak1.pak` is kept in `pak1.partial.pak` in the same
// game folder; once every entry of the server's archive is held, the archive is put together and
// the partial renamed to `pak1.pak`. A zip (`maps.pk3`) the same way, as `maps.partial.pk3`.
//
// THE ORIGIN (`ArchiveOrigin`) is what the server tells of the archive a file came from: its name,
// its game folder, how many entries it has and this file's, the archive's size, and for a pack
// the file's offset and the directory's. `archive_origin` reads it off the search path that found
// the file; a loose file has none (and is kept as a loose file at its place).
//
// THE PARTIAL is itself an archive of the same kind, so the search path mounts it like any other
// and every file in it is used at once: the files held, then an entry `.partial` (text: the
// origin's archive and every held file's entry, offset and path). A file fetched from an archive
// that differs from the one the partial was started on (another count, size or directory) starts
// the partial again. A pack's next file is written in place at its end (only its directory and the
// `.partial` entry are read, so a nearly whole partial costs no more than a new one); a partial
// started, a file fetched again, a zip's, and the archive put together are written whole to a
// temporary file renamed over the old one. A partial cut short by a crash reads as damaged and
// starts again: every file in it can be fetched again.
//
// THE WHOLE ARCHIVE is put together in the server's order: a pack with every file at its offset
// and the directory where it was, so it is the server's archive byte for byte when the server's
// pack holds nothing but its files and directory (leftover bytes between files, or after a name's
// end, which no file carries, come back as zeros; offsets that overlap are laid out in entry
// order); a zip of stored entries in entry order. An archive of that name already in the folder is
// never overwritten: the file is refused and the caller keeps it loose.
//
// ge:summary Files fetched from a server kept in the archive they came from, renamed when whole
// ge:keywords vfs partial-pack partial download origin pak pk3 zip assemble rename whole-archive
// ge:keywords archive_origin keep_in_partial partial_name downloads-into-paks
// ge:skill vfs
// ge:test tests/vfs_partial_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "vfs/search.hpp"

namespace ge::vfs {

struct ArchiveOrigin {
  std::string archive;      // its file name in the game folder: "pak1.pak", "maps.pk3"
  std::string folder;       // the game folder it is in on the server: "id1", a mod's
  uint32_t count = 0;       // its entries
  uint32_t index = 0;       // this file's entry
  uint64_t size = 0;        // the archive's bytes
  uint32_t offset = 0;      // a pack's: this file's offset in it
  uint32_t dir_offset = 0;  // a pack's: its directory's offset
};

// The archive the search path found `file` in; false for a loose file (a directory's).
bool archive_origin(const SearchPath& sp, const FileRef& file, ArchiveOrigin* out);

// The partial's name for an archive: "pak1.pak" -> "pak1.partial.pak"; "" for a name that is not
// a plain file name with an extension.
std::string partial_name(std::string_view archive);

struct Kept {
  std::string on_disk;  // the file written: the partial, or the whole archive when it completed
  bool whole = false;   // every entry held: the archive was put together and the partial removed
  uint32_t held = 0, count = 0;  // the entries the partial holds now, of the archive's
};

// Keeps `bytes`, fetched as `path` out of the archive `origin` names, in that archive's partial in
// the folder `dir` on disk; puts the archive together when it is the last entry missing. False with
// the reason (an origin that does not hold together, a name a pack cannot store, the whole archive
// already there, a write that failed): the caller keeps the file loose then.
bool keep_in_partial(const std::string& dir, const ArchiveOrigin& origin, std::string_view path,
                     std::span<const uint8_t> bytes, Kept* out, std::string* error);

}  // namespace ge::vfs

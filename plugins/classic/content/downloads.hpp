// A FILE FETCHED FROM A SERVER, KEPT WHERE IT BELONGS, for every classic game a client joins
// servers of. A file the server took out of one of its archives (it said which:
// engine/vfs/partial.hpp's ArchiveOrigin) goes back into that archive: into
// `<basedir>/<the archive's game folder>/<name>.partial.<ext>` (`pak1.partial.pak`), renamed to the
// archive's own name (`pak1.pak`) once every file of it is held. A file with no archive (a loose
// file on the server, or a server that does not say) is written at its place in the game's folder
// on disk, `<game folder>/<path>`: the base folder under the base directory
// (`data/<game>/id1/skins/red.pcx`) or the server's game folder when it plays another. When the
// archive cannot take it (one by that name already whole, a path a pack cannot store) it is kept
// loose too, so a fetched file is never lost.
//
// THE SERVER'S HALF (`served_origin`): the archive a file it serves comes out of, as found on its
// search path; none when the game folder's loose file on disk wins over the archives. Every game's
// server answers it the same way, so a game wires it in one line.
//
// What was kept is mounted over the search path at once when it belongs to the folders played, so
// the file is used without a restart: a loose file as itself, a file put in a partial as an archive
// of that file alone (the partial itself is mounted with the folder at the next start).
//
// A DOWNLOAD CUT SHORT (`HeldDownloads`): what came of a file so far is kept as `<path>.tmp` at its
// place in the game's folder, a piece added as it comes, so the next time it is asked for the
// client asks for the rest from that length; once the file is whole and kept, the `.tmp` goes.
//
// ge:summary Server downloads kept in the archive they came from, or loose at their place on disk
// ge:keywords download keep_download partial-pack partial origin game-folder data-folder loose
// ge:keywords downloads-into-paks fetched-file skins served_origin download-resume tmp-file
// ge:keywords held_download resume-download
// ge:skill classic vfs
// ge:test tests/classic_content_downloads_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "classic/content/files.hpp"
#include "vfs/partial.hpp"

namespace ge::classic::content {

struct KeptDownload {
  std::string on_disk;      // the file written: the loose file, the partial or the whole archive
  bool in_archive = false;  // kept in a partial of its archive (or completed it)
  bool whole = false;       // that archive is whole now
  uint32_t held = 0, count = 0;  // the archive's files held, of all of them
  bool mounted = false;          // mounted over `files` at once
  std::string why;               // why it was kept loose when the server named an archive; "" else
};

// Keeps the file `path` fetched from a server playing the game folder `gamedir` ("" or the
// client's own folder: `files.folder`). `ours`: that folder is the one the client plays (its files
// mounted in `files`); `base_folder` the content row's base folder name (`id1`). False with the
// reason when nothing could be written.
bool keep_download(SearchPath& files, const std::string& basedir, const std::string& base_folder,
                   const std::string& gamedir, bool ours, const std::string& path,
                   const std::vector<uint8_t>& bytes, const ge::vfs::ArchiveOrigin* origin,
                   KeptDownload* out, std::string& error);

// The archive the server's `files` found `path` in, for the client to keep it there; false for a
// loose file (one on disk in the game folder wins when `files.loose_first`) or a file not found.
bool served_origin(const SearchPath& files, const std::string& path, ge::vfs::ArchiveOrigin* out);
// The file `path` as the deflate stream it is stored as in a `.pkz` (a zip the game reads), for a
// client that inflates a download itself; false for any other file (loose, a pack's, stored).
bool served_deflated(const SearchPath& files, const std::string& path, std::vector<uint8_t>* out);

// A download cut short, held to be resumed: `read` what is held of `path` (empty: none),
// `add` a piece to it (`fresh`: begun again, anything held dropped first), `drop` it once the file
// is whole. Unset: nothing is held, every file fetched from its start.
struct HeldDownloads {
  std::function<std::vector<uint8_t>(const std::string& path)> read;
  std::function<void(const std::string& path, std::span<const uint8_t> bytes, bool fresh)> add;
  std::function<void(const std::string& path)> drop;
};

// The held downloads as `<path>.tmp` files in the game folder `dir()` names when each is called
// (the client's own folder, where `keep_download` keeps a loose file); a path naming no place in it
// is never held, and nothing is while `dir()` is empty.
HeldDownloads held_in_folder(std::function<std::string()> dir);

}  // namespace ge::classic::content

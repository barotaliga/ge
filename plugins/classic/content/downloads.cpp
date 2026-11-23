// Server downloads kept in the archive they came from, or loose at their place (downloads.hpp).
//
// ge:summary keep_download: a fetched file into its archive's partial or loose, then mounted
// ge:keywords download keep_download partial-pack loose mount add_archive add_file
// ge:skill classic vfs
// ge:test tests/classic_content_downloads_test.cpp
#include "classic/content/downloads.hpp"

#include <filesystem>
#include <fstream>
#include <utility>

#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ge::classic::content {
namespace {

namespace fs = std::filesystem;

bool plain_folder(const std::string& n) {
  return !n.empty() && n[0] != '.' && n.find_first_of("/\\:") == std::string::npos;
}

}  // namespace

bool keep_download(SearchPath& files, const std::string& basedir, const std::string& base_folder,
                   const std::string& gamedir, bool ours, const std::string& path,
                   const std::vector<uint8_t>& bytes, const ge::vfs::ArchiveOrigin* origin,
                   KeptDownload* out, std::string& error) {
  *out = KeptDownload{};
  if (path.empty() || path[0] == '/' || path.find("..") != std::string::npos ||
      path.find_first_of("\\:") != std::string::npos) {
    error = "the file \"" + path + "\" names no place in the game's folder";
    return false;
  }
  // Back into the archive the server took it out of, in that archive's game folder.
  if (origin != nullptr && !basedir.empty() && plain_folder(origin->folder)) {
    fs::path dir = on_disk_any_case(basedir, origin->folder);
    if (dir.empty()) dir = fs::path(basedir) / origin->folder;
    ge::vfs::Kept kept;
    if (ge::vfs::keep_in_partial(dir.string(), *origin, path, bytes, &kept, &out->why)) {
      out->on_disk = kept.on_disk;
      out->in_archive = true;
      out->whole = kept.whole;
      out->held = kept.held;
      out->count = kept.count;
      // Used at once when the archive's folder is one played: the file as an archive of its own.
      const bool played =
          ge::vfs::same_name(origin->folder, base_folder) ||
          (ours && !files.folder.empty() &&
           ge::vfs::same_name(fs::path(files.folder).filename().string(), origin->folder));
      std::vector<uint8_t> one;
      std::string why;
      if (played && ge::vfs::write_pak({{path, bytes}}, &one, &why))
        out->mounted =
            ge::vfs::add_archive(&files.vfs, kept.on_disk, std::move(one), origin->folder, &why);
      return true;
    }
  }
  // Loose, at its place in the game's folder.
  const fs::path dir = ours ? fs::path(files.folder) : fs::path(basedir) / gamedir;
  if (dir.empty() || (!ours && !plain_folder(gamedir))) {
    error = "no game folder to keep " + path + " in";
    return false;
  }
  std::error_code ec;
  fs::create_directories((dir / path).parent_path(), ec);
  std::ofstream file(dir / path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  file.close();
  if (!file) {
    error = "could not write " + (dir / path).string() + ": check the folder is writable";
    return false;
  }
  out->on_disk = (dir / path).string();
  std::string why;
  if (ours)
    out->mounted = ge::vfs::add_file(&files.vfs, dir.string(), path, dir.filename().string(), &why);
  if (ours && !out->mounted) error = why;
  return true;
}

bool served_origin(const SearchPath& files, const std::string& path, ge::vfs::ArchiveOrigin* out) {
  if (files.loose_first && !files.folder.empty() && !on_disk_any_case(files.folder, path).empty())
    return false;
  const ge::vfs::FileRef* f = ge::vfs::find(files.vfs, path);
  return f != nullptr && ge::vfs::archive_origin(files.vfs, *f, out);
}

bool served_deflated(const SearchPath& files, const std::string& path, std::vector<uint8_t>* out) {
  if (files.loose_first && !files.folder.empty() && !on_disk_any_case(files.folder, path).empty())
    return false;
  const ge::vfs::FileRef* f = ge::vfs::find(files.vfs, path);
  std::span<const uint8_t> packed;
  if (f == nullptr || !ge::vfs::deflated(files.vfs, *f, &packed)) return false;
  const std::string& archive = files.vfs.sources[f->source].path;
  if (archive.size() < 4) return false;
  std::string end = archive.substr(archive.size() - 4);
  for (char& ch : end) ch = static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
  if (end != ".pkz") return false;
  out->assign(packed.begin(), packed.end());
  return true;
}

}  // namespace ge::classic::content

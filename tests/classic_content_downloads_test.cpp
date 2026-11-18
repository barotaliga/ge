// A file fetched from a server kept where it belongs (plugins/classic/content/downloads.hpp): one
// the server took out of its pak1.pak goes into `<basedir>/<its folder>/pak1.partial.pak` and is
// found on the search path at once; with the archive's last file the partial becomes pak1.pak;
// a file with no archive lands loose at its place in the game's folder and is found at once; one
// fetched from a server playing another folder lands in that folder; one whose archive is already
// whole is kept loose rather than lost; a path that climbs out is refused.
//
// ge:summary Tests keep_download: into the archive's partial, whole, loose at its place, mounted
// ge:keywords test download keep_download partial-pack loose data-folder mounted
// ge:skill classic vfs testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/content/downloads.hpp"
#include "vfs/pak.hpp"
#include "vfs/search.hpp"

namespace cc = ge::classic::content;
namespace vfs = ge::vfs;
namespace fs = std::filesystem;

namespace {

bool found(const cc::SearchPath& files, const std::string& path, const std::vector<uint8_t>& want) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> got = cc::find_file(files, path, store);
  return std::vector<uint8_t>(got.begin(), got.end()) == want;
}

}  // namespace

TEST(a_fetched_file_goes_into_its_archive_or_to_its_place) {
  const fs::path root = fs::temp_directory_path() / "ge_classic_downloads";
  fs::remove_all(root);
  fs::create_directories(root / "id1");
  // The server's pack: two skins.
  const std::vector<uint8_t> red(400, 1), blue(600, 2), loose(50, 3);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(vfs::write_pak({{"skins/red.pcx", red}, {"skins/blue.pcx", blue}}, &pak, &error),
            error);
  vfs::SearchPath server;
  CHECK_MSG(vfs::add_archive(&server, "pak1.pak", pak, "id1", &error), error);
  const auto origin = [&server](const std::string& path) {
    vfs::ArchiveOrigin o;
    const vfs::FileRef* f = vfs::find(server, path);
    CHECK(f != nullptr && vfs::archive_origin(server, *f, &o));
    return o;
  };
  cc::SearchPath files;
  files.folder = (root / "id1").string();
  const std::string base = root.string();

  cc::KeptDownload kept;
  vfs::ArchiveOrigin o = origin("skins/red.pcx");
  CHECK_MSG(cc::keep_download(files, base, "id1", "", true, "skins/red.pcx", red, &o, &kept, error),
            error);
  CHECK(kept.in_archive && !kept.whole && kept.held == 1 && kept.count == 2 && kept.mounted);
  CHECK(fs::exists(root / "id1" / "pak1.partial.pak"));
  CHECK(!fs::exists(root / "id1" / "skins" / "red.pcx"));
  CHECK(found(files, "skins/red.pcx", red));

  // The loose file: at its place, found at once.
  CHECK_MSG(cc::keep_download(files, base, "id1", "", true, "skins/green.pcx", loose, nullptr,
                              &kept, error),
            error);
  CHECK(!kept.in_archive && kept.mounted && fs::exists(root / "id1" / "skins" / "green.pcx"));
  CHECK(found(files, "skins/green.pcx", loose));

  // The last file of the pack: the partial is renamed to the archive, the server's byte for byte.
  o = origin("skins/blue.pcx");
  CHECK_MSG(
      cc::keep_download(files, base, "id1", "", true, "skins/blue.pcx", blue, &o, &kept, error),
      error);
  CHECK(kept.in_archive && kept.whole && kept.held == 2);
  CHECK(!fs::exists(root / "id1" / "pak1.partial.pak"));
  std::ifstream in(root / "id1" / "pak1.pak", std::ios::binary);
  CHECK(std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}) == pak);
  CHECK(found(files, "skins/blue.pcx", blue));

  // The archive is whole now: a file of it fetched again is kept loose, never lost.
  o = origin("skins/red.pcx");
  CHECK_MSG(cc::keep_download(files, base, "id1", "", true, "skins/red.pcx", red, &o, &kept, error),
            error);
  CHECK(!kept.in_archive && kept.why.find("already") != std::string::npos);
  CHECK(fs::exists(root / "id1" / "skins" / "red.pcx"));

  // A server playing another folder: a loose file lands in that folder under the base directory.
  CHECK_MSG(cc::keep_download(files, base, "id1", "mymod", false, "skins/x.pcx", loose, nullptr,
                              &kept, error),
            error);
  CHECK(!kept.mounted && fs::exists(root / "mymod" / "skins" / "x.pcx"));

  CHECK(!cc::keep_download(files, base, "id1", "", true, "../escape.txt", loose, nullptr, &kept,
                           error));
  CHECK(!fs::exists(root / "escape.txt"));
  fs::remove_all(root);
}

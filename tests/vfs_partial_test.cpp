// The partial archive (engine/vfs/partial.hpp): files fetched one by one out of a server's pack,
// in any order, are held in `pak1.partial.pak` (a pack the search path mounts and reads them from)
// and, with the last one, put together into `pak1.pak` byte for byte as the server's; a zip the
// same way with the same files; a partial started on another archive of the same name starts
// again; an origin that does not hold together, a path a pack cannot store and an archive already
// there are refused.
//
// ge:summary Tests: files kept in a partial of their archive and the whole archive put together
// ge:keywords test vfs partial-pack partial download origin assemble byte-identical zip refuse
// ge:skill vfs testing
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "vfs/pak.hpp"
#include "vfs/partial.hpp"
#include "vfs/search.hpp"
#include "vfs/zip.hpp"

namespace vfs = ge::vfs;
namespace fs = std::filesystem;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

const Files kFiles = {{"skins/red.pcx", std::vector<uint8_t>(300, 1)},
                      {"progs/player.mdl", std::vector<uint8_t>(5000, 2)},
                      {"maps/start.bsp", std::vector<uint8_t>(777, 3)},
                      {"sound/a.wav", {}}};

fs::path fresh(const char* name) {
  const fs::path d = fs::temp_directory_path() / name;
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}

std::vector<uint8_t> read_whole(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// The server's side: the archive mounted, each file's origin as the search path tells it.
vfs::ArchiveOrigin origin_of(const vfs::SearchPath& sp, const std::string& path) {
  vfs::ArchiveOrigin o;
  const vfs::FileRef* f = vfs::find(sp, path);
  CHECK(f != nullptr);
  if (f) CHECK(vfs::archive_origin(sp, *f, &o));
  return o;
}

}  // namespace

TEST(a_pack_fetched_file_by_file_is_put_together_byte_for_byte) {
  std::vector<uint8_t> server;
  std::string error;
  CHECK_MSG(vfs::write_pak(kFiles, &server, &error), error);
  vfs::SearchPath sp;
  CHECK_MSG(vfs::add_archive(&sp, "pak1.pak", server, "base", &error), error);
  const fs::path root = fresh("ge_vfs_partial_pack"), dir = root / "base";
  CHECK(vfs::partial_name("pak1.pak") == "pak1.partial.pak");
  const int order[] = {2, 0, 3, 1};
  for (int n = 0; n < 4; ++n) {
    const auto& [path, bytes] = kFiles[size_t(order[n])];
    const vfs::ArchiveOrigin o = origin_of(sp, path);
    CHECK(o.archive == "pak1.pak" && o.folder == "base" && o.count == 4);
    CHECK(o.index == uint32_t(order[n]));
    vfs::Kept kept;
    CHECK_MSG(vfs::keep_in_partial(dir.string(), o, path, bytes, &kept, &error), error);
    CHECK(kept.held == uint32_t(n + 1) && kept.count == 4);
    CHECK(kept.whole == (n == 3));
    if (n == 1) {
      // Half way: the partial is a pack the game's search path mounts and reads the files from.
      vfs::SearchPath game;
      CHECK_MSG(vfs::add_game(&game, root.string(), "base", &error), error);
      for (int k : {2, 0}) {
        const vfs::FileRef* f = vfs::find(game, kFiles[size_t(k)].first);
        std::vector<uint8_t> got;
        CHECK(f != nullptr && vfs::read(game, *f, &got, &error) && got == kFiles[size_t(k)].second);
      }
      CHECK(vfs::find(game, "progs/player.mdl") == nullptr);
      CHECK(!fs::exists(dir / "pak1.pak"));
    }
  }
  CHECK(!fs::exists(dir / "pak1.partial.pak"));
  CHECK(read_whole(dir / "pak1.pak") == server);
  // Once whole, the archive is never written over: another file is refused, kept loose.
  vfs::Kept kept;
  CHECK(!vfs::keep_in_partial(dir.string(), origin_of(sp, "skins/red.pcx"), "skins/red.pcx",
                              kFiles[0].second, &kept, &error));
  CHECK(error.find("already") != std::string::npos);
  fs::remove_all(root);
}

TEST(a_zip_fetched_file_by_file_holds_the_same_files) {
  const std::vector<uint8_t> server = vfs::write_stored_zip(kFiles);
  vfs::SearchPath sp;
  std::string error;
  CHECK_MSG(vfs::add_archive(&sp, "extra.pk3", server, "mod", &error), error);
  const fs::path dir = fresh("ge_vfs_partial_zip");
  vfs::Kept kept;
  for (int k : {3, 1, 0, 2})
    CHECK_MSG(
        vfs::keep_in_partial(dir.string(), origin_of(sp, kFiles[size_t(k)].first),
                             kFiles[size_t(k)].first, kFiles[size_t(k)].second, &kept, &error),
        error);
  CHECK(kept.whole && !fs::exists(dir / "extra.partial.pk3"));
  const std::vector<uint8_t> whole = read_whole(dir / "extra.pk3");
  vfs::Zip zip;
  CHECK_MSG(vfs::read_zip(whole, &zip, &error), error);
  CHECK(zip.entries.size() == kFiles.size());
  for (size_t i = 0; i < kFiles.size() && i < zip.entries.size(); ++i) {
    std::vector<uint8_t> got;
    CHECK(zip.entries[i].name == kFiles[i].first);
    CHECK(vfs::zip_unpack(whole, zip.entries[i], &got, &error) && got == kFiles[i].second);
  }
  fs::remove_all(dir);
}

TEST(a_partial_of_another_archive_by_the_same_name_starts_again) {
  const fs::path dir = fresh("ge_vfs_partial_other");
  std::string error;
  vfs::ArchiveOrigin o{"pak1.pak", "base", 4, 0, 4096, 12, 4096 - 4 * 64};
  vfs::Kept kept;
  CHECK_MSG(
      vfs::keep_in_partial(dir.string(), o, "a.txt", std::vector<uint8_t>(10, 1), &kept, &error),
      error);
  o.index = 1;
  o.offset = 22;
  CHECK(vfs::keep_in_partial(dir.string(), o, "b.txt", std::vector<uint8_t>(10, 2), &kept, &error));
  CHECK(kept.held == 2);
  o.count = 5;  // the server's pak1.pak is another one
  o.dir_offset = 4096 - 5 * 64;
  CHECK(vfs::keep_in_partial(dir.string(), o, "b.txt", std::vector<uint8_t>(10, 2), &kept, &error));
  CHECK(kept.held == 1 && kept.count == 5);
  fs::remove_all(dir);
}

TEST(an_origin_that_does_not_hold_together_is_refused) {
  const fs::path dir = fresh("ge_vfs_partial_refused");
  std::string error;
  vfs::Kept kept;
  const std::vector<uint8_t> b(4, 9);
  CHECK(!vfs::keep_in_partial(dir.string(), {"pak1.pak", "base", 2, 2, 0, 0, 0}, "a", b, &kept,
                              &error));
  CHECK(error.find("hold together") != std::string::npos);
  CHECK(!vfs::keep_in_partial(dir.string(), {"../pak1.pak", "base", 2, 0, 0, 0, 0}, "a", b, &kept,
                              &error));
  CHECK(!vfs::keep_in_partial(dir.string(), {"pak1.pak", "base", 2, 0, 0, 0, 0}, "../a", b, &kept,
                              &error));
  CHECK(!vfs::keep_in_partial(dir.string(), {"pak1.pak", "base", 2, 0, 0, 0, 0},
                              std::string(56, 'x'), b, &kept, &error));
  CHECK(error.find("longer") != std::string::npos);
  CHECK(vfs::partial_name("noext").empty() && vfs::partial_name("a/b.pak").empty());
  // A loose file has no archive to go back into.
  vfs::SearchPath sp;
  std::ofstream(dir / "loose.txt") << "x";
  CHECK_MSG(vfs::add_file(&sp, dir.string(), "loose.txt", "base", &error), error);
  vfs::ArchiveOrigin o;
  const vfs::FileRef* f = vfs::find(sp, "loose.txt");
  CHECK(f != nullptr && !vfs::archive_origin(sp, *f, &o));
  fs::remove_all(dir);
}

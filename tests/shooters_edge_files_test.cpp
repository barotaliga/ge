// The client's search path (games/shooters_edge/client/files.hpp) over folders written here: the
// base folder's packs, a later pack overriding an earlier one and a loose file overriding both; a
// mod folder over the base, its pack and its loose file winning and every other file still the
// base's; a mod's zip read too; the maps listed with the start map first; a loose file written
// after the mount found; the base game's numbered packs (and no stand-in) as the content packs,
// whose cache key is the one classic-enhance's pipeline gives the same packs.
//
// ge:summary Tests the client's search path: base and mod folders, packs, zips, loose files
// ge:keywords test shooters-edge search-path mod gamedir pak pk3 loose-files content_packs
// ge:skill vfs shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "assets/sha256.hpp"
#include "check.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/profile.hpp"
#include "shooters_edge/client/files.hpp"
#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

std::vector<uint8_t> bytes_of(const std::string& s) { return {s.begin(), s.end()}; }

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

void write_pack(const fs::path& p, const Files& files) {
  std::vector<uint8_t> b;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &b, &error), error);
  write(p, b);
}

std::string text_of(const cl::SearchPath& sp, const std::string& name) {
  std::vector<uint8_t> store;
  const auto b = cl::find_file(sp, name, store);
  return {b.begin(), b.end()};
}

// <root>/<base>: pak0 (maps a, start; sound x "base"), pak1 (sound x "later"), a loose readme;
// <root>/mod: pak0 (sound y "mod"), a loose readme.
fs::path make_tree() {
  const fs::path root = fs::temp_directory_path() / "ge_se_files_test";
  fs::remove_all(root);
  const fs::path base = root / std::string(ge::classicenhance::classic_profile().base_dir);
  write_pack(base / "pak0.pak", {{"maps/a.bsp", bytes_of("A")},
                                 {"maps/start.bsp", bytes_of("S")},
                                 {"sound/x.wav", bytes_of("base")}});
  write_pack(base / "PAK1.PAK", {{"sound/x.wav", bytes_of("later")}});
  write(base / "readme.txt", bytes_of("base readme"));
  write_pack(root / "mod" / "pak0.pak", {{"sound/y.wav", bytes_of("mod")}});
  write(root / "mod" / "readme.txt", bytes_of("mod readme"));
  return root;
}

}  // namespace

TEST(the_base_folder_layers_its_packs_and_loose_files) {
  const fs::path root = make_tree();
  const std::string base(ge::classicenhance::classic_profile().base_dir);
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), base, sp, error), error);
  CHECK(text_of(sp, "sound/x.wav") == "later");  // pak1 over pak0, its name in any case
  CHECK(text_of(sp, "maps/a.bsp") == "A");
  CHECK(text_of(sp, "readme.txt") == "base readme");
  CHECK(text_of(sp, "sound/y.wav").empty());  // the mod is not mounted
  const std::vector<std::string> maps = cl::map_names(sp);
  CHECK(maps.size() == 2 && maps[0] == "start" && maps[1] == "a");
  CHECK(cl::archive_paths(sp).size() == 2);
  CHECK(cl::content_packs(sp).size() == 2);
  fs::remove_all(root);
}

TEST(a_mod_folder_sits_over_the_base) {
  const fs::path root = make_tree();
  // A zip in the mod beside its pack, mounted after it.
  write(root / "mod" / "extra.pk3",
        ge::vfs::write_stored_zip({{"sound/z.wav", bytes_of("zipped")}}));
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", sp, error), error);
  CHECK(text_of(sp, "sound/y.wav") == "mod");        // the mod's own
  CHECK(text_of(sp, "sound/x.wav") == "later");      // the base's, through the mod
  CHECK(text_of(sp, "readme.txt") == "mod readme");  // the mod's loose file wins
  CHECK(text_of(sp, "sound/z.wav") == "zipped");
  CHECK(cl::map_names(sp).size() == 2);
  CHECK(sp.folder == (root / "mod").string());  // saves go to the mod's folder
  // The content packs are the base game's, not the mod's.
  CHECK(cl::content_packs(sp).size() == 2);
  // A file written into the game folder after the mount is found.
  write(root / "mod" / "demos" / "new.dem", bytes_of("fresh"));
  CHECK(text_of(sp, "demos/new.dem") == "fresh");
  fs::remove_all(root);
}

TEST(a_missing_game_folder_says_where_to_point) {
  cl::SearchPath sp;
  std::string error;
  CHECK(!cl::open_game_dir((fs::temp_directory_path() / "ge_se_no_such").string(), "nope", sp,
                           error));
  CHECK(error.find("-basedir") != std::string::npos);
}

TEST(the_content_packs_key_the_cache_as_the_pipeline_does) {
  const fs::path root = make_tree();
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", sp, error), error);
  // A stand-in pack added by hand is not one of the game's packs.
  CHECK_MSG(cl::add_archive(
                sp, "stage-fixture.pak",
                [] {
                  std::vector<uint8_t> b;
                  std::string e;
                  ge::vfs::write_pak({{"progs/x.mdl", {1}}}, &b, &e);
                  return b;
                }(),
                error),
            error);
  const auto packs = cl::content_packs(sp);
  CHECK(packs.size() == 2);
  std::vector<std::string> digests;
  digests.reserve(packs.size());
  for (const auto p : packs) digests.push_back(ge::assets::sha256_hex(p));
  const std::string key = ge::classicenhance::set_key(digests);
  CHECK(key.size() == 64 && key != digests[0]);
  CHECK(ge::classicenhance::set_key({&digests[0], 1}) == digests[0]);
  fs::remove_all(root);
}

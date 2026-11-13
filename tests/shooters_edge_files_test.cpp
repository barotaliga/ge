// The client's search path (games/shooters_edge/client/files.hpp) over folders written here: the
// base folder's packs, a later pack overriding an earlier one and a loose file overriding both; a
// mod folder over the base, its pack and its loose file winning and every other file still the
// base's; a mod's zip read too; the maps listed with the start map first; a loose file written
// after the mount found; the base game's numbered packs (and no stand-in) as the content packs,
// whose cache key is the one classic-enhance's pipeline gives the same packs, a content zip
// standing in for the first pack (`pak0.pk3`) among them; a map's `.lit` beside it, else in
// `lits/`.
//
// ge:summary Tests the client's search path: base and mod folders, packs, zips, loose files
// ge:keywords test shooters-edge search-path mod gamedir pak pk3 loose-files content_packs
// ge:keywords sky_box_files sky-box
// ge:skill vfs shooter testing
#include <filesystem>
#include <fstream>
#include <span>
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

TEST(a_maps_coloured_light_is_found_beside_it_then_in_lits) {
  const fs::path root = fs::temp_directory_path() / "ge_se_files_lit_test";
  fs::remove_all(root);
  const std::string base(ge::classicenhance::classic_profile().base_dir);
  write_pack(root / base / "pak0.pak", {{"maps/a.bsp", bytes_of("A")},
                                        {"maps/a.lit", bytes_of("beside")},
                                        {"maps/b.bsp", bytes_of("B")}});
  write(root / base / "lits" / "a.lit", bytes_of("kept"));
  write(root / base / "lits" / "b.lit", bytes_of("kept b"));
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), base, sp, error), error);
  std::vector<uint8_t> store;
  std::string found;
  std::span<const uint8_t> lit = cl::map_lit(sp, "a", store, &found);
  CHECK(std::string(lit.begin(), lit.end()) == "beside" && found == "maps/a.lit");
  lit = cl::map_lit(sp, "b", store, &found);
  CHECK(std::string(lit.begin(), lit.end()) == "kept b" && found == "lits/b.lit");
  found.clear();
  CHECK(cl::map_lit(sp, "c", store, &found).empty() && found.empty());
  fs::remove_all(root);
}

TEST(a_sky_boxs_six_pictures_are_found_each_by_its_own_path_and_format) {
  const fs::path root = fs::temp_directory_path() / "ge_se_files_sky_test";
  fs::remove_all(root);
  const std::string base(ge::classicenhance::classic_profile().base_dir);
  // rt, bk and lf in the pack under env/; ft as a png under gfx/env/; up with an underscore; dn
  // both as a tga and a pcx, the tga first.
  write_pack(root / base / "pak0.pak", {{"env/duskrt.tga", bytes_of("rt")},
                                        {"env/duskbk.tga", bytes_of("bk")},
                                        {"env/dusklf.pcx", bytes_of("lf")}});
  write(root / base / "gfx" / "env" / "duskft.png", bytes_of("ft"));
  write(root / base / "env" / "dusk_up.jpg", bytes_of("up"));
  write(root / base / "env" / "duskdn.pcx", bytes_of("dn pcx"));
  write(root / base / "env" / "duskdn.tga", bytes_of("dn"));
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), base, sp, error), error);
  cl::SkyBoxFiles found;
  std::string missing;
  CHECK_MSG(cl::sky_box_files(sp, "dusk", &found, &missing), missing);
  CHECK(found.paths[0] == "env/duskrt.tga" && found.paths[2] == "env/dusklf.pcx");
  CHECK(found.paths[3] == "gfx/env/duskft.png" && found.paths[4] == "env/dusk_up.jpg");
  CHECK(found.paths[5] == "env/duskdn.tga");
  CHECK(std::string(found.bytes[5].begin(), found.bytes[5].end()) == "dn");
  // One missing refuses the box and names it; a name with a dot or a slash finds nothing.
  fs::remove(root / base / "env" / "dusk_up.jpg");
  cl::SearchPath again;
  CHECK_MSG(cl::open_game_dir(root.string(), base, again, error), error);
  CHECK(!cl::sky_box_files(again, "dusk", &found, &missing) && missing == "up");
  CHECK(!cl::sky_box_files(again, "../dusk", &found, &missing));
  CHECK(!cl::sky_box_files(again, "", &found, &missing));
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

TEST(a_content_zip_standing_in_for_the_first_pack_keys_the_cache) {
  const fs::path root = make_tree();
  const fs::path base = root / std::string(ge::classicenhance::classic_profile().base_dir);
  // The base's first pack repackaged as a content zip: the zip is the first content pack.
  fs::remove(base / "pak0.pak");
  const std::vector<uint8_t> zip =
      ge::vfs::write_stored_zip({{"maps/a.bsp", bytes_of("A")}, {"maps/start.bsp", bytes_of("S")}});
  write(base / "PAK0.PK3", zip);
  write(base / "extra.pk3", ge::vfs::write_stored_zip({{"sound/z.wav", bytes_of("zipped")}}));
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", sp, error), error);
  CHECK(text_of(sp, "maps/a.bsp") == "A");
  auto packs = cl::content_packs(sp);
  CHECK(packs.size() == 2);  // the zip, then pak1; not the other zip, nor the mod's pack
  if (packs.size() == 2) {
    CHECK(ge::assets::sha256_hex(packs[0]) == ge::assets::sha256_hex(zip));
    CHECK(std::string(packs[1].begin(), packs[1].end()).find("later") != std::string::npos);
  }
  // With the first pack back beside it, the pack is the first and the zip is not a content pack.
  write_pack(base / "pak0.pak", {{"maps/a.bsp", bytes_of("A")}});
  cl::SearchPath again;
  CHECK_MSG(cl::open_game_dir(root.string(), "mod", again, error), error);
  packs = cl::content_packs(again);
  CHECK(packs.size() == 2);
  for (const auto p : packs) CHECK(ge::assets::sha256_hex(p) != ge::assets::sha256_hex(zip));
  fs::remove_all(root);
}

// The release's words by key: from its own archive (a `*.kpf` zip) beside the game folder, or a
// loose file on the search path first; none when neither holds the file.
TEST(the_releases_words_come_from_its_archive_beside_the_game_folder) {
  const fs::path root = make_tree();
  const std::string base(ge::classicenhance::classic_profile().base_dir);
  cl::SearchPath sp;
  std::string error;
  CHECK_MSG(cl::open_game_dir(root.string(), base, sp, error), error);
  CHECK(cl::localized_text(sp) == nullptr);
  const fs::path other = fs::temp_directory_path() / "ge_se_files_test_words";
  fs::remove_all(other);
  const fs::path game = other / base;
  fs::create_directories(game);
  write(other / "Release.KPF",
        ge::vfs::write_stored_zip(
            {{"localization/loc_english.txt", bytes_of("qc_nailgun = \"Nailgun\"\n")}}));
  cl::SearchPath release;
  CHECK_MSG(cl::open_game_dir(other.string(), base, release, error), error);
  const auto words = cl::localized_text(release);
  CHECK(words != nullptr && words->words.at("qc_nailgun") == "Nailgun");
  CHECK(cl::localized_text(release) == words);  // read once a folder
  // A loose file on the search path is read before the archive.
  write(root / base / "localization" / "loc_english.txt", bytes_of("qc_nailgun = Loose\n"));
  cl::SearchPath loose;
  CHECK_MSG(cl::open_game_dir(root.string(), base, loose, error), error);
  const auto first = cl::localized_text(loose);
  CHECK(first != nullptr && first->words.at("qc_nailgun") == "Loose");
  fs::remove_all(other);
  fs::remove_all(root);
}

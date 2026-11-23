// A stock map in two versions (plugins/classic/content/map_versions.hpp): another release's copy
// kept hidden by its checksum beside the player's, read in its place while chosen, and the version
// a server took remembered.
//
// ge:summary Tests a map's versions: kept by checksum, chosen over the player's copy, remembered
// ge:keywords test map-versions version_path stored_versions choose_version keep_choice
// ge:skill classic testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/content/files.hpp"
#include "classic/content/map_versions.hpp"

namespace cc = ge::classic::content;
namespace fs = std::filesystem;

TEST(a_version_is_kept_beside_the_players_copy_and_read_in_its_place_while_chosen) {
  const fs::path root = fs::temp_directory_path() / "ge_map_versions";
  fs::remove_all(root);
  fs::create_directories(root / "id1" / "maps");
  {
    std::ofstream own(root / "id1" / "maps" / "dm3.bsp", std::ios::binary);
    own << "ours";
  }
  CHECK(cc::version_path("maps/dm3.bsp", 0x1a2b3c4du) == "maps/versions/dm3.1a2b3c4d.bsp");
  CHECK(cc::version_path("maps/../x.bsp", 1).empty());
  std::string why;
  const std::vector<uint8_t> theirs = {'t', 'h', 'e', 'i', 'r', 's'};
  const std::string kept = cc::keep_version(root / "id1", "maps/dm3.bsp", 0xabcu, theirs, why);
  CHECK_MSG(kept == "maps/versions/dm3.00000abc.bsp", why);
  cc::keep_version(root / "id1", "maps/dm3.bsp", 0xdefu, {'v', 'i', 's'}, why);
  const auto all = cc::stored_versions(root / "id1", "maps/dm3.bsp");
  CHECK(all.size() == 2 && all[0].first == kept && all[0].second == theirs);
  CHECK(cc::stored_versions(root / "id1", "maps/dm4.bsp").empty());
  // The player's own copy is untouched and read until a version is chosen.
  cc::SearchPath files;
  CHECK_MSG(cc::open_game_dir(root.string(), "id1", files, why), why);
  std::vector<uint8_t> store;
  const auto read = [&](const char* path) {
    const auto b = cc::find_file(files, path, store);
    return std::string(b.begin(), b.end());
  };
  CHECK(read("maps/dm3.bsp") == "ours");
  cc::choose_version(files, "maps/dm3.bsp", kept);
  CHECK(read("maps/dm3.bsp") == "theirs");
  cc::choose_version(files, "maps/dm3.bsp", "");
  CHECK(read("maps/dm3.bsp") == "ours");
  // The version each server took, one line each.
  CHECK(!cc::remembered_key(root / "id1", "a:27500", "maps/dm3.bsp"));
  cc::keep_choice(root / "id1", "a:27500", "maps/dm3.bsp", 0xabcu);
  cc::keep_choice(root / "id1", "b:27500", "maps/dm3.bsp", 0xdefu);
  cc::keep_choice(root / "id1", "a:27500", "maps/dm3.bsp", 0xdefu);
  CHECK(cc::remembered_key(root / "id1", "a:27500", "maps/dm3.bsp") == 0xdefu);
  CHECK(cc::remembered_key(root / "id1", "b:27500", "maps/dm3.bsp") == 0xdefu);
  fs::remove_all(root);
}

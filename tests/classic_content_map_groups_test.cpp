// plugins/classic/content/map_groups: the built-in exmy group (textures only, never listed or
// changed); a group made, refilled, changed by + and -, cleared; a map in one group at most; the
// lookups falling back to the group's files (a place file, the coloured light, a texture).
//
// ge:summary Tests map groups and sky groups and the lookups that fall back through them
// ge:keywords test mapgroup skygroup group_of apply_group exmy map_lit
// ge:skill classic testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/content/files.hpp"
#include "classic/content/map_groups.hpp"

namespace cc = ge::classic::content;
namespace fs = std::filesystem;

TEST(groups_are_made_changed_and_cleared) {
  cc::MapGroups g;
  CHECK(cc::group_of(g.maps, "E1M2") == "exmy" && cc::group_of(g.maps, "e1m2", false).empty());
  CHECK(cc::apply_group(g.maps, {}, "map") == std::vector<std::string>{"No map groups are set."});
  CHECK(cc::apply_group(g.maps, {"exmy", "dm6"}, "map") ==
        std::vector<std::string>{"The map group exmy cannot be changed."});
  CHECK(cc::apply_group(g.maps, {"duel", "dm4", "DM6", "ztndm3"}, "map").empty());
  CHECK(cc::group_of(g.maps, "dm6") == "duel");
  cc::apply_group(g.maps, {"duel", "-dm4", "ztndm3", "+aerowalk"}, "map");
  CHECK(cc::apply_group(g.maps, {"duel"}, "map") ==
        std::vector<std::string>{"duel: dm6 aerowalk"});  // the sign holds until the next
  CHECK(cc::apply_group(g.maps, {"other", "dm6"}, "map") ==
        std::vector<std::string>{"dm6 is already in the map group duel."});
  CHECK(cc::apply_group(g.maps, {"other"}, "map") ==
        std::vector<std::string>{"There is no map group called other."});  // left empty: gone
  CHECK(cc::apply_group(g.maps, {"bad?name", "dm2"}, "map").size() == 1);
  cc::apply_group(g.maps, {"duel", "clear"}, "map");
  CHECK(cc::group_of(g.maps, "dm6").empty() && cc::group_of(g.maps, "e2m1") == "exmy");
  cc::apply_group(g.skies, {"night", "dm3"}, "sky");
  cc::apply_group(g.skies, {"clear"}, "sky");
  CHECK(g.skies.empty());
}

TEST(a_grouped_map_reads_the_groups_light) {
  const fs::path game = fs::temp_directory_path() / "ge_classic_map_groups";
  fs::remove_all(game);
  fs::create_directories(game / "lits");
  std::ofstream(game / "lits" / "duel.lit") << "QLIT";
  cc::SearchPath files;
  files.folder = game.string();
  std::vector<uint8_t> store;
  std::string found;
  CHECK(cc::map_lit(files, "dm6", store, &found).empty());
  cc::apply_group(files.groups->maps, {"duel", "dm6"}, "map");
  const cc::SearchPath copy = files;  // copies share the groups
  CHECK(!cc::map_lit(copy, "dm6", store, &found).empty() && found == "lits/duel.lit");
  fs::remove_all(game);
}

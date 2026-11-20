// MAP GROUPS AND SKY GROUPS: maps that share files under the group's name. A map in a group whose
// name is not its own falls back, file by file, to the group's: its place file
// (`locs/<group>.loc`), coloured light (`maps/<group>.lit`, `lits/<group>.lit`) and replacement
// textures (`textures/<group>/<texture>`); a sky group's maps draw the sky box named after the
// group in place of whatever sky they ask for (even none). The map's name stays its own
// everywhere else (macros, recordings).
//
// - **The built-in group** `exmy` holds `start` and e1m1..e4m8 for its textures alone; it is not
//   listed, saved or changed. Sky groups have none.
// - **The command form** (`apply_group`): a group's name and a list of maps makes or refills it;
//   `+map` and `-map` change it, the sign holding until the next; `<group> clear` deletes it, and
//   a group left empty goes. A map belongs to one group at most: another group naming it is told.
//   At most 36 maps a group; names are matched in any case.
//
// ge:summary Map groups and sky groups: maps sharing place, light, texture and sky files
// ge:keywords mapgroup skygroup map-group sky-group exmy group_of loc lit textures sky-box
// ge:skill classic
// ge:test tests/classic_content_map_groups_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ge::classic::content {

inline constexpr int kGroupMaps = 36;

struct MapGroup {
  std::string name;
  std::vector<std::string> maps;  // lower case
  bool built_in = false;
};

struct MapGroups {
  std::vector<MapGroup> maps, skies;
  MapGroups();  // with the built-in `exmy`
};

// The group `map` belongs to when its name is not the map's ("": none); `built_in` false skips
// the built-in groups.
std::string group_of(const std::vector<MapGroup>& groups, std::string_view map,
                     bool built_in = true);

// One `mapgroup` or `skygroup` command's words (its name not among them) on `groups`: the lines
// it says (a listing, a refusal). `what` is "map" or "sky".
std::vector<std::string> apply_group(std::vector<MapGroup>& groups,
                                     const std::vector<std::string>& words, std::string_view what);

}  // namespace ge::classic::content

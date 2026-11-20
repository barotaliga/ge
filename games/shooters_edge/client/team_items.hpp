// THE TEAM'S ITEM SETS: which items count when the player takes one (`tp_took`: remembered for the
// took codes and `f_took` run; `tp_pickup`: remembered only) and which can be pointed at
// (`tp_point`, client/team_point.hpp), each a set of the classic item names.
//
// - **The names**, in their fixed order: quad pent ring suit ra ya ga mh health lg rl gl sng ng ssg
//   pack cells rockets nails shells flag teammate enemy eyes sentry disp quaded pented rune1 rune2
//   rune3 rune4; and the groups `powerups` (quad pent ring suit), `weapons` (lg rl gl sng ssg),
//   `armor` (ra ya ga), `ammo` (cells rockets nails shells), `players` (teammate enemy eyes),
//   `runes` (rune1..rune4), `default` (the set's own default) and `all` (every name; for `tp_took`
//   and `tp_pickup` without the players and the buildings). Matched in any case.
// - **The command**: no argument lists the set ("none" when it is empty); a list replaces it; a
//   first word with `+` or `-` keeps it and adds or removes, the sign holding until the next;
//   `none` empties it. A word it does not know is said, and the rest still apply.
// - **The defaults**: `tp_took` and `tp_pickup` the powerups, armour, weapons, mh, pack, rockets,
//   cells, flag and runes; `tp_point` those, the players, sentry and disp.
// - **Deathmatch 4** leaves the ammunition and the weapons out of a pickup's check; pointing has
// its
//   own rule (team_point.hpp). An item of Quake II's that has no classic name always counts.
// - The sets are kept until quit (a config sets them); `cfg_save` does not write them.
//
// ge:summary The team's item sets: tp_took, tp_pickup and tp_point, their names and groups
// ge:keywords tp_took tp_pickup tp_point item-set took pickup point powerups weapons armor ammo
// ge:keywords runes players item_flag
// ge:skill shooter console
// ge:test tests/shooters_edge_team_items_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace shooters_edge::client {

using ItemSet = uint64_t;  // one bit a name, in the names' order
inline constexpr int kItemNames = 32;
inline constexpr ItemSet kAllItems = (ItemSet{1} << kItemNames) - 1;

enum class ItemSetKind : uint8_t { took, pickup, point };

struct TeamItems {
  ItemSet took, pickup, point;
  TeamItems();
};

// The set's default.
ItemSet default_items(ItemSetKind kind);
// The bit of the name `name` ("ra"; any case), 0 for a name that is none.
ItemSet item_flag(std::string_view name);
// The bit of a team word setting ("tp_name_ra", "tp_name_backpack"), 0 for one with no classic
// name (Quake II's own items).
ItemSet item_of_word(std::string_view word_setting);
// `args` (the command's words, its name not among them) applied to `set`; returns the words it
// did not know.
std::vector<std::string> apply_items(ItemSet& set, ItemSetKind kind,
                                     const std::vector<std::string>& args);
// The set's names in order, one space between; "none" when empty.
std::string items_text(ItemSet set);
// Whether a pickup of `word_setting` counts in `set` under `deathmatch`.
bool item_counts(ItemSet set, std::string_view word_setting, int deathmatch);

// tp_took, tp_pickup and tp_point over `items` (it must outlive `con`).
void add_item_commands(ge::console::Console& con, TeamItems& items);

}  // namespace shooters_edge::client

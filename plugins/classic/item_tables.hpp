// A SHOOTER GAME'S ITEM TABLES IN A CLASSIC GAME: the shapes (plugins/shooterbots/item_tables.hpp)
// by the classic games' names, and the names a pickup calls an item given to a mod as local info.
//
// ge:summary The item tables' shapes in ge::classic, and the item names as local info lines
// ge:keywords item-tables item_name_lines localinfo item-name
// ge:skill classic shooter bots
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <span>
#include <string>
#include <vector>

#include "shooterbots/item_tables.hpp"

namespace ge::classic {

using ge::shooterbots::item_row;
using ge::shooterbots::item_words;
using ge::shooterbots::ItemKind;
using ge::shooterbots::ItemName;
using ge::shooterbots::ItemRow;
using ge::shooterbots::ItemTables;
using ge::shooterbots::ItemWords;
using ge::shooterbots::kItemKinds;
using ge::shooterbots::SoundMeaning;

// The server's console lines that give the mod `names`: `localinfo <class name> "<name>"` each.
inline std::vector<std::string> item_name_lines(std::span<const ItemName> names) {
  std::vector<std::string> lines;
  lines.reserve(names.size());
  for (const ItemName& n : names)
    lines.push_back("localinfo " + std::string(n.class_name) + " \"" + std::string(n.name) + "\"");
  return lines;
}

}  // namespace ge::classic

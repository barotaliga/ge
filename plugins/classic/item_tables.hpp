// A SHOOTER GAME'S ITEMS AS DATA, the shapes: what the bots, the HUD and the team's words know of a
// game's pickups, in tables each game fills with its own rows (Shooter's Edge's in
// games/shooters_edge/profiles/items.hpp).
//
// - **Item rows** (`ItemRow`): a map entity's class name (a trailing `*` matches any rest of
//   it), the spawnflags bits it needs (all of them; the rows are tried in order, so a variant's
//   row comes before the plain one), and what it sets: the kind, the amount (doubled by
//   spawnflags 1 when `doubles`), the respawn time, a weapon's bit and the name (empty: the
//   class name). A field at -1 (or `sets_kind` false) leaves the bot's default.
// - **Words** (`ItemWords`): an item's call word (what a teammate says: "ra", "rl") and the word
//   its pickup sound is heard as, by kind, and for a weapon by its bit (0 matches any).
// - **Names** (`ItemName`): what a pickup calls an item in the game's own words, by class name;
//   `item_name_lines` gives them to a mod as local info.
// - **Sounds** (`SoundMeaning`, hearing.hpp): the samples a listener reads.
//
// ge:summary A shooter game's item tables, the shapes: rows, call and heard words, names, sounds
// ge:keywords item-tables item-row item-words item-name item-kind call-word heard-word
// ge:keywords item_row item_words item_name_lines spawnflags respawn
// ge:skill classic shooter bots
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooter/hearing.hpp"

namespace ge::classic {

using ge::shooter::SoundMeaning;

enum class ItemKind : uint8_t {
  health,
  mega,
  armor1,
  armor2,
  armor3,
  shells,
  nails,
  rockets,
  cells,
  weapon,
  quad,
  pent,      // the shield (no damage taken)
  grenades,  // ammunition a game keeps apart from its rockets (the sequel's)
  slugs,     // the sequel's railgun's
};
inline constexpr int kItemKinds = 14;

struct ItemRow {
  std::string_view class_name{};  // a trailing '*' matches any rest
  int32_t needs_flags = 0;        // spawnflags bits all present
  bool sets_kind = true;
  ItemKind kind = ItemKind::health;
  float amount = -1;        // health given or ammunition; -1: unset
  bool doubles = false;     // spawnflags 1 doubles the amount
  float respawn_s = -1;     // -1: the default
  int32_t weapon = 0;       // a weapon's bit (0: none)
  std::string_view name{};  // empty: the class name
};

struct ItemWords {
  ItemKind kind;
  int32_t weapon;          // for a weapon: its bit, 0 any
  std::string_view call;   // said of it
  std::string_view heard;  // its pickup sound's word
};

struct ItemName {
  std::string_view class_name;  // the map's class name: the local info's key
  std::string_view name;        // what a pickup calls it
};

// A game's tables, which its profile row carries.
struct ItemTables {
  std::span<const ItemRow> items;                     // the map's items by class name
  std::span<const ItemWords> words;                   // their call and heard words
  std::span<const ge::shooter::SoundMeaning> sounds;  // the samples a listener reads
  std::span<const ItemName> names;                    // what a pickup calls them
};
// The server's console lines that give the mod `names`: `localinfo <class name> "<name>"` each.
inline std::vector<std::string> item_name_lines(std::span<const ItemName> names) {
  std::vector<std::string> lines;
  lines.reserve(names.size());
  for (const ItemName& n : names)
    lines.push_back("localinfo " + std::string(n.class_name) + " \"" + std::string(n.name) + "\"");
  return lines;
}

// The row `class_name` with `spawnflags` matches first, or nullptr.
inline const ItemRow* item_row(std::span<const ItemRow> rows, std::string_view class_name,
                               int32_t spawnflags) {
  for (const ItemRow& r : rows) {
    const bool prefix = !r.class_name.empty() && r.class_name.back() == '*';
    const std::string_view stem =
        prefix ? r.class_name.substr(0, r.class_name.size() - 1) : r.class_name;
    const bool named = prefix ? class_name.substr(0, stem.size()) == stem : class_name == stem;
    if (named && (spawnflags & r.needs_flags) == r.needs_flags) return &r;
  }
  return nullptr;
}

// The words of a `kind` (and a weapon's `weapon` bit), or nullptr.
inline const ItemWords* item_words(std::span<const ItemWords> words, ItemKind kind,
                                   int32_t weapon) {
  for (const ItemWords& w : words) {
    if (w.kind == kind && (w.weapon == 0 || w.weapon == weapon)) return &w;
  }
  return nullptr;
}

}  // namespace ge::classic

// The team's item sets (team_items.hpp).
//
// ge:summary The team's item sets: names, groups, defaults, the commands
// ge:keywords tp_took tp_pickup tp_point apply_items items_text item_counts
// ge:skill shooter console
// ge:test tests/shooters_edge_team_items_test.cpp
#include "shooters_edge/client/team_items.hpp"

#include <array>
#include <utility>

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr std::array<std::string_view, kItemNames> kNames = {
    "quad",   "pent",    "ring",   "suit",   "ra",    "ya",       "ga",    "mh",
    "health", "lg",      "rl",     "gl",     "sng",   "ng",       "ssg",   "pack",
    "cells",  "rockets", "nails",  "shells", "flag",  "teammate", "enemy", "eyes",
    "sentry", "disp",    "quaded", "pented", "rune1", "rune2",    "rune3", "rune4"};

ItemSet bits(std::initializer_list<std::string_view> names) {
  ItemSet s = 0;
  for (std::string_view n : names) s |= item_flag(n);
  return s;
}

const ItemSet kPowerups = bits({"quad", "pent", "ring", "suit"});
const ItemSet kWeapons = bits({"lg", "rl", "gl", "sng", "ssg"});
const ItemSet kArmour = bits({"ra", "ya", "ga"});
const ItemSet kAmmo = bits({"cells", "rockets", "nails", "shells"});
const ItemSet kPlayers = bits({"teammate", "enemy", "eyes"});
const ItemSet kRunes = bits({"rune1", "rune2", "rune3", "rune4"});
const ItemSet kBuildings = bits({"sentry", "disp"});

bool same_word(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    const auto low = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; };
    if (low(a[i]) != low(b[i])) return false;
  }
  return true;
}

// The bits `word` stands for under `kind` (0: none).
ItemSet word_bits(std::string_view word, ItemSetKind kind) {
  const ItemSet own = kind == ItemSetKind::point ? kAllItems : kAllItems & ~(kPlayers | kBuildings);
  if (same_word(word, "all")) return own;
  if (same_word(word, "default")) return default_items(kind);
  const std::pair<std::string_view, ItemSet> groups[] = {
      {"powerups", kPowerups}, {"weapons", kWeapons}, {"armor", kArmour},
      {"ammo", kAmmo},         {"players", kPlayers}, {"runes", kRunes}};
  for (const auto& [name, set] : groups)
    if (same_word(word, name)) return set & own;
  return item_flag(word) & own;
}

}  // namespace

TeamItems::TeamItems()
    : took(default_items(ItemSetKind::took)),
      pickup(default_items(ItemSetKind::pickup)),
      point(default_items(ItemSetKind::point)) {}

ItemSet default_items(ItemSetKind kind) {
  const ItemSet took =
      kPowerups | kArmour | kWeapons | kRunes | bits({"mh", "pack", "rockets", "cells", "flag"});
  return kind == ItemSetKind::point ? took | kPlayers | kBuildings : took;
}

ItemSet item_flag(std::string_view name) {
  for (size_t i = 0; i < kNames.size(); ++i)
    if (same_word(name, kNames[i])) return ItemSet{1} << i;
  return 0;
}

ItemSet item_of_word(std::string_view word_setting) {
  constexpr std::string_view kLead = "tp_name_";
  if (!word_setting.starts_with(kLead)) return 0;
  const std::string_view name = word_setting.substr(kLead.size());
  if (name == "backpack") return item_flag("pack");
  return item_flag(name);
}

std::vector<std::string> apply_items(ItemSet& set, ItemSetKind kind,
                                     const std::vector<std::string>& args) {
  std::vector<std::string> unknown;
  if (args.empty()) return unknown;
  if (args.size() == 1 && same_word(args[0], "none")) {
    set = 0;
    return unknown;
  }
  const char lead = args[0].empty() ? '\0' : args[0][0];
  if (lead != '+' && lead != '-') set = 0;
  bool adding = true;
  for (std::string_view w : args) {
    if (!w.empty() && (w[0] == '+' || w[0] == '-')) {
      adding = w[0] == '+';
      w.remove_prefix(1);
    }
    if (w.empty()) continue;
    const ItemSet b = word_bits(w, kind);
    if (word_bits(w, ItemSetKind::point) == 0 && !same_word(w, "none")) {  // no name at all
      unknown.emplace_back(w);
      continue;
    }
    set = adding ? set | b : set & ~b;
  }
  return unknown;
}

std::string items_text(ItemSet set) {
  std::string out;
  for (size_t i = 0; i < kNames.size(); ++i)
    if ((set & (ItemSet{1} << i)) != 0) {
      if (!out.empty()) out += ' ';
      out += kNames[i];
    }
  return out.empty() ? "none" : out;
}

bool item_counts(ItemSet set, std::string_view word_setting, int deathmatch) {
  const ItemSet b = item_of_word(word_setting);
  if (b == 0) return true;  // an item with no classic name: always
  if (deathmatch == 4) set &= ~(kAmmo | kWeapons);
  return (set & b) != 0;
}

void add_item_commands(con::Console& c, TeamItems& items) {
  const std::pair<std::string_view, ItemSetKind> kinds[] = {{"tp_took", ItemSetKind::took},
                                                            {"tp_pickup", ItemSetKind::pickup},
                                                            {"tp_point", ItemSetKind::point}};
  const std::string_view helps[] = {
      "tp_took [+|-]<item> ...: the items a pickup is told of for (f_took and the took codes)",
      "tp_pickup [+|-]<item> ...: more items a pickup is remembered for, with no f_took",
      "tp_point [+|-]<item> ...: the items and players you can point at"};
  for (size_t k = 0; k < 3; ++k) {
    const ItemSetKind kind = kinds[k].second;
    ItemSet* set = kind == ItemSetKind::took     ? &items.took
                   : kind == ItemSetKind::pickup ? &items.pickup
                                                 : &items.point;
    c.add_command(kinds[k].first, helps[k], [set, kind](con::Console& cc, const con::Args& a) {
      if (a.size() < 2) {
        cc.print(items_text(*set));
        return;
      }
      std::vector<std::string> words;
      for (size_t i = 1; i < a.size(); ++i) words.push_back(a[i]);
      for (const std::string& w : apply_items(*set, kind, words))
        cc.print("There is no item called " + w + ".");
    });
  }
}

}  // namespace shooters_edge::client

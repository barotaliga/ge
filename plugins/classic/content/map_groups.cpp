// Map groups and sky groups (map_groups.hpp).
//
// ge:summary Map groups and sky groups: the built-in group, the lookup, the command's words
// ge:keywords mapgroup skygroup group_of apply_group exmy
// ge:skill classic
// ge:test tests/classic_content_map_groups_test.cpp
#include "classic/content/map_groups.hpp"

#include <algorithm>

namespace ge::classic::content {

namespace {

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

bool valid_name(std::string_view n) {
  if (n.empty() || n.find("../") != n.npos || n.find("..\\") != n.npos) return false;
  for (const char c : n)
    if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126 ||
        std::string_view("?*:<>\"").find(c) != std::string_view::npos)
      return false;
  return true;
}

MapGroup* find(std::vector<MapGroup>& groups, std::string_view name) {
  for (MapGroup& g : groups)
    if (g.name == name) return &g;
  return nullptr;
}

std::string listing(const MapGroup& g) {
  std::string line = g.name + ":";
  for (const std::string& m : g.maps) line += " " + m;
  return line;
}

}  // namespace

MapGroups::MapGroups() {
  MapGroup exmy;
  exmy.name = "exmy";
  exmy.maps = {"start"};
  exmy.built_in = true;
  for (int e = 1; e <= 4; ++e)
    for (int m = 1; m <= 8; ++m)
      exmy.maps.push_back("e" + std::to_string(e) + "m" + std::to_string(m));
  maps.push_back(std::move(exmy));
}

std::string group_of(const std::vector<MapGroup>& groups, std::string_view map, bool built_in) {
  const std::string m = lower(map);
  for (const MapGroup& g : groups) {
    if (g.built_in && !built_in) continue;
    if (g.name != m && std::find(g.maps.begin(), g.maps.end(), m) != g.maps.end()) return g.name;
  }
  return {};
}

std::vector<std::string> apply_group(std::vector<MapGroup>& groups,
                                     const std::vector<std::string>& words, std::string_view what) {
  const std::string kind(what);
  std::vector<std::string> said;
  if (words.empty()) {
    for (const MapGroup& g : groups)
      if (!g.built_in) said.push_back(listing(g));
    if (said.empty()) said.push_back("No " + kind + " groups are set.");
    return said;
  }
  const std::string name = lower(words[0]);
  if (words.size() == 1 && name == "clear") {
    std::erase_if(groups, [](const MapGroup& g) { return !g.built_in; });
    return said;
  }
  MapGroup* g = find(groups, name);
  if (words.size() == 1) {
    said.push_back(g != nullptr ? listing(*g)
                                : "There is no " + kind + " group called " + name + ".");
    return said;
  }
  if (!valid_name(name)) return {name + " cannot name a " + kind + " group."};
  if (g != nullptr && g->built_in)
    return {"The " + kind + " group " + name + " cannot be changed."};
  if (words.size() == 2 && lower(words[1]) == "clear") {
    if (g == nullptr) return {"There is no " + kind + " group called " + name + "."};
    std::erase_if(groups, [&](const MapGroup& x) { return x.name == name; });
    return said;
  }
  if (g == nullptr) {
    MapGroup made;
    made.name = name;
    groups.push_back(std::move(made));
    g = &groups.back();
  }
  const char lead = words[1].empty() ? '\0' : words[1][0];
  if (lead != '+' && lead != '-') g->maps.clear();
  bool adding = true;
  for (size_t i = 1; i < words.size(); ++i) {
    std::string_view w = words[i];
    if (!w.empty() && (w[0] == '+' || w[0] == '-')) {
      adding = w[0] == '+';
      w.remove_prefix(1);
    }
    const std::string map = lower(w);
    if (map.empty()) continue;
    auto at = std::find(g->maps.begin(), g->maps.end(), map);
    if (!adding) {
      if (at != g->maps.end()) g->maps.erase(at);
      continue;
    }
    if (at != g->maps.end()) continue;
    const std::string other = group_of(groups, map, false);
    if (!other.empty() && other != name) {
      said.push_back(map + " is already in the " + kind + " group " + other + ".");
      continue;
    }
    if (static_cast<int>(g->maps.size()) >= kGroupMaps) {
      said.push_back("A " + kind + " group holds at most " + std::to_string(kGroupMaps) + " maps.");
      break;
    }
    g->maps.push_back(map);
  }
  if (g->maps.empty()) std::erase_if(groups, [&](const MapGroup& x) { return x.name == name; });
  return said;
}

}  // namespace ge::classic::content

// The player page's figures (figures.hpp).
//
// ge:summary Figures and their skins found under players/, in the page's order
// ge:keywords player_figures split_look players skins
// ge:skill ui shooter
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/figures.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace shooters_edge::menu {

std::vector<Figure> player_figures(const ge::vfs::SearchPath& files) {
  // Per folder: whether it has tris.md2, its skins and its icons (folded names from the list).
  struct Folder {
    bool body = false;
    std::set<std::string> pictures;
  };
  std::map<std::string, Folder> folders;
  for (const ge::vfs::Listed& f : ge::vfs::list(files)) {
    const std::string_view name = f.name;
    if (name.substr(0, 8) != "players/") continue;
    const size_t slash = name.find('/', 8);
    if (slash == std::string_view::npos || slash == 8) continue;
    const std::string_view file = name.substr(slash + 1);
    if (file.find('/') != std::string_view::npos) continue;
    Folder& d = folders[std::string(name.substr(8, slash - 8))];
    if (file == "tris.md2")
      d.body = true;
    else if (file.size() > 4 && file.substr(file.size() - 4) == ".pcx")
      d.pictures.insert(std::string(file.substr(0, file.size() - 4)));
  }
  std::vector<Figure> out;
  for (const auto& [model, d] : folders) {
    if (!d.body) continue;
    Figure fig{model, {}};
    for (const std::string& p : d.pictures) {
      const bool icon = p.size() > 2 && p.substr(p.size() - 2) == "_i";
      if (!icon && d.pictures.count(p + "_i") != 0) fig.skins.push_back(p);
    }
    if (!fig.skins.empty()) out.push_back(std::move(fig));
  }
  const auto rank = [](const std::string& m) { return m == "male" ? 0 : m == "female" ? 1 : 2; };
  std::stable_sort(out.begin(), out.end(), [&rank](const Figure& a, const Figure& b) {
    return rank(a.model) != rank(b.model) ? rank(a.model) < rank(b.model) : a.model < b.model;
  });
  if (out.size() > 1024) out.resize(1024);
  return out;
}

std::pair<std::string, std::string> split_look(std::string_view skin) {
  const size_t cut = skin.find_first_of("/\\");
  if (cut == std::string_view::npos) return {"male", "grunt"};
  return {std::string(skin.substr(0, cut)), std::string(skin.substr(cut + 1))};
}

}  // namespace shooters_edge::menu

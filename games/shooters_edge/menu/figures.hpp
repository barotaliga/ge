// THE PLAYER PAGE'S FIGURES (Quake II's player setup): every folder under `players/` that holds
// `tris.md2` and at least one usable skin, a `.pcx` that is not an icon and has its icon
// `<skin>_i.pcx` beside it (names compared ignoring case). `male` first, `female` second, the rest
// by name ignoring case; each figure's skins by name, without the extension. The `skin` setting
// names one as `<model>/<skin>` (or `\`); with neither, `male` and `grunt`.
//
// ge:summary The player page's figures and skins read off the game's files, and the skin setting
// ge:keywords player-setup figures models skins players tris-md2 icon skin-setting split_look
// ge:keywords player_figures
// ge:skill ui shooter
// ge:test tests/shooters_edge_menu_figures_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "vfs/search.hpp"

namespace shooters_edge::menu {

struct Figure {
  std::string model;               // its folder's name, lower case
  std::vector<std::string> skins;  // lower case, no extension, by name
};

std::vector<Figure> player_figures(const ge::vfs::SearchPath& files);

// The model and skin a `skin` setting names (`male`, `grunt` when it names neither).
std::pair<std::string, std::string> split_look(std::string_view skin);

}  // namespace shooters_edge::menu

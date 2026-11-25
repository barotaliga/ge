// The window's classic pictures, the packs' or their enhanced copies (classic_pictures.hpp).
//
// ge:summary Loads the classic menu and status bar pictures through the game's adapter
// ge:keywords classic-pictures load upgrade cached_picture find_cache
// ge:skill classicenhance ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/client/classic_pictures.hpp"

#include "core/output.hpp"

#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

ClassicPictures load_classic_pictures(const SearchPath& files, bool enhance,
                                      const std::filesystem::path& cache_root,
                                      const GameAdapter* game) {
  const GameAdapter& g = game != nullptr ? *game : qw_adapter();
  return g.pictures(g, files, enhance, cache_root);
}

void say_hud_pictures(const hud::HudArt& art, const SearchPath& files, const std::string& why) {
  namespace out = ge::core::output;
  if (!art.bar.empty())
    out::status("shooters_edge: the HUD draws the packs' own status bar pictures" +
                std::string(art.density > 1 ? ", enhanced (ce_pictures)" : ""));
  else if (!archive_paths(files).empty())
    out::status("shooters_edge: the HUD has no status bar pictures: " + why);
}

const hud::HudArt& hud_art_of(const SearchPath& files, const hud::HudArt& game,
                              const hud::HudArt& own) {
  return files.folder.empty() ? own : game;
}

}  // namespace shooters_edge::client

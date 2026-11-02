// The window's classic pictures, the packs' or their enhanced copies (classic_pictures.hpp).
//
// ge:summary Loads the classic menu and status bar pictures through the game's adapter
// ge:keywords classic-pictures load upgrade cached_picture find_cache
// ge:skill classicenhance ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/client/classic_pictures.hpp"

#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

ClassicPictures load_classic_pictures(const SearchPath& files, bool enhance,
                                      const std::filesystem::path& cache_root,
                                      const GameAdapter* game) {
  const GameAdapter& g = game != nullptr ? *game : classic_adapter();
  return g.pictures(g, files, enhance, cache_root);
}

}  // namespace shooters_edge::client

// The window's classic pictures, the packs' or their enhanced copies (classic_pictures.hpp).
//
// ge:summary Loads the classic menu and status bar pictures, upgraded from the cache
// ge:keywords classic-pictures load upgrade cached_picture find_cache
// ge:skill classicenhance ui
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/client/classic_pictures.hpp"

#include <utility>

#include "classicenhance/cached.hpp"
#include "shooters_edge/hud/classic_art.hpp"

namespace shooters_edge::client {

ClassicPictures load_classic_pictures(const SearchPath& files, bool enhance,
                                      const std::filesystem::path& cache_root) {
  ClassicPictures out;
  out.enhanced = enhance;
  if (enhance && !files.archives.empty())
    out.cache = ge::classicenhance::find_cache(files.archives[0].bytes, cache_root);
  const auto find = [&files](std::string_view p, std::vector<uint8_t>& store) {
    return find_file(files, p, store);
  };
  const std::filesystem::path& dir = out.cache;
  const auto upgrade = [&dir](std::string_view name, int w, int h, ge::ui::Image* img) {
    ge::assets::Image big;
    const uint32_t k = ge::classicenhance::cached_picture(dir, name, static_cast<uint32_t>(w),
                                                          static_cast<uint32_t>(h), &big);
    if (k == 0) return k;
    img->width = big.width;
    img->height = big.height;
    img->rgba = std::move(big.rgba);
    return k;
  };
  const menu::PictureUpgrade up_menu =
      dir.empty() ? menu::PictureUpgrade{} : menu::PictureUpgrade(upgrade);
  const hud::PictureUpgrade up_hud =
      dir.empty() ? hud::PictureUpgrade{} : hud::PictureUpgrade(upgrade);
  std::string error;
  if (!menu::load_classic_art(find, &out.menu, &error, up_menu)) out.menu = {};
  if (!hud::classic_art_from_packs(find, &out.hud, &out.hud_why, up_hud)) out.hud = {};
  return out;
}

}  // namespace shooters_edge::client

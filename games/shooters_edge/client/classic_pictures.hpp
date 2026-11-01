// THE WINDOW'S CLASSIC PICTURES: the classic menu's pictures (menu/classic_art.hpp) and the classic
// status bar's (hud/classic_art.hpp), read from the packs on the search path and, when
// `ce_pictures` is on, swapped for classic-enhance's upscaled copies in the packs' cache
// (`cached_picture`, its pictures stage), so the menus, the status bar and the glyph sheet's text
// are drawn from four times the pixels.
//
// The cache is the first pack's, as the world's textures read it (world_draw.cpp); a pack never
// enhanced, or a cache from before the pictures stage, leaves the pack's own pictures. The window
// loads them at start and again when `ce_pictures` flips or a map loads (after an enhance run
// from the menu, the next map shows its pictures).
//
// ge:summary The window's classic menu and status bar pictures, enhanced when cached
// ge:keywords classic-pictures menu hud status-bar glyph-sheet conchars ce_pictures enhanced
// ge:keywords cache cached_picture upgrade density load reload
// ge:skill classicenhance ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <filesystem>
#include <string>

#include "shooters_edge/client/files.hpp"
#include "shooters_edge/hud/art.hpp"
#include "shooters_edge/menu/classic_art.hpp"

namespace shooters_edge::client {

struct ClassicPictures {
  menu::ClassicArt menu;        // empty when the packs lack the menu's pictures
  hud::HudArt hud;              // empty (`bar` empty) when the packs lack the status bar's
  std::string hud_why;          // why `hud` is empty
  std::filesystem::path cache;  // the packs' enhanced cache read from, empty: the packs' own
  bool enhanced = false;        // asked for the enhanced copies (ce_pictures)
};

// The pictures of `files`, enhanced from the packs' cache when `enhance` is set and the cache has
// them (an empty `cache_root` is classic-enhance's own folder).
ClassicPictures load_classic_pictures(const SearchPath& files, bool enhance,
                                      const std::filesystem::path& cache_root = {});

}  // namespace shooters_edge::client

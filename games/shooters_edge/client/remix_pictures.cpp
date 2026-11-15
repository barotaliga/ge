// Classic remix's menu and letters (remix_pictures.hpp).
//
// ge:summary Classic remix's cr_menu and cr_charset: another game's menu pictures and letters
// ge:keywords remix cr_menu cr_charset glyph-sheet take_glyphs remixed_pictures
// ge:skill ui classicenhance
// ge:test tests/shooters_edge_remix_test.cpp
#include "shooters_edge/client/remix_pictures.hpp"

#include <utility>

#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::client {

namespace {

// Another game's pictures read from the remix's files, or none when `game` is the played one.
bool other_pictures(const RemixFiles& remix, std::string_view game, bool enhance,
                    ClassicPictures& out) {
  if (game.empty() || game == remix.played) return false;
  const profiles::GameProfile* row = profiles::find_game(game);
  if (row == nullptr || remix.files.folder.empty()) return false;
  out = load_classic_pictures(remix.files, enhance, {}, &adapter_of(*row));
  return true;
}

}  // namespace

bool take_glyphs(ge::ui::Image& into, float density, const ge::ui::Image& sheet, float from) {
  if (sheet.empty() || from <= 0 || density < from) return false;
  const auto k = static_cast<int>(density / from);
  if (static_cast<float>(k) * from != density) return false;
  into = k == 1 ? sheet : ge::ui::scaled_nearest(sheet, k);
  return true;
}

ClassicPictures remixed_pictures(const SearchPath& files, const RemixFiles& remix, bool enhance,
                                 const GameAdapter& played) {
  ClassicPictures own = load_classic_pictures(files, enhance, {}, &played);
  ClassicPictures other;
  const bool menu = other_pictures(remix, remix.menu, enhance, other);
  // The letters' sheet: the menu's game's, read once for both, or cr_charset's own read.
  ge::ui::Image sheet;
  float sheet_density = 1;
  if (menu && remix.charset == remix.menu) {
    sheet = other.menu.glyphs;
    sheet_density = other.menu.density;
  } else if (ClassicPictures letters; other_pictures(remix, remix.charset, enhance, letters)) {
    sheet = std::move(letters.menu.glyphs);
    sheet_density = letters.menu.density;
  }
  if (menu && other.menu.menu_ready()) {
    // The other game's menu, written in the played game's letters unless cr_charset names another.
    take_glyphs(other.menu.glyphs, other.menu.density, own.menu.glyphs, own.menu.density);
    own.menu = std::move(other.menu);
  }
  if (!sheet.empty()) {
    take_glyphs(own.menu.glyphs, own.menu.density, sheet, sheet_density);
    if (!own.hud.glyphs.empty()) take_glyphs(own.hud.glyphs, own.hud.density, sheet, sheet_density);
  }
  return own;
}

}  // namespace shooters_edge::client

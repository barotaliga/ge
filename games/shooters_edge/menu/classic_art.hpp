// THE CLASSIC MENU'S PICTURES: the menu art the player's own classic pack carries (`gfx/*.lmp`:
// the plaque down the left, each page's title, the strips of a page's rows, the spinning cursor,
// the help pages, the text box's pieces) and the console's glyph sheet from its `gfx.wad`, read
// off the game's search path and coloured by the pack's palette (`gfx/palette.lmp`), so
// classic_view.hpp draws the menu as the pack's own.
//
// ENHANCED (`ce_pictures`): given an `upgrade`, every picture and the glyph sheet are swapped for
// classic-enhance's upscaled copies (`cached_picture`), and `density` is their scale: the view
// draws each picture at its classic size, filtered, from `density` times the pixels. A picture
// the cache lacks is the pack's scaled up nearest to the same density.
//
// Nothing here ships: the pictures are read at run time from the packs the player put in the game
// folder. A pack without the menu's pictures (no palette, no glyph sheet, no main strip) loads
// nothing and the menu keeps its modern look. The labs and the tests use `fixture_files`, pictures
// in the same formats painted in code.
//
// ge:summary The classic menu's pictures (gfx/*.lmp, the glyph sheet) read from the player's pack
// ge:keywords menu classic pictures lmp palette conchars glyph-sheet plaque title strip cursor
// ge:keywords help box pack search-path fixture shooters-edge ranking scorebar palette-colour
// ge:keywords enhanced upgrade ce_pictures density upscaled
// ge:skill ui vfs
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/hud/classic_art.hpp"
#include "ui/image.hpp"

namespace shooters_edge::menu {

// A file by its path in the game ("gfx/palette.lmp"): a view into `store` or into a pack, empty
// when no place has it (the client's `find_file`).
using FindFile =
    std::function<std::span<const uint8_t>(std::string_view path, std::vector<uint8_t>& store)>;

struct ClassicArt {
  // By the picture's name without its folder and extension: `qplaque`, `ttl_main`, `mainmenu`,
  // `menudot1` .. `menudot6`, `help0` ..; index 255 drawn see-through.
  std::map<std::string, ge::ui::Image, std::less<>> pictures;
  ge::ui::Image glyphs;  // the 128 x 128 glyph sheet, 16 x 16 cells of 8 px; index 0 see-through
  std::array<uint8_t, 768> palette{};  // the pack's palette, RGB a colour (the scoreboard's fills)
  float density = 1;  // picture pixels a classic pixel: 1 the pack's own, 4 enhanced

  // A picture's size in classic pixels.
  float width_of(const ge::ui::Image& img) const { return static_cast<float>(img.width) / density; }
  float height_of(const ge::ui::Image& img) const {
    return static_cast<float>(img.height) / density;
  }

  bool loaded() const { return !glyphs.empty() && find("mainmenu") != nullptr; }
  const ge::ui::Image* find(std::string_view name) const;
  // The palette's colour `index` (0..255), opaque.
  ge::ui::Color colour(int index) const;
};

// A picture's enhanced copy by classic-enhance's name for it (`conchars`, `num_0`, `gfx/qplaque`):
// the HUD's hook (hud/classic_art.hpp).
using PictureUpgrade = hud::PictureUpgrade;

// Every picture the classic menu draws, read through `find`, each upgraded through `upgrade` when
// it is set (above). False with the reason when the files lack the palette, the glyph sheet or the
// main menu's strip; a picture missing past those is left out (its page falls back to the glyphs).
bool load_classic_art(const FindFile& find, ClassicArt* out, std::string* error,
                      const PictureUpgrade& upgrade = {});

// The pictures' names the view reads (without the folder and the extension).
std::span<const std::string_view> classic_picture_names();

// The labs' and the tests' stand-in: the palette, a `gfx.wad` holding a glyph sheet and every
// menu picture, painted in code in the pack's formats, as path and bytes.
std::map<std::string, std::vector<uint8_t>, std::less<>> fixture_files();

}  // namespace shooters_edge::menu

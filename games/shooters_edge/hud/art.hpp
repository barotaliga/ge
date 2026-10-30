// THE HUD'S PICTURES: every picture Shooter's Edge's HUDs draw, by what it shows, in one set the
// layouts read (layout.hpp). Two sets fill it:
//
//   - the game's own FREE ART (`free_art`), painted in code on the 2D layer's CPU raster at four
//     picture pixels per art pixel, so it is sharp at any size and needs no file;
//   - a player's own classic content, read from the pack by classic-enhance into the same set
//     (games/shooters_edge/hud/classic_art.hpp once that plugin lands), at one picture pixel per
//     art pixel and drawn nearest, so the classic status bar looks as it did.
//
// Every size below is in ART PIXELS, the classic status bar's: the bar and the inventory bar are
// 320 x 24, an icon on the bar 24 x 24, a weapon on the inventory bar 24 x 16 (the beam gun 48 x
// 16), a key or a powerup 16 x 16, a rune 8 x 16, a digit 24 x 24. A set's `density` is its
// pictures' pixels per art pixel.
//
// THE PORTRAIT has five health bands (0 near death .. 4 healthy), each calm and wincing, and four
// powered faces.
//
// ge:summary Shooter's Edge's HUD pictures by what they show, and the free set painted in code
// ge:keywords hud art pictures icons free-art portrait face digits status-bar inventory-bar
// ge:keywords weapons ammo armour keys powerups runes crosshair density glyph-sheet
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <array>

#include "hud/face.hpp"
#include "hud/pictures.hpp"
#include "shooters_edge/hud/status.hpp"
#include "ui/image.hpp"

namespace shooters_edge::hud {

inline constexpr int kFaceBands = 5;
inline constexpr int kFlashFrames = 5;  // a picked-up weapon's flash, 10 frames a second

struct HudArt {
  float density = 1;  // picture pixels per art pixel

  ge::ui::Image bar;            // 320 x 24: the status bar's panel
  ge::ui::Image inventory_bar;  // 320 x 24: the inventory bar's, above it
  ge::ui::Image health;         // a health icon for the looks that show one; empty: the portrait
  std::array<ge::ui::Image, 3> armour;  // light, medium, heavy
  ge::ui::Image armour_shield;          // the armour's icon while the shield runs
  std::array<ge::ui::Image, kAmmoKinds> ammo;
  std::array<std::array<ge::ui::Image, 2>, kFaceBands> face;  // [band][0 calm, 1 wincing]
  ge::ui::Image face_cloak, face_shield, face_cloak_shield, face_damage;
  // The guns on the inventory bar: as held, lit (the one in hand), and the pickup's flash frames.
  std::array<ge::ui::Image, kGuns> gun, gun_lit;
  std::array<std::array<ge::ui::Image, kFlashFrames>, kGuns> gun_flash;
  std::array<ge::ui::Image, 2> key;  // silver, gold
  std::array<ge::ui::Image, kPowers> power;
  std::array<ge::ui::Image, 4> rune;
  std::array<ge::ui::Image, 11> digit, digit_low;  // 0..9 and the minus; low: the alarm colour
  ge::ui::Image crosshair;
  // The type the counts on the inventory bar and the board are drawn in: a glyph sheet when the
  // set has one (`glyphs` not empty, its figures at `digits_at`), else a charset.
  ge::ui::Image glyphs;
  uint8_t digits_at = '0';
  ge::charset::FontId count_font = ge::charset::FontId::terminal;
  ge::ui::Color count_color{1, 1, 1, 1};  // the counts' tint (a sheet's figures carry their own)

  // Filled by `finish` from the pictures above; the layouts point at them.
  ge::hud::DigitSheet sheet;
  ge::hud::Face count_face;

  // Points `sheet` and `count_face` at this set's pictures: call once the pictures are in, and
  // again after the set is moved.
  void finish();
};

// The free set, painted in code: the same every run and on every platform.
HudArt free_art();

}  // namespace shooters_edge::hud

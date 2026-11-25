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
// A MISSION PACK'S PICTURES: a pack's graphics archive replaces the base game's whole and adds its
// own pictures by name; the classic set loads them when every one of a pack's set is there, and
// `pack` says which set came (any pack or mod whose archive carries the same names gets the same
// status bar). With it the classic bar reads the status's items word (status.hpp) as that pack's
// program fills it.
//
// ge:summary Shooter's Edge's HUD pictures by what they show, and the free set painted in code
// ge:keywords hud art pictures icons free-art portrait face digits status-bar inventory-bar
// ge:keywords weapons ammo armour keys powerups runes crosshair density glyph-sheet
// ge:keywords mission-pack scourge-of-armagon dissolution-of-eternity pack-pictures team-border
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "hud/face.hpp"
#include "hud/pictures.hpp"
#include "shooters_edge/hud/status.hpp"
#include "ui/image.hpp"

namespace shooters_edge::hud {

inline constexpr int kFaceBands = 5;
inline constexpr int kFlashFrames = 5;  // a picked-up weapon's flash, 10 frames a second

// Which mission pack's pictures a set carries (`HudArt::pack`).
enum class Pack : uint8_t { none, armagon, eternity };

// A gun's three looks: as held, lit in hand, and the pickup's flash frames.
struct GunPictures {
  ge::ui::Image plain, lit;
  std::array<ge::ui::Image, kFlashFrames> flash;
};

// Scourge of Armagon's guns on the inventory bar, by `ArmagonGun`.
enum class ArmagonGun : uint8_t { laser, hammer, launcher_and_prox, prox_and_launcher, prox };
inline constexpr int kArmagonGuns = 5;

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
  ge::ui::Image turtle, net;  // the slow-frame and the silent-server icons; empty: none drawn

  // A mission pack's own pictures (above); `pack` none: none loaded.
  Pack pack = Pack::none;
  std::array<GunPictures, kArmagonGuns> armagon_gun;  // by ArmagonGun
  std::array<ge::ui::Image, 2> armagon_item;          // the wetsuit, the empathy shields
  std::array<ge::ui::Image, 2> eternity_bar;   // the inventory bar: a pack gun in hand, a base one
  std::array<ge::ui::Image, 5> eternity_gun;   // lava nailgun, lava super nailgun, multi grenade
                                               // launcher, multi rocket launcher, plasma gun
  std::array<ge::ui::Image, 3> eternity_ammo;  // lava nails, plasma, multi rockets
  std::array<ge::ui::Image, 2> eternity_item;  // the power shield, the anti-grav belt
  ge::ui::Image team_border;                   // the frame the team colours fill
  // The classic palette's 256 colours (768 bytes) when the set came from the packs; empty for the
  // free set (crosshair.hpp's colours by index).
  std::vector<uint8_t> palette;
  // The type the counts on the inventory bar and the board are drawn in: a glyph sheet when the
  // set has one (`glyphs` not empty, its figures at `digits_at`), else a charset.
  ge::ui::Image glyphs;
  uint8_t digits_at = '0';
  ge::charset::FontId count_font = ge::charset::FontId::terminal;
  ge::ui::Color count_color{1, 1, 1, 1};  // the counts' tint (a sheet's figures carry their own)

  // Filled by `finish` from the pictures above; the layouts point at them.
  ge::hud::DigitSheet sheet;
  ge::hud::Face count_face;
  ge::hud::Face plain_face;  // the sheet's white figures (the team border's frags)

  // Points `sheet` and `count_face` at this set's pictures: call once the pictures are in, and
  // again after the set is moved.
  void finish();
};

// The free set, painted in code: the same every run and on every platform.
HudArt free_art();

}  // namespace shooters_edge::hud

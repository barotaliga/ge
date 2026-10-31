// SHOOTER'S EDGE'S HUDS: three looks over one table of elements (engine/hud/layout.hpp), picked
// by the `hud_style` cvar and each element still the player's to move, resize, fade or hide with
// its `hud_<id>_*` cvars.
//
//   - CLASSIC: the status bar across the bottom centre, as the classic game drew it, in art
//     pixels: the armour's icon and figure, the portrait, the health, the ammunition's icon and
//     figure on the bar; above it the inventory bar with the four ammunition counts on its top
//     line, the guns in a row (the one in hand lit, a new one flashing), the keys and powerups in
//     their slots, the runes, and on a frag match the four best players' plates. The bar is
//     scaled by whole numbers (crisp pixel art) and never wider than the screen. Its pieces are
//     the bar's children (`hud_sbar_*`, `hud_ibar_*`).
//   - MODERN: the corners. Health and armour bottom left, each an icon, a big figure and a thin
//     bar, the keys above them; the ammunition bottom right; the guns in a row at the bottom
//     centre with each one's rounds; the powerups down the right edge with their seconds left;
//     the frags top right.
//   - COMPACT: everything near the crosshair, where the eyes are. Two arcs round it, health on the
//     left and armour on the right, with their figures beside them; the ammunition under it and
//     the powerups with their seconds under that; the keys over it; the guns small at the
//     bottom.
//
// `viewsize` steps the bars away as the classic game did: at 100 or under every element shows; at
// 110 the classic look drops its inventory bar (the status bar alone); at 120 every look drops its
// status elements (the bar, the corners, the arcs), leaving the crosshair, the feed and the centre
// message.
//
// Every look also shows the crosshair, the notify feed (pickups and frags, top left) and the
// centre message. A look draws with the HUD's ART (art.hpp): the free set, or the player's own
// classic pictures when classic-enhance has them.
//
// The figures' colours say what the classic bar said: a figure in the alarm colour is low
// (health or armour 25 or under, ammunition 10 or under); the shield shows "666" over its disc.
//
// ge:summary Shooter's Edge's HUDs: the classic status bar, modern and compact looks, hud_style
// ge:keywords hud shooter status-bar inventory-bar classic modern compact hud_style looks
// ge:keywords health armour ammo face portrait weapons keys powerups runes frags crosshair
// ge:keywords notify centre-message ring arcs fill_hud add_cvars layout elements viewsize
// ge:skill ui console
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "hud/feed.hpp"
#include "hud/layout.hpp"
#include "shooters_edge/hud/art.hpp"
#include "shooters_edge/hud/status.hpp"

namespace shooters_edge::hud {

enum class Look : uint8_t { classic, modern, compact };
inline constexpr int kLooks = 3;

std::string_view look_name(Look l);
// The look a word names; classic for a word that names none.
Look look_from(std::string_view word);

// The HUD's type and colours (the kit's style).
ge::hud::Style hud_style();

// The table and what fills it every frame.
struct Hud {
  ge::hud::Layout layout;
  std::string look = "classic";  // bound to hud_style
  int32_t viewsize = 100;        // bound to viewsize: 110 drops the inventory bar, 120 every bar
  ge::hud::Feed notify{4, 4.0, 0.8, 0.15};
  std::string centre;  // the centre message; empty for none
  double centre_until = 0;
};

// The table: every look's elements, parents before their children.
Hud make_hud();

// Registers hud_style, viewsize and every element's hud_ cvars (`hud` must outlive `con`).
void add_cvars(ge::console::Console& con, Hud& hud);

// Writes every element's content for this frame from the status and the art: the active look's
// elements on (as the status calls for), every other look's off.
void fill_hud(Hud& hud, const Status& s, const HudArt& art);

}  // namespace shooters_edge::hud

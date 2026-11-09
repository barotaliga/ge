// SHOOTER'S EDGE'S HUDS: three looks over one table of elements (engine/hud/layout.hpp), picked
// by the `hud_style` cvar and each element still the player's to move, resize, fade or hide with
// its `hud_<id>_*` cvars.
//
//   - CLASSIC: the status bar across the bottom centre, flush with the screen's bottom edge, as the
//   classic game drew it, in art
//     pixels: the armour's icon and figure, the portrait, the health, the ammunition's icon and
//     figure on the bar; above it the inventory bar with the four ammunition counts on its top
//     line, the guns in a row (the one in hand lit, a new one flashing), the keys and powerups in
//     their slots, the runes, and on a frag match the four best players' plates. The bar is
//     scaled by whole numbers (crisp pixel art) and never wider than the screen. Its pieces are
//     the bar's children (`hud_sbar_*`, `hud_ibar_*`).
//   - MODERN: the corners. Health and armour bottom left, each an icon, a big figure and a thin
//     bar, the keys above them; the rounds bottom right with the four ammunition counts and the
//     guns in pairs over them; the powerups down the right edge with their seconds left; the
//     frags top right.
//   - COMPACT: one arch at the bottom centre: health's arc rising from the left and armour's from
//     the right, their figures inside it either side of the weapon in hand, the rounds by its right
//     foot, the keys and the powerups over it; the guns small on the right edge.
//   - ARENA: the competitive look (arena shooters and the classic games' tuned configs): health
//     and armour as big figures either side of the portrait at the bottom centre, each with a
//     thin bar over it; the rounds small under the crosshair; the guns and the four ammunition
//     counts down the right edge; the powerups and their seconds across the top.
//   - TACTICAL: the hero and tactical shooters' look: health's figure bottom left beside its bar
//     with the armour as a segmented shield bar over it; the rounds big bottom right beside the
//     gun in hand, the four ammunition kinds over them and the guns as slots over those; the
//     powerups bottom centre.
//   - MINIMAL: figures only, small and a little faded: health and armour bottom left, the rounds
//     bottom right, the keys and the powerups only while held.
//
// THE OPTIONS, each a cvar: `hud_sbar 0` draws the classic look's pieces without its two panels
// (the see-through bar); `hud_swap 1` puts every look but the classic one on the other side of the
// screen (the engine's mirrored layout); `hud_guns ammo|pairs|row` groups the guns (Grouping;
// `look`, the default, each look's own); `hud_guns_all 0` shows only the guns owned and
// `hud_ammo_all 0` only the ammunition in hand. The armour's icon shows its type in every look.
// While the classic bars show, the weapon in hand is raised over them (`status_lift`).
//
// `viewsize` steps the bars away as the classic game did: at 100 or under every element shows; at
// 110 the classic look drops its inventory bar (the status bar alone); at 120 every look drops its
// status elements (the bar, the corners, the arcs), leaving the crosshair, the feed and the centre
// message.
//
// Every look also shows the crosshair, the notify feed (pickups and frags, top left) and the
// centre message. A look draws with the HUD's ART (art.hpp): the free set, or the player's own
// classic pictures (its digits, icons, portrait and guns), only placed and sized anew.
//
// The figures' colours say what the classic bar said: a figure in the alarm colour is low
// (health or armour 25 or under, ammunition 10 or under); the shield shows "666" over its disc.
//
// ge:summary Shooter's Edge's HUDs: the classic status bar and five other looks, hud_style
// ge:keywords hud shooter status-bar inventory-bar classic modern compact hud_style looks hud_sbar
// ge:keywords sbar-background transparent arena tactical minimal shield-bar loadout hud_swap
// ge:keywords hud_guns hud_guns_all hud_ammo_all grouping pairs status_lift lift health armour ammo
// ge:keywords face portrait weapons keys powerups runes frags crosshair notify centre-message ring
// ge:keywords arcs fill_hud add_cvars layout elements viewsize
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

enum class Look : uint8_t { classic, modern, compact, arena, tactical, minimal };
inline constexpr int kLooks = 6;

// How a look lays out its guns (`hud_guns`): `ammo` two to a line by the ammunition they share
// (the beam gun, as wide as two, alone), each line led by that ammunition's count and icon;
// `pairs` the same lines with the counts kept apart (the look's stock); `row` one row, or one
// column where the look stands them down an edge.
enum class Grouping : uint8_t { ammo, pairs, row };
std::string_view grouping_name(Grouping g);
// The grouping a word names; `fallback` for a word that names none.
Grouping grouping_from(std::string_view word, Grouping fallback);

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
  int32_t sbar = 1;           // hud_sbar: the classic look's two panels drawn (0: its pieces alone)
  int32_t swap = 0;           // hud_swap: every look but the classic one on the other side
  std::string guns = "look";  // hud_guns: ammo, pairs or row; look: each look's own
  int32_t guns_all = 1;       // hud_guns_all: every gun, those not owned dim (0: owned ones)
  int32_t ammo_all = 1;       // hud_ammo_all: all four counts (0: the kind in hand alone)
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

// The share of a `height` px screen the classic status bars covered when last drawn (0 with
// hud_sbar 0, another look, or the bars hidden): what the weapon in hand is raised by, so it
// shows over the bars as a view drawn above them would (render/viewmodel.hpp's `lift`).
float status_lift(const Hud& hud, int height);

}  // namespace shooters_edge::hud

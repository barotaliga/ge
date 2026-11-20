// THE CROSSHAIR as the player sets it, for any game with one: which mark (`crosshair`: 0 none, 1
// the game's own, 2..7 the built marks below), its colour (`crosshaircolor`: empty the mark's own,
// one number a colour of the game's palette, or "R G B" 0..255), its size (`crosshairsize`, times
// the mark's own) and how solid it is (`crosshairalpha`). The built marks are painted white in
// code, so the colour tints them as given; the game's own keeps its picture's colours under the
// tint.
//
//   2 a dot          3 a cross       4 a cross with a gap at the middle
//   5 a ring         6 a ring and a dot          7 a cross turned on its side (an X)
//
// ITS PLACE: `cl_crossx` and `cl_crossy` move it from the view's middle, right and down, in the
// HUD's art pixels (a classic screen's pixel). A game may tint it further by its own shares of red,
// green and blue (`shade`), or colour it by the player's health (`by_health`: white at full,
// yellow at two thirds, red at a third and below, black dead).
//
// A GAME'S DEFAULTS: the settings it registers hold them (`add_crosshair_cvars` takes the values
// they have at the call as the cvars' defaults), so each game keeps its own.
//
// ge:summary The crosshair's settings for any game: the mark, its colour, size and opacity, the
// marks painted in code, the picture a screen draws
// ge:keywords crosshair crosshaircolor crosshairsize crosshairalpha crosshair-style
// ge:keywords crosshair-marks palette-index crosshair_picture cl_crossx cl_crossy health-colour
// ge:skill ui
// ge:test tests/hud_crosshair_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "console/cvar.hpp"
#include "hud/pictures.hpp"
#include "ui/image.hpp"

namespace ge::hud {

inline constexpr int32_t kCrosshairMarks = 7;  // the highest `crosshair`

struct CrosshairSettings {
  int32_t mark = 1;             // crosshair
  std::string colour;           // crosshaircolor ("": the mark's own colours)
  double size = 1, alpha = 1;   // crosshairsize, crosshairalpha
  double offset[2] = {0, 0};    // cl_crossx, cl_crossy: art pixels right and down
  double shade[3] = {1, 1, 1};  // a game's own shares of red, green and blue (1: the colour's)
  bool by_health = false;       // a game's: the colour the player's health (crosshair_health)
  // The loaded game's own meaning of `crosshair` (docs/porting-games.md § One name, one meaning):
  // its number picks one of the game's own pictures (1..N), never a built mark. The game's layer
  // binds `crosshair` to `mark` and sets this; the caller hands picture `mark` as `own`.
  bool pictures = false;
};

// Registers crosshair, crosshaircolor, crosshairsize, crosshairalpha, cl_crossx and cl_crossy over
// `s` (it must outlive `cvars`), each archived, its default the value `s` holds now: a game sets
// its own first.
void add_crosshair_cvars(console::Cvars& cvars, CrosshairSettings& s);

// The colour `text` names: one number an index into `palette` (256 RGB triples; empty: a grey of
// that level), or "R G B" 0..255; white (no tint) for nothing or anything else.
ui::Color crosshair_colour(std::string_view text, std::span<const uint8_t> palette);

// Built mark `mark` (2..kCrosshairMarks), painted white at kMarkDensity pixels per art pixel; an
// empty picture for any other.
inline constexpr float kMarkDensity = 4;
const ui::Image& crosshair_mark(int32_t mark);

// The crosshair as a picture for any game's screen: the built mark `s` picks, or `own` (the game's
// own crosshair, `own_density` of its pixels an art pixel) for mark 1 (any mark but 0 when
// `s.pictures`: the game's picture of that number), at `s`'s size, tinted by
// its colour (palette numbers from `palette`) at its alpha. `*shown`: false when nothing would
// show (mark 0, alpha 0, no picture). `health`: the player's, for `s.by_health`.
Picture crosshair_picture(const CrosshairSettings& s, const ui::Image& own, float own_density,
                          std::span<const uint8_t> palette, bool* shown, int health = 100);

// The colour a crosshair coloured by health takes: red always; green from none at 33 to full at
// 66; blue from none at 66 to full at 99; black at 0 and below.
ui::Color crosshair_health(int health);

}  // namespace ge::hud

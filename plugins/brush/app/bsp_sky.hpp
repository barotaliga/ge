// A CLASSIC MAP'S SKY AS OURS (the v2 BSP renderer, bsp_draw.hpp): our sky picked to suit the
// map's own sky texture. The texture's colours (draw_data.hpp's `SkyColours`) say what kind of sky
// it is: its hue picks a sky type (render/sky_style.hpp: a red smoke is a hellish air, a violet
// dusk a nebula's, a green haze a toxic one), its brightness the hour and how bright the sky is
// against the rooms (a dark sky a dusk, veiled dim over torch-lit halls), its cloud layer the
// heaps' cover, and the type's air is tinted toward the texture's colour so the sky through the sky
// faces reads as the map's.
//
// THE CLASSIC NIGHT (`night_over`, on by default under v2): a classic map is never seen by day or
// in the morning. Over the pick's type and tint each map gets an hour of its own, dealt off its
// name: an EVENING (the sun 3 to 6 degrees under the horizon, the twilight's glow on one side and
// a full moon rising on the other) or a NIGHT (the sun far down, the moon 25 to 50 degrees up and
// more than half lit). It is always windy (8 to 14 m/s at 10 m) and always partly cloudy with LOW
// cloud: ragged scud a few hundred metres over the map's top racing across the moon on the
// evening's low-level jet (wind/aloft.hpp), under a broken deck of rolls with clear sky between.
// The eye stays adapted to the rooms (`SkyPick::adapt_cd`), so the torch-lit halls keep their
// light and the lava its colour, and the hour, not the veil, darkens the sky through the sky faces.
// `dress_sky` puts it all in a frame's view, the layers drifting on the wind at the frame's time.
//
// ge:summary Our sky picked by a classic map's sky texture: its type, light, tint and cloud
// ge:keywords bsp brush sky sky-type match pick_sky sky-texture hue dusk cover v2 night_over
// ge:keywords classic-night evening moon scud low-cloud wind dress_sky clouds_of adapt
// ge:skill brush render
// ge:test tests/brush_bsp_sky_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "brush/app/draw_data.hpp"
#include "render/sky_style.hpp"
#include "render/view.hpp"

namespace ge::brush::app {

struct SkyPick {
  std::string type = "earth";  // the sky type it starts from (render/sky_style.hpp)
  render::SkyStyle style;      // that type, its air tinted toward the texture's colour
  double sun_elevation = 30;   // degrees
  double sky_light = 1;        // the sky against the rooms (Options::sky_light, r_skylight)
  double sky_seen = 1;         // the sky as seen through the sky faces (Options::sky_seen)
  double tint[3] = {1, 1, 1};  // the sky's tint toward the texture's hue (Options::sky_tint)
  double cover = 0.3;          // the heaps' share of the sky
  double cell_m = 400;         // a heap's span, m (the classic's small clouds)
  std::string why;             // one line: what in the texture chose it (and the night's hour)
  // THE HOUR AND THE AIR (night_over). By day the sun stands at `sun_elevation` and one deck of
  // heaps of `cover` is overhead, still.
  bool night = false;
  double sun_azimuth = 220;     // degrees, a world heading
  double moon_elevation = -10;  // degrees
  double moon_azimuth = 0;      // degrees, a world heading
  double moon_lit = 1;          // the share of its disc lit
  double sidereal = 0;          // the stars' turn, degrees (the local sidereal angle)
  double wind = 0;              // m/s, 10 m over the map's top
  double wind_from = 0;         // the bearing it blows from, rad (heading 0 = +z)
  double scud_cover = 0;        // the scud's share of the sky
  double deck_cover = 0;        // the broken deck's
  uint32_t seed = 0;            // where in the noise the map's layers start
  double adapt_cd = 0;          // the rooms' light the eye stays adapted to, cd/m^2 (FrameView's)
};

// Our sky for a map whose sky texture has `colours` (no texture: the Earth's fair day).
SkyPick pick_sky(const SkyColours& colours);

// `pick` with the type `name` in place of its own (its tint and hour kept); false for an unknown
// word.
bool with_type(SkyPick& pick, std::string_view name);

// The classic night over `pick` for the map named `map` (its hour, moon, wind and low cloud
// dealt off the name, so a map always gets the same night). The type and tint are kept; the sky's
// light against the rooms goes to 1, the hour doing the dimming.
void night_over(SkyPick& pick, std::string_view map);

// The layers over a map whose highest point is at world `top_y`, at time `t` s: by day the pick's
// one still deck of heaps (`cover`, at 1.5 to 2.3 km); at night the scud and the deck, each
// carried on the wind aloft at its height and the evening's jet.
render::Clouds clouds_of(const SkyPick& pick, double top_y, double t);

// The pick in `view` at time `t`: the type, the sun, the moon and the stars, the layers and the
// eye's adaptation.
void dress_sky(render::FrameView& view, const SkyPick& pick, double top_y, double t);

}  // namespace ge::brush::app

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
// evening's low-level jet (wind/aloft.hpp), under one layer the map deals (broken rolls, a
// rippled middle sheet or high ice), never more than a share of the sky.
//
// THE CLOUD BY DAY AND BY NIGHT is a map's own STACK of kinds (render/cloud_kinds.hpp), dealt off
// its name so no two maps' skies are alike: each layer in ISLANDS (its cells gathered, wide open
// sky between), its cells' outlines WARPED (leaning and twisting, never a field of matching
// domes), its cells' span dealt across the kind's band, and drifting on the wind.
// The eye stays adapted to the rooms (`SkyPick::adapt_cd`), so the torch-lit halls keep their
// light and the lava its colour, and the hour, not the veil, darkens the sky through the sky faces.
// `dress_sky` puts it all in a frame's view, the layers drifting on the wind at the frame's time.
//
// THE FAR LAND (render/far_land.hpp, `far_land_for`): the land round the map seen through its sky
// faces from high up, a kind dealt off its name among those that suit its sky's type (the Earth's
// air one of the Earth's eight lands, an alien air an alien one), standing round its box
// (`far_site_of`).
//
// ge:summary Our sky picked by a classic map's sky texture: its type, light, tint and cloud
// ge:keywords bsp brush sky sky-type match pick_sky sky-texture hue dusk cover v2 night_over
// ge:keywords classic-night evening moon scud low-cloud wind dress_sky clouds_of adapt
// ge:keywords dark_sky own-hour day-or-night at_hour sv_hour time-of-day sun-position
// ge:keywords cloud-stack sky-layer set_cover warp islands
// ge:keywords far-land far_land_for far_site_of r_farland skybox 3d-skybox
// ge:skill brush render
// ge:test tests/brush_bsp_sky_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "brush/app/draw_data.hpp"
#include "render/cloud_kinds.hpp"
#include "render/far_land.hpp"
#include "render/sky_style.hpp"
#include "render/view.hpp"

namespace ge::brush::app {

// ONE LAYER OF A MAP'S SKY: a kind of cloud (render/cloud_kinds.hpp) and what the map dealt it.
struct SkyLayer {
  const render::CloudKind* kind = nullptr;
  double cover = 0;  // the share of the sky its cells cover before its patches thin it
  double patch = 0;  // how far its cells gather into islands with clear sky between (0 .. 1)
  double warp = 0;   // how far its cells' outlines lean and twist, cells (CloudLayer::warp)
  double cell = 1;   // its cells' span, a share of the kind's own band
};
inline constexpr int kSkyLayers = 3;

struct SkyPick {
  std::string type = "earth";  // the sky type it starts from (render/sky_style.hpp)
  render::SkyStyle style;      // that type, its air tinted toward the texture's colour
  double sun_elevation = 30;   // degrees
  double sky_light = 1;        // the sky against the rooms (Options::sky_light, r_skylight)
  double sky_seen = 1;         // the sky as seen through the sky faces (Options::sky_seen)
  double tint[3] = {1, 1, 1};  // the sky's tint toward the texture's hue (Options::sky_tint)
  double cover = 0.3;          // the texture's own cloud share (0..1): how cloudy the map's sky is
  double bright = 1;           // the texture's luminance as seen (0..1): a day's sky or a dark one
  std::string why;             // one line: what in the texture chose it (and the night's hour)
  // THE HOUR AND THE AIR (night_over). By day the sun stands at `sun_elevation`.
  bool night = false;
  double sun_azimuth = 220;     // degrees, a world heading
  double moon_elevation = -10;  // degrees
  double moon_azimuth = 0;      // degrees, a world heading
  double moon_lit = 1;          // the share of its disc lit
  double sidereal = 0;          // the stars' turn, degrees (the local sidereal angle)
  double wind = 0;              // m/s, 10 m over the map's top
  double wind_from = 0;         // the bearing it blows from, rad (heading 0 = +z)
  uint32_t seed = 0;            // where in the noise the map's layers start
  double adapt_cd = 0;          // the rooms' light the eye stays adapted to, cd/m^2 (FrameView's)
  // THE CLOUD: the map's own stack of kinds, lowest first, dealt off its name (by day a low
  // layer under the texture's share, sometimes scud under it and ice or a rippled sheet high
  // over it; at night the scud and one layer over it), each layer in islands with open sky
  // between and its cells' outlines twisted.
  SkyLayer layers[kSkyLayers];
  int layer_count = 0;
};

// Our sky for a map whose sky texture has `colours` (no texture: the Earth's fair day); its
// wind and its day's cloud dealt off the map's name `map` (a map always gets the same sky).
SkyPick pick_sky(const SkyColours& colours, std::string_view map = {});

// A layer of the kind `name` (render/cloud_kinds.hpp) on top of the pick's stack (none past
// kSkyLayers): its cover, its islands, its warp and its cells' span as a share of the kind's.
void add_layer(SkyPick& pick, const char* name, double cover, double patch = 0, double warp = 0,
               double cell = 1);

// Every layer's cover scaled so the stack's lowest layer covers `cover` (0 a clear sky).
void set_cover(SkyPick& pick, double cover);

// `pick` with the type `name` in place of its own (its tint and hour kept); false for an unknown
// word.
bool with_type(SkyPick& pick, std::string_view name);

// The classic night over `pick` for the map named `map` (its hour, moon, wind and low cloud
// dealt off the name, so a map always gets the same night). The type and tint are kept; the sky's
// light against the rooms goes to 1, the hour doing the dimming.
void night_over(SkyPick& pick, std::string_view map);

// THE MAP'S OWN HOUR: a map whose sky texture is bright (the classic renderer draws it as a day
// sky: the violet-blue one) is seen by day under `pick`; one whose sky is dark (the red-brown
// smoke, under kDarkSky of the white) under its classic night.
inline constexpr double kDarkSky = 0.05;
bool dark_sky(const SkyPick& pick);

// THE HOUR SET (sv_hour, `at_hour`): the sun where it stands at `hour` (0 .. 24) on a summer's day
// at the latitude the night's stars turn over (46 N, the sun 15 degrees north of the equator): up
// at about 5, 59 degrees high at noon in the south, down at about 19. By day the pick's own type,
// tint and cover keep; once the sun is 6 degrees under the horizon the hour is `night` (the map's
// own classic night, its moon on the far side of the sky from the sun).
void at_hour(SkyPick& pick, const SkyPick& night, double hour);
// The layers over a map whose highest point is at world `top_y`, at time `t` s: the pick's
// stack, each layer at its kind's height over the top, carried on the wind aloft at its height
// (and at dusk the evening's jet).
render::Clouds clouds_of(const SkyPick& pick, double top_y, double t);

// The pick in `view` at time `t`: the type, the sun, the moon and the stars, the layers and the
// eye's adaptation.
void dress_sky(render::FrameView& view, const SkyPick& pick, double top_y, double t);

// THE FAR LAND for a map named `map` under `pick` (r_farland's `word`): "auto" deals a kind off the
// name among those that suit the sky's type (the Earth's air one of the Earth's eight; the dust's
// the desert's; an alien, violet, toxic or nebula air the violet meadows; a hellish or ashen one
// the lava's; an icy one the frozen moon's; a black sky, the airless type's, the Earth's as a
// plain night's), "off" none, a kind's name that kind. Null
// for none and for a word that names no kind (`known` false then).
const render::FarLandKind* far_land_for(const SkyPick& pick, std::string_view map,
                                        std::string_view word = "auto", bool* known = nullptr);
// Where a map stands for its far land: its box's middle on the plan, the circle round its box a
// few metres out, its lowest point, a seed off its name.
render::FarSite far_site_of(const Scene& scene, std::string_view map);

}  // namespace ge::brush::app

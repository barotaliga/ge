// THE WEATHER OVER A BSP MAP (docs/rendering.md § Precipitation): the wind, rain, snow and storms
// a map's open sky lets in, turned by cvars and stepped by the frames' clock into what the frame
// (render/view.hpp's Weather), the BSP renderer (bsp_draw.hpp's `Options::weather`) and the fall
// (render/precip.hpp) draw. The open sky and the windows decide where it reaches: the renderer's
// map along the fall keeps the rain, the snow, the splashes, the wet and the puddles to what the
// sky reaches the way the wind drives it, so a courtyard is soaked, the floor inside a window wet
// as far as the rain slants in, and a room behind a wall dry.
//
//   sv_weather      0/1    the weather on a map at all
//   sv_precip       0..1   how hard it falls (engine/wind/precip.hpp's dial: light, moderate,
//                          heavy, a storm)
//   sv_rain         0..1   the share that is rain (0 dry snow, 0.5 sleet, 1 rain)
//   sv_wind         m/s    the mean wind, 10 m over open ground
//   sv_winddir      deg    the bearing it blows FROM (0 the map's +z, clockwise from above)
//   sv_gust         0..1   the gusts' strength (the speed's deviation over its mean)
//   r_precip        0/1    the falling rain and snow drawn
//   r_precip_density 0.25..2  the particles a full fall draws, a share of the default
//   r_splashes      0/1    the drops' splashes
//   r_wet           0/1    the wet faces and the puddles
//   r_snowcover     0/1    the snow settling on what faces up
//   r_lightning     0/1    a storm's lightning
//
// The wet ground and the snow's cover are one state for the whole map (`WeatherState`), stepped
// from the steady state the first frame's rain would leave; where they show is the renderer's.
//
// ge:summary The weather over a BSP map: cvars, the wet and the snow stepped, the fall's frame
// ge:keywords weather bsp rain snow sleet storm wind lightning puddles wet cvars sv_weather
// ge:keywords sv_precip sv_rain sv_wind sv_winddir sv_gust r_precip r_splashes r_wet r_snowcover
// ge:keywords r_lightning add_weather_cvars step_weather apply_weather
// ge:skill brush render
// ge:test tests/brush_weather_test.cpp
#pragma once

#include <cstdint>

#include "brush/app/bsp_draw.hpp"
#include "console/cvar.hpp"
#include "render/precip.hpp"
#include "render/view.hpp"
#include "wind/precip.hpp"

namespace ge::brush::app {

// The values the cvars write, in their own units.
struct WeatherCvars {
  bool on = false;
  double precip = 0.5, rain = 1;
  double wind = 4, wind_from = 210, gust = 0.25;
  bool draw = true;
  double density = 1;
  bool splashes = true, wet = true, snowcover = true, lightning = true;
};

// Register the weather's cvars on `cvars` (sv_ the server's, r_ the player's), bound to `out`.
void add_weather_cvars(console::Cvars& cvars, WeatherCvars& out);

// The state kept between frames.
struct WeatherState {
  wind::Wet wet;
  wind::Cover cover;
  double t = 0;
  bool placed = false;  // false: the next step starts at the steady state
};

// One frame's weather.
struct WeatherFrame {
  bool on = false;
  wind::Precip fall;  // the dial and the rain share
  wind::Tier tier = wind::Tier::none;
  double air[3] = {0, 0, 0};   // the wind's velocity, m/s
  double gust = 0.5;           // the gusts' share now
  double way[3] = {0, -1, 0};  // the way the fall comes (unit, down and downwind, 75 deg at most)
  double spread = 0.1;         // the scatter round the way: eddies over pace (a tangent)
  double rate = 0;             // mm/h of water
  double visibility = 20000;   // m
  double flash = 0;
  wind::Wet wet;
  wind::Cover cover;
  render::PrecipFrame draw;  // the fall's frame (its eye and dt the caller's)
};

// Step the weather to time `t` s (the eye at `eye`, engine m; the sun's share on the open ground,
// 0 .. 1, for the drying and the melt). A jump back in time, or a first frame, starts the wet and
// the cover from the steady state of a long spell of this weather (the cover from an hour of it).
WeatherFrame step_weather(const WeatherCvars& c, WeatherState& s, const double eye[3], double t,
                          double sun);

// The frame's weather on a view (its wet, puddles, snow and flash; the fog of the fall when
// `fog` is set) and on the renderer's options.
void apply_weather(const WeatherFrame& w, const WeatherCvars& c, render::FrameView& view,
                   Options& options, bool fog = true);

// The particles a full fall draws at `density` (r_precip_density) of `base`.
uint32_t weather_particles(const render::PrecipSettings& base, double density);

}  // namespace ge::brush::app

// THE CLOUD OVER THE MOUNTAIN: which layers each of the mountain's broken skies (R19's
// `WeatherKind`) stands over a map, at what heights, and how the wind aloft carries them
// (render/view.hpp's `Clouds`; render/clouds.hpp draws them).
//
// - FAIR: heaps 1.5–2 km over the valley floor, 500–900 m deep, over a fifth to a third of the
//   sky, and a few ice streaks at 7.6–8.8 km.
// - FLURRIES: taller heaps (1.3–2 km deep) over a third of the sky, and a rippled sheet of small
//   cells at 3.6–4.5 km under the high air.
// - HIGH: ice streaks over a third to a half of the sky at 7.5–9 km, under a thin veil over most
//   of it.
// - CLEAR has none, and a LID (overcast, snow, storm, fog) keeps its deck (weather_look.hpp).
// Every band (the base, the depth, the cover, the cell) is dealt off the map's seed on a hash of
// its own, never a run's random stream. THE WIND ALOFT is the map's mean wind grown with height
// on a log law, turned with it (clockwise in the north, the other way in the south), floored at
// the speed each kind of cloud is seen to move at; the layers drift at a quarter of it (a real
// cloud's shadow at a game's lens reads as a time-lapse; a quarter crosses a slope in ten minutes),
// and a cell lives its kind's life (a heap's eight minutes, a veil's half hour).
//
// ge:summary Which cloud layers a broken sky stands over a map, and how the wind carries them
// ge:keywords cloud field layers cumulus congestus altocumulus cirrus veil wind-aloft veer
// ge:keywords drift cloud_field cloud_layers_for sky_at
// ge:skill mountain render
// ge:test tests/mountain_cloud_field_test.cpp
#pragma once

#include "mountain/mapgen/types.hpp"
#include "render/view.hpp"

namespace ge::mountain::app {

// How fast the layers drift, a share of the wind aloft that carries them.
inline constexpr double kCloudDrift = 0.25;

// The wind aloft at `h` m over the floor from a surface mean of `surface` m/s, floored at
// `least` and held under 45 m/s; and how far it has turned by then, rad (positive clockwise).
double wind_aloft(double surface, double h, double least);
double veer_aloft(double h, double latitude);

// The layers `weather` stands over a map of `seed` whose valley floor is at world `floor_y`, at
// the map's moment `t` (s).
render::Clouds cloud_layers_for(const mapgen::Weather& weather, double seed, double floor_y,
                                double latitude, double t);

// The same over `level` at `t`.
render::Clouds cloud_field(const mapgen::Level& level, double t);

// THE SKY AT A MOMENT, drawn: the weather (weather_look.hpp's, with the share of the sun a broken
// sky's mean cloud takes given back, since each layer now casts its own shadow) and the layers.
void sky_at(const mapgen::Level& level, double t, render::Weather& weather, render::Clouds& clouds);

}  // namespace ge::mountain::app

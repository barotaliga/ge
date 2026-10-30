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
// - OVERCAST: a low grey stratus over all of the sky; SNOW and STORM: the thick nimbostratus deck.
//   Each keeps its deck (weather_look.hpp), which lights the ground; the layer is its texture.
// - CLEAR and FOG have none.
// Every band (the base, the depth, the cover, the cell) is dealt off the map's seed on a hash of
// its own, never a run's random stream. THE WIND ALOFT is the map's mean wind grown with height
// on a log law, turned with it (clockwise in the north, the other way in the south), floored at
// the speed each kind of cloud is seen to move at; the layers drift at the true wind (M8f9: at a
// quarter of it, as before, a low cloud hung still where a real one races; `sv_cloudspeed 0.25`
// is the old sky), and a cell lives its kind's life (a heap's eight minutes, a veil's half hour).
// THE EVENING'S LOW-LEVEL JET: once the sun is down the ground cools, the air over it stops
// mixing with the air aloft, and the wind a few hundred metres up speeds up past its day's value
// (up to about 1.7 times it at 300 to 500 m, peaking after midnight). A layer's wind grows by that
// as dusk falls (`Aloft::dusk`). With a wind of 2 m/s or more, a fair, flurrying, grey or snowing
// sky also gets ragged scud racing under it at 200 to 450 m (`fractus`) from twilight into the
// night. The scud is so near that it crosses the sky many times faster than the layers over it, and
// it blows across a lit moon. A layer's height and its speed can be picked by hand (`Aloft::base`,
// `Aloft::speed`): every height is a true one over the floor, since the layers are marched on the
// eye's rays and need no world under them; a lower layer moves faster across the sky for the same
// wind.
//
// ge:summary Which cloud layers a broken sky stands over a map, and how the wind carries them
// ge:keywords cloud field layers cumulus congestus altocumulus cirrus veil wind-aloft veer
// ge:keywords drift cloud_field cloud_layers_for sky_at low-level-jet scud fractus dusk aloft
// ge:keywords cloud-height cloud-speed
// ge:skill mountain render
// ge:test tests/mountain_cloud_field_test.cpp
#pragma once

#include "mountain/air/air.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/cloud_kinds.hpp"
#include "render/view.hpp"
#include "wind/aloft.hpp"

namespace ge::mountain::app {

// How fast the layers drift, a share of the wind aloft that carries them: all of it.
inline constexpr double kCloudDrift = 1;

// What a sky's layers are dealt under beside the map: how far dusk has fallen (0 by day, 1 with
// the sun 6 degrees under the horizon; `dusk_of`), a share of the TRUE wind aloft they drift at
// in place of `kCloudDrift` (negative: kCloudDrift), the lowest layer's base over the floor, m,
// every layer over it raised or lowered in the same proportion (negative: the dealt heights), and
// the lowest layer's THICKNESS, how tall it stands from its base to its top, m (negative: the
// dealt depth). A layer's cloud is as dense a metre whatever its thickness, so a taller one is
// darker below and hides more of the sun. However they are dealt, every layer starts where the
// one under it ends: from a layer's top up there is clear sky or the next layer, never two
// layers in one slab of air.
struct Aloft {
  double dusk = 0;
  double speed = -1;
  double base = -1;
  double thickness = -1;
};

// The wind aloft and the evening's jet (wind/aloft.hpp), as the mountain's layers read them.
using ge::wind::dusk_of;
using ge::wind::jet_at;
using ge::wind::veer_aloft;
using ge::wind::wind_aloft;

// The layers of the `count` kinds `kinds` (render/cloud_kinds.hpp, lowest first) over a map of
// `seed` whose valley floor is at world `floor_y`, under `weather`'s wind, at the map's moment
// `t` (s): each kind's bands dealt off the seed as a broken sky's are.
render::Clouds cloud_layers_of(const render::CloudKind* const* kinds, int count,
                               const mapgen::Weather& weather, double seed, double floor_y,
                               double latitude, double t, const Aloft& aloft = {});

// The weather under `count` kinds of cloud picked by hand: a LID among them (a full, thick sheet
// over the valley floor: stratus, nimbostratus) turns `weather` to a deck's, the sky's light on
// the ground grey and the darker the thicker the lid.
void lid_under(const render::CloudKind* const* kinds, int count, render::Weather& weather);

// The layers `weather` stands over a map of `seed` whose valley floor is at world `floor_y`, at
// the map's moment `t` (s); from twilight, with scud under them (above).
render::Clouds cloud_layers_for(const mapgen::Weather& weather, double seed, double floor_y,
                                double latitude, double t, const Aloft& aloft = {});

// The same over `level` at `t`.
render::Clouds cloud_field(const mapgen::Level& level, double t, const Aloft& aloft = {});

// THE SKY AT A MOMENT, drawn: the weather (weather_look.hpp's, with the share of the sun a broken
// sky's mean cloud takes given back, since each layer now casts its own shadow) and the layers.
// With `layers` off (the quality ladder's bottom), no layer is drawn and the weather keeps the
// broken sky's mean cloud.
// `eye_y` is the eye's world height, m: under the map's snow line the fall it looks through is
// rain (air::fall_at).
void sky_at(const mapgen::Level& level, double t, render::Weather& weather, render::Clouds& clouds,
            bool layers = true, double eye_y = air::kAloft, const Aloft& aloft = {});

}  // namespace ge::mountain::app

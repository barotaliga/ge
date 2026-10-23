// THE WEATHER IN THE PICTURE: what each of the mountain's skies (R19's `WeatherKind`, at the
// moment's fall from air/air.hpp) is in the renderer's numbers (render/view.hpp's `Weather`).
//
// - A CLEAR sky changes nothing; FAIR heaps and a FLURRY's broken cloud cross the sun now and
//   then (a fifth of its light gone on the whole) and whiten the blue a little; a HIGH veil takes
//   more of the sun and milks a fifth of the blue. Where those layers are drawn (cloud_field.hpp)
//   the cloud is theirs, its shadow each place's own.
// - A LID (OVERCAST, a steady SNOW, a STORM, a FOG) is a deck over the whole dome: the sun a pale
//   patch behind it (an eighth of its light through an overcast, a twenty-fifth through a storm),
//   the light it spreads most of the clear day's again (more under a thin overcast or in a fog,
//   half under a storm's slate), darker overhead than at the horizon. That is the FLAT LIGHT: no
//   shadow, no glitter, the snow's relief read only off the sky.
// - THE FOG closes the view at a visual range: a lid's damp air some 3.3 km (an overcast) or
//   2.3 km (a fall's deck), a valley fog's 1.2 km thinning to 130 m at its thickest, and in a fall
//   the fall's own (air::visibility_in: a flurry's half kilometre, a blizzard's forty-five metres),
//   whichever is shorter. A fall also hides the deck overhead behind the snow between (as much as
//   the 600 m of air up to a deck's base takes), so a blizzard's sky is one grey with the ground.
// - The fall and the fog take their share off the sun as well: up to 60 % each at their worst.
//
// ge:summary The mountain's weather kinds as the renderer's cloud, deck, flat light and fog
// ge:keywords weather look overcast deck lid flat-light fog visibility sun-pass cover veil
// ge:keywords weather_look blizzard storm flurries haze sky
// ge:skill mountain render
// ge:test tests/mountain_weather_look_test.cpp
#pragma once

#include "mountain/air/air.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/view.hpp"

namespace ge::mountain::app {

// The renderer's weather for a map's sky with `fall` in the air (air::fall_at at the moment).
// With `layers` (the broken sky's cloud drawn as layers, cloud_field.hpp), a broken sky's mean
// cloud is no deck and takes nothing off the sun: each layer casts its own shadow.
render::Weather weather_look(const mapgen::Weather& weather, const air::Fall& fall,
                             bool layers = false);

// The same at the map's moment `t`, s.
render::Weather weather_at(const mapgen::Level& level, double t);

}  // namespace ge::mountain::app

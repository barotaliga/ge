// THE COLD'S MARKS ON THE MAP'S THINGS (docs/climate.md § Effects): what the map's day of air
// (air.hpp's `climate_of`) has laid on a surface by an hour, as pure functions of the map and the
// clock, for the village's look, the lake and the sound to read. Each runs engine/wind/cold.hpp's
// rates over the hours before the one asked about, from a bare surface, in quarter-hour steps.
//
// - FROST (`frost_on`): hoar frost (or rime, in a freezing fog with a wind) on a surface open to
//   the sky at `site`: over the 24 hours before `hour` each step adds what `deposit_on` lays and
//   takes what `frost_loss` takes, at the surface's own temperature (`surface_at`: a clear night
//   radiates it under the air, the sun warms it by day). A clear, calm night leaves a few tenths
//   of a millimetre by dawn and the morning's sun takes it by mid-morning; a cloudy or windy
//   night leaves little. `frost_cover` is the share of a surface it whitens: 0.25 mm hides it.
// - BLACK ICE (`glaze_at`): a road's clear glaze at run time `t`: over the 12 hours before, each
//   step of fall adds `glaze_on` of its rain at the road's surface, half an hour of steady rain
//   glazing it whole, and a road over 0 C melts it, a kelvin an hour. Rain onto a road below
//   freezing is FREEZING RAIN: the snow line high (a warm layer aloft, a sky picked by hand) over
//   a cold valley. 0 bare .. 1 a glaze across the road.
// - LAKE ICE (`lake_ice`): still water at `y` after `days` of winter at the map's day's mean,
//   lapsed to `y`: `ice_grown` from open water. `ice_bears` says what it carries.
//
// ge:summary The cold's marks on the map: frost by dawn, a road's black ice, the lake's ice
// ge:keywords frost hoar-frost rime black-ice glaze freezing-rain lake-ice frost_on frost_cover
// ge:keywords glaze_at lake_ice marks cold village road
// ge:skill mountain
// ge:test tests/mountain_marks_test.cpp
#pragma once

#include "mountain/mapgen/types.hpp"
#include "wind/climate.hpp"

namespace ge::mountain::air {

// Frost this thick, mm, whitens a surface wholly.
inline constexpr double kFrostHides = 0.25;

// Hoar frost (or rime) on a surface open to the sky at `site`, mm, at solar `hour`.
double frost_on(const mapgen::Level& level, const wind::Site& site, double hour);
// The share of a surface `mm` of frost whitens, 0..1.
double frost_cover(double mm);

// A road's black ice at `site` at run time `t` s and solar `hour` (`hour_at` when none is given),
// 0 bare .. 1 glazed across.
double glaze_at(const mapgen::Level& level, const wind::Site& site, double t, double hour);
double glaze_at(const mapgen::Level& level, const wind::Site& site, double t);

// The ice on still water at world height `y` m after `days` of winter at the map's day's mean, m.
double lake_ice(const mapgen::Level& level, double y, double days);

}  // namespace ge::mountain::air

// FOG FROM THE AIR (docs/climate.md § Fog): whether fog lies at a site at an hour, what kind, how
// thick and how deep, as a pure function of the air (climate.hpp). A world turns the answer into
// what it draws (an area of fog, render/fog.hpp's `FogVolume`: `density` is its extinction at the
// floor, `depth` twice its half height, `noise` its carving, `rise` its drift up) and into what
// its players see through (`visibility`). Five ways air becomes fog, each the physics that makes
// it:
//
// - RADIATION FOG: on a clear night the ground radiates to the sky and the air over it cools to
//   its dew point. Thickest where the air is moist (near water, over wet ground, a thaw's melting
//   snow lies at 0 C and saturated) and in still air; a breeze of 1 to 2 m/s mixes it deeper, past
//   5 m/s the wind mixes it away. It burns off as the morning sun warms the air past its dew point.
// - VALLEY FOG: the same fog in the night's cold pool, a quarter as deep as the hollow (to 150 m
//   more). A world lays it LEVEL, like water: its top is the floor's height plus `depth`, and the
//   slopes above that top stand clear in the sun (the valley's thermal belt).
// - HILL FOG: air carried up a mountain cools at 6.5 K a kilometre while its dew point falls at
//   1.8; where they meet the mountain is in its cloud, whatever the hour or the wind.
// - STEAM FOG (sea smoke): air far colder than open water (6 K and more) is saturated by what
//   the water steams off: thin wisps rising off the surface, indoors as outdoors, in wind or calm.
// - ADVECTION FOG: warm, moist air blown over a colder surface (a cold lake, a snowfield in a
//   thaw) cools from below to its dew point; it needs a wind to bring the air and lifts into low
//   cloud in a gale.
//
// A fog's water is what the air can hold: the same saturation lays a thicker fog at 5 C than at
// -15 C (the vapour pressure is four times higher). Fog below 0 C is FREEZING fog: its droplets
// are supercooled and freeze on what they touch (cold.hpp's rime).
//
// ge:summary Fog from the air: radiation, valley, hill, steam and advection fog, density and depth
// ge:keywords fog mist radiation-fog valley-fog hill-fog steam-fog sea-smoke advection-fog
// ge:keywords freezing-fog visibility extinction dew-point saturation burn-off mist_at mist_name
// ge:skill water render
// ge:test tests/wind_mist_test.cpp
#pragma once

#include <cstdint>

#include "wind/climate.hpp"

namespace ge::wind {

enum class MistKind : uint8_t { none, radiation, valley, hill, steam, advection };

// The kinds' words ("none", "radiation", "valley", "hill", "steam", "advection").
const char* mist_name(MistKind kind);

struct Mist {
  MistKind kind = MistKind::none;
  double strength = 0;        // 0 none .. 1 the thickest this kind lays
  double density = 0;         // 1/m: the extinction at its floor (the visual range is 3 / it)
  double depth = 0;           // m: from the ground (or the water) to its top
  double visibility = 20000;  // m: how far one sees inside it
  double noise = 0.5;         // how patchy: 0 an even bank .. 1 wisps
  double rise = 0;            // m/s: how fast its wisps climb (steam fog)
  bool freezing = false;      // below 0 C: supercooled, rimes what it touches
};

// Under this strength no fog is said to lie.
inline constexpr double kNoMist = 0.02;

// THE FOG at a site and an hour: the strongest of the five.
Mist mist_at(const Climate& c, const Site& s, double hour);

}  // namespace ge::wind

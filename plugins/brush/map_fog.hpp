// A CLASSIC MAP'S OWN FOG: the worldspawn's `fog` key (or `_fog`, or `airfog`), and the `fog`
// console command, which take the same words, read into one setting (`MapFog`):
//
// - THE WORDS, one to six numbers: `density`; `density grey`; `red green blue` (the density 0.05);
//   `density red green blue` (the common form); then the fog's `alpha` (the most of a surface it
//   hides, 1) and its `bias`. Colours are 0..1 as the map's author saw them (sRGB) and are not
//   clamped; nothing else is either. A density of 0 is no fog.
// - THE CURVE: a pixel `z` map units from the eye keeps e^-(t^2) of its colour, t = max(D x z -
//   bias, 0), D = density / 64, the rest the fog's colour, times `alpha` (so half the light is
//   fog at about 1066 units for the common 0.05). The bias starts the fog bias / D units out.
// - THE SKY: no distance, so a fixed share of the fog's colour over it: the worldspawn's `skyfog`
//   when it has one (`sky_share`; -1 when not), else the player's setting; times `alpha`.
//
// app/bsp_fog.hpp's `map_flat_fog` turns a setting into the renderer's `render::FlatFog` (metres,
// linear light); the renderer draws the curve per pixel (render/wgsl/frame.wgsl's `flat_fog`).
//
// ge:summary A classic map's own fog: the worldspawn's fog and skyfog keys, the fog command's
// ge:summary words, read into one setting
// ge:keywords brush fog map-fog worldspawn skyfog airfog fog-command flat-fog density exp2
// ge:keywords map_fog parse_fog fog_words
// ge:skill brush
// ge:test tests/brush_map_fog_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "brush/map.hpp"

namespace ge::brush {

struct MapFog {
  double density = 0;                  // as written: D is it over 64, per map unit; 0 none
  double colour[3] = {0.3, 0.3, 0.3};  // 0..1, sRGB as written
  double alpha = 1;                    // the most of a surface the fog hides
  double bias = 0;                     // D x z less this, floored at 0
  double sky_share = -1;               // the worldspawn's `skyfog`; -1: the player's setting
};

// The words of a `fog` key or command into `out` (its sky share kept): false, and `out`
// untouched, when they are not one to six numbers.
bool parse_fog(std::string_view words, MapFog* out);

// The map's own fog off its worldspawn (the first entity): `fog`, `_fog` or `airfog`, and
// `skyfog`. None (density 0) when it has no fog key or the key's words do not read.
MapFog map_fog(const Map& map);

// The words a `fog` command with no arguments answers: "density red green blue alpha bias".
std::string fog_words(const MapFog& fog);

}  // namespace ge::brush

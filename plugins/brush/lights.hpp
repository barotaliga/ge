// A BRUSH MAP'S LIGHT ENTITIES: every `light*` entity read once (where it is, how bright, how far
// it reaches, its colour by its kind or its `_color`, its style number, how it falls off, and the
// cone it is aimed in), and the same light as a bake's
// (`bake::Light`) at the strength the real-time path gives it. The renderer's point lights
// (`plugins/brush/app/draw_data`), the rebake (rebake.hpp) and the classic-enhance lightmap stage
// all read the lights here, so a light baked and a light drawn live are the same light.
//
// THE FALL-OFF (`delay`): 0 or none falls in a straight line to nothing at value / wait units (the
// real-time path matches it by an inverse square at half that reach); 1 as one over the distance,
// 2 as one over its square, 5 the same eased over 128 units round the source, and 3 not at all,
// each the light's value at 128 / wait units (`wait` 1 for 3) and matched there by an inverse
// square (`gain` against the straight one's); 4 is a floor of light under the rest, which the
// real-time path's own floor stands for, so it is skipped, as is a light that only says where a
// texture's lights go (`_surface`). A bright light of a falling kind would otherwise be lit as a
// straight one of its value, whose strength grows as the cube of it: a 2500 nearly 600 times a
// 300, the rooms white.
//
// THE SPOT: a light with `mangle` (yaw, pitch up, roll in degrees) or a `target` (the entity whose
// `targetname` it names) is aimed: full light inside its cone (`angle`, the whole cone in degrees,
// 40 when unset; `_softangle` the full-light part, else the cone's inner three quarters), none
// outside it.
//
// ge:summary A brush map's light entities read once, and each as a bake's light
// ge:keywords bsp brush light-entity light torch flame fluoro colour reach bake-light strength
// ge:keywords light_entities bake_light_of map_bake_lights delay falloff inverse-square spotlight
// ge:keywords mangle cone softangle color-key overexposed
// ge:skill brush
// ge:test tests/brush_lights_test.cpp
#pragma once

#include <vector>

#include "bake/scene.hpp"
#include "brush/map.hpp"

namespace ge::brush {

struct LightEntity {
  double pos[3] = {0, 0, 0};  // engine metres
  double value = 300;         // the entity's brightness (its `light` key)
  double reach = 7.5;         // m: where its light ends (value / wait map units)
  // Linear, luminance 1: a flame's (about 1900 K), a warm bulb's (2900 K) or a cool tube's.
  double colour[3] = {1, 1, 1};
  bool flame = false;  // a torch or a flame
  bool torch = false;  // a wall torch (a flame too)
  bool large = false;  // a large flame: a brazier's on the floor (a flame too)
  int style = 0;       // the entity's light style number
  // The light a metre off over a straight fall-off's of the same value (1 for a straight one).
  double gain = 1;
  double soft_m = 0;           // m: its light eased round the source over this (`delay` 5), 0 none
  bool spot = false;           // aimed: lit only inside its cone
  double dir[3] = {0, -1, 0};  // spot: the way it points (unit, engine frame)
  double cone_cos[2] = {-1, -1};  // spot: full inside the first's cosine, none outside the second's
};

// Every light entity of `map` with an origin, in the order the file lists them, but the floors
// (`delay` 4) and the surface lights' templates (`_surface`).
std::vector<LightEntity> light_entities(const Map& map);

// `light` as a bake's light: the real-time path's strength a metre off (the classic's light at half
// its reach, value / 2 of 128, matched by an inverse square, times its `gain`) times `strength`,
// reaching 1.5 times as far, its shadow soft over 0.2 m (or its `soft_m`), a spot when aimed.
bake::Light bake_light_of(const LightEntity& light, double strength = 1);

// Every light entity of `map` as a bake's light.
std::vector<bake::Light> map_bake_lights(const Map& map, double strength = 1);

}  // namespace ge::brush

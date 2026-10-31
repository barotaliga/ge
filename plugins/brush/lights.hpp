// A BRUSH MAP'S LIGHT ENTITIES: every `light*` entity read once (where it is, how bright, how far
// it reaches, its colour by its kind, its style number), and the same light as a bake's
// (`bake::Light`) at the strength the real-time path gives it. The renderer's point lights
// (`plugins/brush/app/draw_data`), the rebake (rebake.hpp) and the classic-enhance lightmap stage
// all read the lights here, so a light baked and a light drawn live are the same light.
//
// ge:summary A brush map's light entities read once, and each as a bake's light
// ge:keywords bsp brush light-entity light torch flame fluoro colour reach bake-light strength
// ge:keywords light_entities bake_light_of map_bake_lights
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
};

// Every light entity of `map` with an origin, in the order the file lists them.
std::vector<LightEntity> light_entities(const Map& map);

// `light` as a bake's light: the real-time path's strength a metre off (the classic's light at half
// its reach, value / 2 of 128, matched by an inverse square) times `strength`, reaching 1.5 times
// as far, its shadow soft over 0.2 m.
bake::Light bake_light_of(const LightEntity& light, double strength = 1);

// Every light entity of `map` as a bake's light.
std::vector<bake::Light> map_bake_lights(const Map& map, double strength = 1);

}  // namespace ge::brush

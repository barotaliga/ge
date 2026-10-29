// A BRUSH MAP'S LIGHT REBAKED (engine/bake): every lit face of a map (map.hpp) charted at a finer
// luxel than the file's own lightmaps, its light worked out again from the lights a caller hands
// (the map's light entities, the sun through its sky faces, the open sky), with bounced light
// coloured by each texture's mean colour and the light's direction a luxel, so a renderer can
// shade the textures' relief by where the light comes from. Liquids glow into the bake (lava
// strongly, slime and teleporters faintly) and sky faces let the sky in; neither is charted.
//
// THE RESULT (`Rebaked`) is an atlas of irradiance and direction and, for every vertex of the map,
// its place in that atlas. A cache keeps it as bytes (`write_rebaked`, `read_rebaked`): the
// irradiance as RGB9E5 (shared-exponent floats, 4 bytes a luxel; a GPU samples it as it is) and
// the direction as RGBA8 (x, y, z at 127.5 ± 127.5, w 255 where a face covers the luxel).
//
// ge:summary A brush map's lightmaps rebaked: finer luxels, colour bounce, the light's direction
// ge:keywords bsp brush rebake lightmap bake directional-lightmap gi bounce cache rgb9e5
// ge:keywords rebake_map write_rebaked read_rebaked luxel
// ge:skill brush render
// ge:test tests/brush_rebake_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "bake/bake.hpp"
#include "brush/map.hpp"

namespace ge::brush {

struct RebakeInput {
  // Each texture's mean colour, linear 0..1, one a Map::textures entry (missing: mid grey).
  std::vector<std::array<float, 3>> albedo;
  std::vector<bake::Light> lights;  // engine metres, the lightmap's units (bake/scene.hpp)
  float sky[3] = {0, 0, 0};         // the open sky's brightness through a sky face
};

struct RebakeOptions {
  double luxel_m = 0.25;        // the classic's are 16 map units, 0.4 m
  uint32_t atlas_width = 2048;  // luxels
  bake::Options bake;
};

struct Rebaked {
  uint32_t width = 0, height = 0;
  std::vector<uint32_t> rgb9e5;  // irradiance, a luxel
  std::vector<uint8_t> dir;      // 4 a luxel
  std::vector<float> lm;         // 2 a Map::vertices entry: its atlas 0..1 place; −1 uncharted
  bake::Stats stats;
};

// `map`'s light rebaked. False (with the reason) when the charts do not fit the atlas width.
bool rebake_map(const Map& map, const RebakeInput& input, const RebakeOptions& options,
                Rebaked* out, std::string* error);

// The irradiance packed as RGB9E5 and back (0 to 65408 a channel).
uint32_t pack_rgb9e5(const float rgb[3]);
void unpack_rgb9e5(uint32_t packed, float rgb[3]);

// The cache's bytes: "GERB", a version, the size, then the luxels and the vertices' places.
std::vector<uint8_t> write_rebaked(const Rebaked& baked);
bool read_rebaked(std::span<const uint8_t> bytes, Rebaked* out, std::string* error);

}  // namespace ge::brush

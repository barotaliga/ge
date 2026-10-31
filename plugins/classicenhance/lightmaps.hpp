// THE LIGHTMAP STAGE: each map's light worked out again and written into the cache for the
// renderer to read in place of the map's own.
//
// `light_map` hands one read map to the rebake (`brush::rebake_map`, plugins/brush/rebake.hpp: the
// baker in `engine/bake`) with each texture's mean colour (`texture_means`: the enhanced albedo
// when the textures stage made one, the pack's own pixels otherwise) and the map's light entities
// (`brush::map_bake_lights`), and writes `write_rebaked`'s bytes as `lightmaps/<map>.gerb`: the
// irradiance and its direction a luxel, and the light bounced alone, which the game adds under its
// live torches and lamps (bsp_draw.hpp's `rebake_bounce`).
//
// No sun and no open sky are baked: the game draws both live, at the map's own hour, and a baked
// sky would light a night map by day.
//
// ge:summary The lightmap stage: each map rebaked (bounce and direction) into the cache
// ge:keywords lightmap stage rebake light atlas cache map gerb bounce texture-means
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "brush/map.hpp"
#include "brush/rebake.hpp"

namespace ge::classicenhance {

// The stage's version: a rerun redoes the stage when it moves. 2: the rebake (GERB) in place of
// the map's own lightmaps packed into a picture.
inline constexpr int kLightmapStageVersion = 2;

// Every lightmap of `map` shelf-packed into one picture, at most `max_width` texels wide (the map's
// own light, for labs to show beside the rebake).
assets::Image pack_lightmaps(const brush::Map& map, int max_width = 1024);

// Each of `map`'s textures' mean colour in linear light: the cache's enhanced albedo
// (`<cache>/textures/<name>/albedo.png`) where there is one, else the map's own pixels through
// `palette` (768 bytes; empty: none), else mid grey.
std::vector<std::array<float, 3>> texture_means(const brush::Map& map,
                                                const std::filesystem::path& cache,
                                                std::span<const uint8_t> palette);

// How finely and how long a map is rebaked.
struct LightmapOptions {
  double luxel_m = 0.25;  // the classic's are 0.4 m
  int samples = 64;       // gather rays a luxel a pass
  int bounces = 2;
};

// The map's light rebaked and written to `dir` as `<name>.gerb`; false with the reason. `stats`
// (when set) gets the bake's counts and the file's size in bytes.
bool light_map(const brush::Map& map, const std::string& name, const std::filesystem::path& dir,
               std::span<const std::array<float, 3>> albedo, std::string* error,
               const LightmapOptions& options = {}, brush::Rebaked* out = nullptr,
               size_t* bytes = nullptr);

}  // namespace ge::classicenhance

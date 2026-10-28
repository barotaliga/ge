// THE LIGHTMAP STAGE: each map's light, written into the cache for the renderer to read in place
// of the map's own.
//
// Rebaking a map's light (finer luxels, light bounced off walls and coloured by them, the
// dominant direction a luxel for normal maps) is generic to any brush map and is built outside
// this plugin: the baker in the engine and its adapter in `plugins/brush` (roadmap row M40).
// This stage only wires it to the cache: `light_map` takes one read map and writes its file.
// Until the rebake lands, it writes the map's own lightmaps packed into one picture,
// `lightmaps/<map>.png` (surfaces in order, shelf-packed rows, one texel a luxel, a texel of
// border round each), so the cache's layout and the stage's place in the pipeline are final and
// only this stage's body changes.
//
// ge:summary The lightmap stage: a map's light into the cache (the original packed, until the rebake)
// ge:keywords lightmap stage rebake light atlas pack shelf cache map
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "assets/image.hpp"
#include "brush/map.hpp"

namespace ge::classicenhance {

// The stage's version: a rerun redoes the stage when it moves.
inline constexpr int kLightmapStageVersion = 1;

// Every lightmap of `map` shelf-packed into one picture, at most `max_width` texels wide.
assets::Image pack_lightmaps(const brush::Map& map, int max_width = 1024);

// The map's light written to `dir` as `<name>.png`; false with the reason.
bool light_map(const brush::Map& map, const std::string& name, const std::filesystem::path& dir,
               std::string* error);

}  // namespace ge::classicenhance

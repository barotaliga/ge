// The `enhance` lab's map views: a map drawn from its starts (the `bsp` lab's named views, brush's
// `start_eyes`) twice side by side, once with the pack's own textures through its palette and once
// with the enhanced copies in their place, both lit by the map's lightmaps. On the lab's fixtures
// the brush plugin's fixture map wears the lab's painted textures, enhanced here; on a local pack
// (`--source`) the enhanced copies are read from the cache the pipeline wrote (`find_cache`,
// `cached_texture`), as the game reads them, and a texture the cache lacks stays the pack's.
//
// ge:summary The enhance lab's map views: a map's starts drawn with its own and its cached textures
// ge:keywords lab enhance map views named-views start cache cached_texture bsp textured
// ge:keywords before-after
// ge:skill classicenhance labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <string>

#include "classicenhance/texture.hpp"
#include "lab/lab.hpp"

namespace ge::classicenhance::labs {

// The map views' shots and metrics. `source` empty: the fixtures, enhanced with `options`;
// otherwise the classic pack there, `maps` (comma separated, without maps/ and .bsp) read out of
// it and their textures read from the cache under `cache_root` (empty: the default root). False
// after `lab.fail` when a map or the cache cannot be found.
bool map_views(lab::Lab& lab, const std::string& source, const std::string& maps,
               const std::string& cache_root, const EnhanceOptions& options);

}  // namespace ge::classicenhance::labs

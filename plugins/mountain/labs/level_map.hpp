// What the mountain labs share about a map: building one from the flags every map lab takes
// (`--version`, `--region`), and the map drawn from above (the face shaded by height, the packed
// piste, the woods, the piste's line, every gate numbered, every kicker and cliff labelled, the
// start and finish).
//
// ge:summary mountain labs' shared map helpers: build a map from lab flags, draw it from above
// ge:keywords lab level map draw hillshade gates kickers cliffs
// ge:keywords trees piste build-map region version
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::labs {

// The flags a map lab takes beside its own: `version` and `region`.
inline const ge::lab::LabFlag kVersionFlag{
    "version", "1", "generator versions, comma-separated: 1 the single piste, 8 the ski area"};
inline const ge::lab::LabFlag kRegionFlag{"region", "alpine",
                                          "snow country: alpine, fell, continental, maritime"};

// The region a `--region` word names; nothing for a word that names none.
std::optional<mapgen::RegionId> region_named(const std::string& name);

// The generator versions the lab's --version names, in order.
std::vector<int> map_versions(const ge::lab::Lab& lab);

// The map for `seed` at `version` under the lab's --region; nothing (and a lab.fail naming the
// seed and the generator's own words) when it cannot be built. Without a version, the first
// --version names.
std::optional<mapgen::Level> build_map(ge::lab::Lab& lab, double seed,
                                       std::optional<int> version = std::nullopt);

// A map's name in a lab's metrics, tables and shots: `seed-38`, or `seed-38-v8` past version 1.
std::string map_id(double seed, int version);

// The map from above, `px` pixels a side: +x to the right, +z down the picture.
ge::lab::Picture draw_map(const mapgen::Level& level, int px);

// The flags a map was built under, for a shot's inputs.
std::vector<std::pair<std::string, std::string>> map_inputs(const ge::lab::Lab& lab,
                                                            const mapgen::Level& level);

}  // namespace ge::mountain::labs

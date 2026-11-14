// FAST LOADS (docs/saves.md § Fast loads: the content cache): the maps Fall Line's runs open on,
// kept in a content cache (engine/save/cache.hpp) so a second load of the same mountain reads it
// instead of generating it again. A map's key is its kind (the race's or the free ride's), its
// seed, the generator's version asked for and the stamp of the code that builds it (the mountain
// plugin and the engine's core, `ge_source_stamp`): changed generator code is a new key, never a
// stale map. Whatever the cache answers, the map is the generator's to the bit (the round trip is
// held by tests/mountain_level_io_test.cpp and tests/fall_line_map_cache_test.cpp).
//
// The play contract's runs (`open_run`) open through the cache the host set with
// `use_content_cache` (the window sets one in the player's folder); none set, they generate.
//
// ge:summary Fall Line's race and free-ride maps through the content cache: key, hit, miss, build
// ge:keywords content-cache map cache fast-load free-map race-map key stamp
// ge:skill sim save
// ge:test tests/fall_line_map_cache_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "mountain/mapgen/types.hpp"
#include "save/cache.hpp"

namespace fall_line::sim {

inline constexpr uint32_t kMapTag = ge::save::tag("LEVL");

// "fall_line free seed=38 version=latest code=<stamp>".
std::string map_key(std::string_view kind, double seed, std::optional<int> version);

// `free_map` and `race_map` (free.hpp, game.hpp) through `cache`: read when it holds the map, else
// built and kept there. `hit` says which.
ge::mountain::mapgen::Level cached_free_map(double seed, std::optional<int> version,
                                            ge::save::ContentCache& cache, bool* hit = nullptr);
ge::mountain::mapgen::Level cached_race_map(double seed, std::optional<int> version,
                                            ge::save::ContentCache& cache, bool* hit = nullptr);

// The cache the play contract's runs open through (null: none, every map generated).
void use_content_cache(ge::save::ContentCache* cache);
ge::save::ContentCache* content_cache();

}  // namespace fall_line::sim

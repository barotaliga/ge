// Fall Line's maps through the content cache (map_cache.hpp).
//
// ge:summary The race and free-ride maps read from the content cache or built and kept
// ge:keywords content-cache map cache key stamp
// ge:skill sim save
// ge:test tests/fall_line_map_cache_test.cpp
#include "fall_line/sim/map_cache.hpp"

#include "core/num_text.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "mountain/mapgen/level_io.hpp"
#include "stamps/map_code_stamp.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;

namespace {

ge::save::ContentCache* g_cache = nullptr;

template <class Build>
mg::Level cached(std::string_view kind, double seed, std::optional<int> version,
                 ge::save::ContentCache& cache, bool* hit, Build build) {
  const std::string key = map_key(kind, seed, version);
  mg::Level level;
  const bool found = cache.get(key, kMapTag, mg::kLevelVersion, level);
  if (hit != nullptr) *hit = found;
  if (found) return level;
  level = build();
  cache.put(key, kMapTag, mg::kLevelVersion, level);  // not kept: only the next load's speed
  return level;
}

}  // namespace

std::string map_key(std::string_view kind, double seed, std::optional<int> version) {
  return "fall_line " + std::string(kind) + " seed=" + ge::core::num::to_string(seed) +
         " version=" + (version ? std::to_string(*version) : std::string("latest")) +
         " code=" + kMapCodeStamp;
}

mg::Level cached_free_map(double seed, std::optional<int> version, ge::save::ContentCache& cache,
                          bool* hit) {
  return cached("free", seed, version, cache, hit, [&] { return free_map(seed, version); });
}

mg::Level cached_race_map(double seed, std::optional<int> version, ge::save::ContentCache& cache,
                          bool* hit) {
  return cached("race", seed, version, cache, hit, [&] { return race_map(seed, version); });
}

void use_content_cache(ge::save::ContentCache* cache) { g_cache = cache; }
ge::save::ContentCache* content_cache() { return g_cache; }

}  // namespace fall_line::sim

// games/fall_line/sim/map_cache.hpp: the free ride's and the race's maps through the content
// cache. The first load builds and keeps the map; the second reads it, and the run on the read map
// steps to the same state digest as the run on the generated one, step after step; another seed
// or version is another key.
//
// ge:summary Test: Fall Line's maps from the content cache ski as the generated ones
// ge:keywords test content-cache map cache free-map race-map digest fast-load
// ge:skill sim save testing
#include "fall_line/sim/map_cache.hpp"

#include <memory>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "mountain/mapgen/digest.hpp"
#include "save/disk.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;

// The state digest after each of `steps` steps of a field on `level` (the free ride when `free`).
std::vector<uint64_t> digests(std::shared_ptr<const mg::Level> level, bool free, int steps) {
  fs::Driver d = free ? fs::Driver(fs::create_free_ride(std::move(level), 0))
                      : fs::Driver(std::move(level), 2);
  std::vector<uint64_t> out;
  for (int i = 0; i < steps && !d.over(); i++) {
    d.step(fs::bot_controls(d.state(), 0, d.plan()));
    out.push_back(d.digest());
  }
  return out;
}

}  // namespace

TEST(a_second_free_ride_load_reads_the_map_and_rides_the_same) {
  ge::save::SimDisk disk;
  ge::save::ContentCache cache(disk, "fall_line");
  bool hit = true;
  const mg::Level first = fs::cached_free_map(38, std::nullopt, cache, &hit);
  CHECK(!hit && cache.counts().written == 1);
  const mg::Level second = fs::cached_free_map(38, std::nullopt, cache, &hit);
  CHECK(hit && cache.counts().hits == 1);
  CHECK(mg::level_digest(first) == mg::level_digest(second));
  CHECK(digests(std::make_shared<const mg::Level>(first), true, 240) ==
        digests(std::make_shared<const mg::Level>(second), true, 240));
}

TEST(the_race_map_too_and_another_seed_is_another_key) {
  ge::save::SimDisk disk;
  ge::save::ContentCache cache(disk, "fall_line");
  bool hit = true;
  const mg::Level built = fs::cached_race_map(7, 1, cache, &hit);
  CHECK(!hit);
  const mg::Level read = fs::cached_race_map(7, 1, cache, &hit);
  CHECK(hit);
  CHECK(digests(std::make_shared<const mg::Level>(built), false, 240) ==
        digests(std::make_shared<const mg::Level>(read), false, 240));
  CHECK(fs::map_key("race", 7, 1) != fs::map_key("race", 38, 1));
  CHECK(fs::map_key("race", 7, 1) != fs::map_key("free", 7, 1));
  CHECK(fs::map_key("race", 7, 1) != fs::map_key("race", 7, 2));
}

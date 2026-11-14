// plugins/mountain/mapgen/level_io.hpp: a generated map written through its describes and read
// back is the same map: its digest equal and its bytes written again the same, for a version 1
// map, a ski area's (with its runs, lifts, courses and routes), a slalom set over one (its base
// map inside it) and a ski jumping hill (its venue's surface); a damaged chunk is refused.
//
// ge:summary Test: a generated map round-trips through the save file, every kind of map
// ge:keywords test level describe save round-trip digest resort slalom ski-jump venue
// ge:skill mountain save testing
#include "mountain/mapgen/level_io.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/ski_jump.hpp"
#include "mountain/mapgen/slalom.hpp"
#include "save/io.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace sv = ge::save;

mg::Level map_of(double seed, int version) {
  mg::GenerateOptions opts;
  opts.version = version;
  if (version != 1) opts.grade = mg::PisteGrade::red;
  return mg::generate_level(seed, opts);
}

void round_trip(const mg::Level& level, const std::string& what) {
  std::vector<uint8_t> bytes, again;
  sv::write(bytes, level, mg::kLevelVersion);
  mg::Level back;
  sv::Reader r(bytes.data(), bytes.size(), mg::kLevelVersion);
  r(back);
  CHECK_MSG(r.ok() && r.at_end(), what + ": " + r.problem());
  CHECK_MSG(mg::level_digest(back) == mg::level_digest(level), what + ": the digest moved");
  sv::write(again, back, mg::kLevelVersion);
  CHECK_MSG(again == bytes, what + ": written again, the bytes differ");
  // Cut short, it is refused, never read past its end.
  sv::Reader cut(bytes.data(), bytes.size() / 2, mg::kLevelVersion);
  mg::Level part;
  cut(part);
  CHECK_MSG(!cut.ok(), what + ": half the bytes read as a whole map");
}

}  // namespace

TEST(a_version_one_map_round_trips) { round_trip(map_of(38, 1), "seed 38 v1"); }

TEST(a_ski_area_round_trips_with_its_runs_lifts_and_routes) {
  const mg::Level level = map_of(38, 9);
  CHECK(level.resort.has_value() && !level.resort->runs.empty());
  round_trip(level, "seed 38 v9");
}

TEST(a_slalom_round_trips_with_its_base_map) {
  const mg::Level level = mg::set_slalom(map_of(38, 1), 1);
  CHECK(level.race.has_value() && level.race->base != nullptr);
  round_trip(level, "seed 38 slalom");
}

TEST(a_ski_jump_round_trips_with_its_venue) {
  const mg::Level level = mg::set_ski_jump(map_of(38, 1), mg::JumpHill::large);
  CHECK(level.ski_jump.has_value() && level.venue != nullptr);
  round_trip(level, "seed 38 ski jump");
}

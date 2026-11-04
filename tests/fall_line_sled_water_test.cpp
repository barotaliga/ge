// FALL LINE'S SNOWMOBILE ON THE TARN, HELD (games/fall_line/sim/lake.hpp's `sled_on_lake`): on seed
// 38's map with its tarn laid open, the free ride's machine driven onto the water flat out planes
// across it to the far bank with its rider still on; one let coast onto it at a planing speed
// skims and then goes under, its rider thrown in to swim out, and his reset hauls it out onto the
// bank with him on it; one driven on slower than it planes goes straight under.
//
// ge:summary Test: the snowmobile on the tarn's open water: planing across, sinking, hauled out
// ge:keywords test snowmobile sled tarn skim sunk sled_on_lake haul_out swim ashore free-ride
// ge:skill sim testing
#include <cmath>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/lake.hpp"
#include "fall_line/sim/sled_outing.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/tarn/tarn.hpp"
#include "snowmobile/sled.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

// Seed 38's map at v10 with its tarn laid open: no ice, the bowl dug.
std::shared_ptr<const mg::Level> open_tarn() {
  mg::GenerateOptions opts;
  opts.version = 10;
  mg::Level level = mg::generate_level(38, opts);
  const std::optional<mg::MapLake> found = tn::find_tarn(level, fs::kTarnRadius);
  CHECK_MSG(found.has_value(), "seed 38 found no tarn; see mountain/tarn/tarn.hpp");
  mg::MapLake lake = found.value_or(mg::MapLake{});
  tn::dig_basin(level, lake, tn::kTarnDepth, false);
  level.lakes.push_back(lake);
  return std::make_shared<const mg::Level>(std::move(level));
}

struct Outcome {
  int skims = 0, sunk = 0, sank = 0, ashore = 0;
  double far = -1e30;  // the farthest north of the tarn's middle the machine got, of its radius
};

void tally(const fs::GameState& state, Outcome& out) {
  for (const fs::GameEvent& e : state.events) {
    out.skims += e.kind == fs::EventKind::skim ? 1 : 0;
    out.sank += e.kind == fs::EventKind::sank ? 1 : 0;
    out.ashore += e.kind == fs::EventKind::ashore ? 1 : 0;
    out.sunk += e.kind == fs::EventKind::sled && e.gate == static_cast<int32_t>(fs::SledPhase::sunk)
                    ? 1
                    : 0;
  }
}

// Racer 0 on the free ride's machine, stood `in` of the tarn's radius south of its middle heading
// north into it at `speed` m/s; then `seconds` on `c`.
Outcome drive(fs::GameState& state, double speed, const fs::Controls& c, double seconds) {
  fs::stand_by_sled(state, 0);
  fs::Controls take;
  take.machine = true;
  fs::step(state, std::span<const fs::Controls>(&take, 1));
  CHECK(fs::on_sled(state, 0));
  const mg::MapLake& lake = state.level->lakes[0];
  ge::snowmobile::SledState& b = state.sled.body;
  const double rpm = b.rpm;
  b = ge::snowmobile::stand_sled(*state.level, lake.x, lake.z - 1.15 * lake.radius, 0);
  b.vz = speed;
  b.speed = speed;
  b.way = speed;
  b.tread_speed = speed;
  b.running = true;
  b.rpm = rpm;
  Outcome out;
  for (int k = 0; k < static_cast<int>(seconds * fs::kHz); k++) {
    fs::step(state, std::span<const fs::Controls>(&c, 1));
    tally(state, out);
    out.far = std::max(out.far, (state.sled.body.z - lake.z) / lake.radius);
  }
  return out;
}

std::string said(const Outcome& o) {
  return std::to_string(o.skims) + " skims, " + std::to_string(o.sunk) + " sunk, " +
         std::to_string(o.sank) + " in, " + std::to_string(o.ashore) + " ashore, as far as " +
         std::to_string(o.far) + " of the radius north";
}

}  // namespace

TEST(flat_out_onto_open_water_it_planes_across_to_the_far_bank) {
  fs::GameState state = fs::create_free_ride(open_tarn(), 0);
  CHECK(state.water != nullptr);
  fs::Controls go;
  go.ski.tuck = 1;
  const Outcome o = drive(state, 16, go, 8);
  CHECK_MSG(o.skims == 1 && o.sunk == 0 && o.sank == 0 && o.far > 1.1,
            said(o) + "; see games/fall_line/sim/lake.hpp (skill: sim)");
  CHECK(fs::on_sled(state, 0) && !state.sled.sunk && !state.sled.skimming);
}

TEST(let_coast_it_skims_then_goes_under_and_he_swims_out_and_hauls_it_out) {
  fs::GameState state = fs::create_free_ride(open_tarn(), 0);
  const Outcome o = drive(state, 17, fs::Controls{}, 85);
  CHECK_MSG(o.skims == 1 && o.sunk == 1 && o.sank == 1 && o.ashore == 1 && o.far < 1,
            said(o) + "; see games/fall_line/sim/lake.hpp (skill: sim)");
  CHECK(state.sled.sunk && !fs::on_sled(state, 0) && !fs::on_water(state, 0));
  const mg::MapLake& lake = state.level->lakes[0];
  CHECK(tn::on_lake(lake, state.sled.body.x, state.sled.body.z));
  // The machine under the water is not to be taken by the press from the bank.
  CHECK(!fs::sled_within(state, 0));
  // His reset hauls it out onto the bank, stood on its belt, and him on its boards.
  fs::Controls reset;
  reset.reset = true;
  fs::step(state, std::span<const fs::Controls>(&reset, 1));
  CHECK(fs::on_sled(state, 0) && !state.sled.sunk && state.sled.lake == -1);
  CHECK(!tn::on_lake(lake, state.sled.body.x, state.sled.body.z));
  CHECK(state.sled.body.y > lake.level);
}

TEST(driven_on_slower_than_it_planes_it_goes_straight_under) {
  fs::GameState state = fs::create_free_ride(open_tarn(), 0);
  const Outcome o = drive(state, 6, fs::Controls{}, 6);
  CHECK_MSG(o.skims == 0 && o.sunk == 1 && o.sank == 1, said(o));
  CHECK(state.sled.sunk && !fs::on_sled(state, 0));
}

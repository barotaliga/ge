// FALL LINE'S TARN, HELD (games/fall_line/sim/lake.hpp): a skier stood in the open water of a
// tarn dug on seed 38's map goes in (one `sank` event), is slowed by the water and is stood back
// on a run after `kSoakReset` s; the same tarn frozen to bear him is level snow he skis over with
// nothing said; and the free ride's own map lays the tarn the tarn module finds.
//
// ge:summary Test: the free ride's tarn: in through thin ice, slowed, reset; frozen, skied over
// ge:keywords test tarn lake sank soaked step_lake lake_under thin-ice free-ride
// ge:skill sim testing
#include <memory>
#include <span>

#include "check.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/lake.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/tarn/tarn.hpp"
#include "skier/skier.hpp"

namespace {

namespace fs = fall_line::sim;
namespace mg = ge::mountain::mapgen;
namespace tn = ge::mountain::tarn;

// Seed 38's map at v10 with a tarn laid open (`frozen` false: no ice, the bowl dug) or frozen.
std::shared_ptr<const mg::Level> with_tarn(bool frozen) {
  mg::GenerateOptions opts;
  opts.version = 10;
  mg::Level level = mg::generate_level(38, opts);
  const std::optional<mg::MapLake> found = tn::find_tarn(level, fs::kTarnRadius);
  CHECK_MSG(found.has_value(), "seed 38 found no tarn; see mountain/tarn/tarn.hpp");
  mg::MapLake lake = found.value_or(mg::MapLake{});
  lake.ice = frozen ? 0.4 : 0;
  tn::dig_basin(level, lake, tn::kTarnDepth, frozen);
  level.lakes.push_back(lake);
  return std::make_shared<const mg::Level>(std::move(level));
}

int count(const std::vector<fs::GameEvent>& events, fs::EventKind kind) {
  int n = 0;
  for (const fs::GameEvent& e : events) n += e.kind == kind ? 1 : 0;
  return n;
}

// Racer 0 stood `in` of the radius from the tarn's middle, heading into it at `speed` m/s, then
// `seconds` of nothing pressed; every event kept.
std::vector<fs::GameEvent> into(fs::GameState& state, double in, double speed, double seconds) {
  const mg::MapLake& lake = state.level->lakes[0];
  state.racers[0].skier =
      ge::skier::stand_skier(state.world(), lake.x, lake.z - in * lake.radius, 0, speed);
  std::vector<fs::GameEvent> all;
  const fs::Controls none{};
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

}  // namespace

TEST(through_thin_ice_he_sinks_is_slowed_and_is_stood_back_on_a_run) {
  fs::GameState state = fs::create_free_ride(with_tarn(false), 0);
  const mg::MapLake& lake = state.level->lakes[0];
  CHECK(lake.depth > 5);
  // A moment in: in the water, the water's hold on him.
  std::vector<fs::GameEvent> ev = into(state, 0.6, 8, 0.5);
  CHECK(count(ev, fs::EventKind::sank) == 1);
  CHECK(fs::lake_under(state, 0) == 0);
  CHECK(state.racers[0].free.soaked > 0.4);
  CHECK(state.racers[0].skier.speed < 4);
  for (const fs::GameEvent& e : ev) {
    if (e.kind == fs::EventKind::sank) CHECK(e.gate == 0 && e.value == 0);
  }
  // Left in it, he is reset once, and stood out of the water.
  state = fs::create_free_ride(with_tarn(false), 0);
  ev = into(state, 0.6, 8, fs::kSoakReset + 0.5);
  CHECK(count(ev, fs::EventKind::sank) == 1);
  CHECK(count(ev, fs::EventKind::reset) == 1);
  CHECK(fs::lake_under(state, 0) == -1);
  CHECK(state.racers[0].free.soaked == 0);
  CHECK(!tn::on_lake(state.level->lakes[0], state.racers[0].skier.x, state.racers[0].skier.z));
}

TEST(ice_that_bears_him_is_snow_he_skis_over) {
  fs::GameState state = fs::create_free_ride(with_tarn(true), 0);
  CHECK(state.level->lakes[0].depth == 0);
  const std::vector<fs::GameEvent> ev = into(state, 0.6, 8, 3);
  CHECK(count(ev, fs::EventKind::sank) == 0);
  CHECK(state.racers[0].free.soaked == 0);
}

TEST(the_free_ride_lays_the_tarn_the_module_finds) {
  const mg::Level level = fs::free_map(38, 10);
  CHECK(level.lakes.size() == 1);
  mg::GenerateOptions opts;
  opts.version = 10;
  const std::optional<mg::MapLake> found =
      tn::find_tarn(mg::generate_level(38, opts), fs::kTarnRadius);
  CHECK(found.has_value());
  if (level.lakes.empty() || !found) return;
  const mg::MapLake& laid = level.lakes[0];
  CHECK(laid.x == found->x && laid.z == found->z);
  CHECK(laid.ice == tn::tarn_ice(level, laid));
  CHECK((laid.depth == 0) == tn::bears(laid.ice));
  // A dealt race map lays none.
  CHECK(mg::generate_level(38, opts).lakes.empty());
}

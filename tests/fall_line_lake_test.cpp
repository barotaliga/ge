// FALL LINE'S TARN, HELD (games/fall_line/sim/lake.hpp): on seed 38's map with its tarn laid
// open, a skier slow onto the water goes in (one `sank` event), floats on his jacket's air,
// swims for the shore and climbs out onto the bank (one `ashore` event); one fast enough skims
// it to the far bank and one a little slower skims and then goes in; the same tarn frozen to
// bear him is level snow he skis over with nothing said; the bot's runs keep off the open water;
// and the free ride's own map lays the tarn the tarn module finds.
//
// ge:summary Test: the free ride's tarn: in, swimming out, skimming across, frozen, the bot
// ge:keywords test tarn lake sank skim ashore swim step_on_water thin-ice free-ride bot
// ge:skill sim testing
#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/bot.hpp"
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

// `seconds` of nothing pressed for racer 0; every event kept.
std::vector<fs::GameEvent> run(fs::GameState& state, double seconds) {
  std::vector<fs::GameEvent> all;
  const fs::Controls none{};
  for (int i = 0; i < static_cast<int>(seconds * fs::kHz); i++) {
    fs::step(state, std::span<const fs::Controls>(&none, 1));
    all.insert(all.end(), state.events.begin(), state.events.end());
  }
  return all;
}

// Racer 0 stood `in` of the radius south of the tarn's middle (past 1.2, on the bank), heading
// north into it at `speed` m/s, then `seconds` of nothing pressed; every event kept.
std::vector<fs::GameEvent> into(fs::GameState& state, double in, double speed, double seconds) {
  const mg::MapLake& lake = state.level->lakes[0];
  state.racers[0].skier =
      ge::skier::stand_skier(state.world(), lake.x, lake.z - in * lake.radius, 0, speed);
  return run(state, seconds);
}

}  // namespace

TEST(slow_onto_open_water_he_goes_in_and_swims_out) {
  fs::GameState state = fs::create_free_ride(with_tarn(false), 0);
  CHECK(state.water != nullptr);
  const mg::MapLake& lake = state.level->lakes[0];
  CHECK(lake.depth > 5);
  // A moment in: in the water, afloat on his jacket's air, the water's hold on him.
  std::vector<fs::GameEvent> ev = into(state, 0.55, 6, 6);
  CHECK(count(ev, fs::EventKind::sank) == 1);
  CHECK(count(ev, fs::EventKind::skim) == 0);
  const fs::FreeRide& f = state.racers[0].free;
  CHECK(f.swimming && f.lake == 0 && fs::on_water(state, 0));
  CHECK(f.swim.in_water > 5 && f.swim.level >= 1);
  CHECK(f.swim.p.y > lake.level - 1.8);
  for (const fs::GameEvent& e : ev) {
    if (e.kind == fs::EventKind::sank) CHECK(e.gate == 0 && e.value == 0);
  }
  // He swims for the shore and climbs out onto the bank, dry of the lake.
  ev = run(state, 120);
  CHECK(count(ev, fs::EventKind::ashore) == 1);
  CHECK(count(ev, fs::EventKind::reset) == 0);
  CHECK(!fs::on_water(state, 0) && f.lake == -1);
  const ge::skier::SkierState& s = state.racers[0].skier;
  CHECK(!tn::on_lake(lake, s.x, s.z) && !s.down);
  for (const fs::GameEvent& e : ev) {
    if (e.kind == fs::EventKind::ashore) CHECK(e.value > 5 && e.value < fs::kSwimReset);
  }
}

TEST(fast_enough_he_skims_it_a_little_slower_he_skims_and_goes_in) {
  fs::GameState state = fs::create_free_ride(with_tarn(false), 0);
  const mg::MapLake lake = state.level->lakes[0];
  // From the south bank at 45 m/s: across on the water and onto the north bank, on his skis.
  std::vector<fs::GameEvent> ev = into(state, 1.25, 45, 9);
  CHECK(count(ev, fs::EventKind::skim) == 1);
  CHECK(count(ev, fs::EventKind::sank) == 0);
  const ge::skier::SkierState& s = state.racers[0].skier;
  CHECK(!fs::on_water(state, 0) && s.z > lake.z + lake.radius * 0.8);
  // At 17 m/s the water's drag has him under 8 m/s short of the middle: in, and swimming.
  state = fs::create_free_ride(with_tarn(false), 0);
  ev = into(state, 1.25, 17, 6);
  CHECK(count(ev, fs::EventKind::skim) == 1);
  CHECK(count(ev, fs::EventKind::sank) == 1);
  CHECK(state.racers[0].free.swimming && state.racers[0].skier.z < lake.z);
}

TEST(ice_that_bears_him_is_snow_he_skis_over) {
  fs::GameState state = fs::create_free_ride(with_tarn(true), 0);
  CHECK(state.level->lakes[0].depth == 0);
  CHECK(state.water == nullptr);
  const std::vector<fs::GameEvent> ev = into(state, 0.6, 8, 3);
  CHECK(count(ev, fs::EventKind::sank) == 0 && count(ev, fs::EventKind::skim) == 0);
  CHECK(!fs::on_water(state, 0));
}

TEST(the_bot_keeps_off_the_open_tarn) {
  // The runs nearest the open tarn, skied by the bot from their heads: never on the water.
  const std::shared_ptr<const mg::Level> level = with_tarn(false);
  const mg::MapLake& lake = level->lakes[0];
  CHECK(level->resort.has_value());
  if (!level->resort) return;
  const std::vector<mg::Run>& runs = level->resort->runs;
  std::vector<std::pair<double, int32_t>> near;
  for (size_t k = 0; k < runs.size(); ++k) {
    double best = 1e30;
    for (const mg::TrackPoint& p : runs[k].points)
      best = std::min(best, std::hypot(p.x - lake.x, p.z - lake.z));
    near.emplace_back(best, static_cast<int32_t>(k));
  }
  std::sort(near.begin(), near.end());
  for (size_t n = 0; n < 2 && n < near.size(); ++n) {
    fs::GameState state = fs::create_free_ride(level, near[n].second);
    const fs::BotPlan plan = fs::plan_free(*state.level, near[n].second, state.spec);
    int wet = 0;
    for (int i = 0; i < 300 * fs::kHz && !state.racers[0].free.arrived; i++) {
      const fs::Controls c = fs::bot_controls(state, 0, plan);
      fs::step(state, std::span<const fs::Controls>(&c, 1));
      for (const fs::GameEvent& e : state.events) {
        wet += e.kind == fs::EventKind::sank || e.kind == fs::EventKind::skim;
      }
    }
    CHECK_MSG(wet == 0, "the bot went onto the open tarn on run " + std::to_string(near[n].second) +
                            "; see games/fall_line/sim/lake.hpp");
  }
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

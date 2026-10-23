// The body on Fall Line's run: every racer's body stepped after his skis under the INJURIES switch
// (`GameState::injuries`), hurt by the blows his skis meet, mended at a reset; mortally hurt he
// is never stood back up, and dead his run is over. Off, no body is touched and no digest moves.
//
// ge:summary Tests the body on Fall Line's run: the injuries switch, the hurt and died events
// ge:keywords body injuries injury_switch hurt died mend_body step_body racer death run_over
// ge:skill sim skier
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/game.hpp"
#include "skier/synthetic.hpp"

namespace {

namespace fs = fall_line::sim;
namespace sk = ge::skier;

// One racer on the hand-built slope, sent straight at its lone tree at `speed` m/s from `z`.
fs::GameState at_the_tree(double speed, double z, bool injuries) {
  auto level = std::make_shared<const ge::mountain::mapgen::Level>(sk::synthetic_slope());
  fs::GameState state = fs::create_game_on(level, 1);
  state.injuries = injuries;
  state.racers[0].skier = sk::stand_skier(state.world(), sk::slope::kTreeX, z, 0, speed, 0);
  return state;
}

struct Seen {
  int32_t hurt = 0, died = 0, resets = 0;
  double died_at = -1;
};

Seen run(fs::GameState& state, double seconds) {
  Seen seen;
  const std::vector<fs::Controls> none(1);
  const auto steps = static_cast<int64_t>(seconds * 120);
  for (int64_t i = 0; i < steps; i++) {
    fs::step(state, none);
    for (const fs::GameEvent& e : state.events) {
      if (e.kind == fs::EventKind::hurt) seen.hurt += e.gate;
      if (e.kind == fs::EventKind::died && seen.died_at < 0) seen.died_at = state.t;
      seen.died += e.kind == fs::EventKind::died;
      seen.resets += e.kind == fs::EventKind::reset;
    }
  }
  return seen;
}

}  // namespace

TEST(the_switch_off_leaves_every_body_untouched) {
  fs::GameState state = at_the_tree(20, 200, false);
  const Seen seen = run(state, 6);
  const sk::BodyState& b = state.racers[0].body;
  CHECK_MSG(seen.hurt == 0 && seen.died == 0 && b.count == 0 && b.blows == 0 && b.dead < 0,
            "with the switch off the body took " + std::to_string(b.count) + " injuries");
  CHECK_MSG(seen.resets > 0, "with the switch off he is stood back up after the trunk");
}

TEST(a_trunk_met_slowly_hurts_and_a_reset_mends_him) {
  fs::GameState state = at_the_tree(6, 254, true);
  const Seen seen = run(state, 4);
  CHECK_MSG(seen.hurt > 0, "a trunk met at 6 m/s took " + std::to_string(seen.hurt) + " injuries");
  CHECK(seen.died == 0 && state.racers[0].body.mortal < 0);
  const int32_t blows = state.racers[0].body.blows;
  fs::Controls press;
  press.reset = true;
  fs::step(state, std::vector<fs::Controls>{press});
  const sk::BodyState& b = state.racers[0].body;
  CHECK_MSG(b.count == 0 && sk::severity_of(b) == 0 && b.blows == blows,
            "after the reset the body still has " + std::to_string(b.count) +
                " injuries; its meter " + std::to_string(b.blows) + " blows, before " +
                std::to_string(blows));
}

TEST(a_trunk_met_flat_out_kills_him_and_his_run_is_over) {
  fs::GameState state = at_the_tree(20, 200, true);
  const Seen seen = run(state, 15);
  const fs::Racer& r = state.racers[0];
  CHECK_MSG(seen.hurt > 0 && r.body.mortal >= 0,
            "a trunk met at 20 m/s took " + std::to_string(seen.hurt) + " injuries, severity " +
                std::to_string(sk::severity_of(r.body)));
  CHECK_MSG(seen.died == 1 && r.body.dead >= 0, "he died " + std::to_string(seen.died) +
                                                    " times; dead at " +
                                                    std::to_string(r.body.dead) + " s");
  CHECK_MSG(seen.resets == 0,
            "a mortally hurt skier was stood back up " + std::to_string(seen.resets) + " times");
  CHECK(r.skier.down);
}

TEST(the_switch_on_digests_the_body_and_replays) {
  fs::GameState a = at_the_tree(20, 200, true);
  fs::GameState b = at_the_tree(20, 200, true);
  run(a, 8);
  run(b, 8);
  CHECK(fs::state_hash(a, 0) == fs::state_hash(b, 0));
  fs::GameState off = at_the_tree(20, 200, false);
  run(off, 8);
  CHECK(fs::state_hash(a, 0) != fs::state_hash(off, 0));
}

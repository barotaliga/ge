// FALL LINE'S MOMENTS FOR A REPLAY (engine/replay: live.hpp's marks, director.hpp's rules): which
// of the player's events are worth marking, what each is worth, and how the director shoots each
// kind. A FLIGHT is filed at its take-off, known only at its landing (its worth is its air time
// and its landing's load), so its shot can open on the run-in before the lip; a WIPEOUT, a
// SHOULDER with a rival (by the closing speed), a MISSED gate and the FINISH are filed as they
// happen; a RESET is marked and never cut to.
//
// The lenses are names the game's renderer draws a shot through: `side` the long lens beside a
// kicker, `chase` behind, `front` facing the skier, `aerial` from above the finish.
//
// ge:summary Fall Line's replay moments: events filed as marks, the director's rules and policy
// ge:keywords moments marks flight wipeout shoulder missed finish reset director shot-rule lens
// ge:keywords replay highlight side chase front aerial take-off back-date
// ge:skill sim replay
// ge:test tests/fall_line_play_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "fall_line/sim/state.hpp"
#include "replay/director.hpp"
#include "replay/live.hpp"

namespace fall_line::sim {

// How the director shoots each kind of Fall Line's marks.
std::span<const ge::replay::ShotRule> shot_rules();
// A highlight reel's spacing, length and count.
ge::replay::DirectorPolicy director_policy();

// Files the player's moments of each step into a live recorder.
class Moments {
 public:
  // `state` as the step just taken left it (its events are that step's), `live` the recorder
  // that took it (its `steps()` the step's count).
  void file(const GameState& state, ge::replay::Live& live);

 private:
  int64_t takeoff_ = -1;  // the step the player's flight left the snow, -1 on the snow
};

}  // namespace fall_line::sim

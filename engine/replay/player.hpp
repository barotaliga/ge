// THE REPLAY'S PLAYER: an input log watched again from any moment (docs/replays.md § Keyframes
// and the seek). It opens the log's game from its setup and steps the run off the log, offering
// every step to its keeper (keep.hpp), so the copies a later seek wants are taken on the way.
//
// A SEEK sets a target; `advance` gets there in a budget of steps, so the app pays a long seek out
// over frames instead of stalling one. Behind the run (or ahead of it past a newer copy) it starts
// from the nearest copy at or before the target, cloned, never stepped itself; with no copy it
// opens the run again from the setup (the run's first step, for free). Then it steps forward off
// the log. A run sought to step `s` IS the run played straight to `s`: the same digest, the same
// state (`tests/replay_player_test.cpp`, the `replay` lab).
//
// The log's values are already on their axes' grids (the parser snaps them), so they are handed
// to the run as they are.
//
// ge:summary The replay's player: a log stepped off its keyframes to any step, in a step budget
// ge:keywords replay player seek advance budget target keyframe clone restore reopen log cursor
// ge:keywords watch again instant-replay scrub stats restores reopens
// ge:skill replay
// ge:test tests/replay_player_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "play/game.hpp"
#include "play/inputs.hpp"
#include "replay/keep.hpp"

namespace ge::replay {

class Player {
 public:
  // `log` must fit `spec` (its game and axes) and open; `ok()` says whether it did, `error` why
  // not. `policy` must have no `policy_problem` at the spec's step rate.
  Player(const play::GameSpec& spec, play::InputLog log, const KeepPolicy& policy = {});
  bool ok() const { return run_ != nullptr; }
  const std::string& error() const { return error_; }

  const play::InputLog& log() const { return log_; }
  // Steps the log holds: the last step a seek can reach.
  int64_t length() const { return length_; }
  // Steps the run has taken.
  int64_t at() const { return at_; }
  const play::Run& run() const { return *run_; }
  play::Run& run() { return *run_; }
  const Keeper& keeper() const { return keeper_; }

  // The run is to stand at `step` (held inside 0..length).
  void seek(int64_t step);
  int64_t target() const { return target_; }
  // At most `budget` steps toward the target (a restore or a reopen takes none of them); true
  // once the run stands on it.
  bool advance(int64_t budget);

  struct Stats {
    int64_t steps = 0;     // steps taken, over every seek
    int64_t restores = 0;  // seeks begun from a keyframe
    int64_t reopens = 0;   // seeks begun from the setup
  };
  const Stats& stats() const { return stats_; }

 private:
  // The run started over from the nearest copy at or before the target, or from the setup.
  void restart();
  void step_once();

  const play::GameSpec* spec_;
  play::InputLog log_;
  play::Setup setup_;
  std::vector<int64_t> ends_;  // ends_[r]: the steps through run r of the log
  int64_t length_ = 0;
  Keeper keeper_;
  std::unique_ptr<play::Run> run_;
  std::string error_;
  int64_t at_ = 0;
  int64_t target_ = 0;
  size_t cursor_ = 0;  // the log's run step `at_` is in
  Stats stats_;
};

}  // namespace ge::replay

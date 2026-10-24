// THE KEYFRAMES: copies of a run kept as it goes, so a replay can be watched from any moment
// instead of only from its first step (docs/replays.md § Keyframes and the seek).
//
// A replay is the run stepped again off its input log, and a step is not free, so every few
// seconds the run is CLONED (`play::Run::clone`) and a seek starts from the nearest copy at or
// before the mark. WHICH COPIES ARE KEPT, by their age behind the newest copy:
//   - every copy of the last `near_s` seconds (one each `every_s`);
//   - then one in `mid_every_s` back to `mid_s`;
//   - then one in `far_every_s` back to `window_s` (0: no limit), and none past it;
//   - the run's first step always: it is the run as opened, which the setup reopens anyway.
// A copy is taken only on a multiple of `every_s`'s steps and kept only on a multiple of its
// age band's spacing, so a copy dropped is never wanted again and the kept set is a pure function
// of the policy and the newest step: the same on every machine, whatever order it was offered in.
//
// The keeper owns its copies and never steps one: a seek clones the copy it starts from, so the
// same keyframe can be sought from again.
//
// ge:summary The keyframes a replay seeks from: run clones kept every few seconds, thinned with age
// ge:keywords replay keyframe keyframes keep keeper clone copy snapshot seek thin age policy
// ge:keywords instant-replay near mid far window every nearest at_or_before kept_steps
// ge:skill replay
// ge:test tests/replay_keep_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "play/game.hpp"

namespace ge::replay {

// Seconds of the run's own clock. Each spacing must be a whole multiple of `every_s` (and
// `far_every_s` of `mid_every_s`); `policy_problem` says when one is not.
struct KeepPolicy {
  double every_s = 2;
  double near_s = 20;
  double mid_every_s = 6;
  double mid_s = 120;
  double far_every_s = 24;
  double window_s = 0;  // the furthest back a copy is kept; 0 keeps the whole run
};

// The policy in steps at `hz`.
struct KeepSteps {
  int64_t every = 0, near = 0, mid_every = 0, mid = 0, far_every = 0, window = 0;
};

// Why `p` cannot be kept by at `hz` ("" when it can): a spacing that is not a whole number of
// steps or not a multiple of the band before it, a band shorter than the one before it.
std::string policy_problem(const KeepPolicy& p, double hz);
KeepSteps keep_steps(const KeepPolicy& p, double hz);

// Whether a copy at `step` is kept while the newest copy is at `newest` (the rule above).
bool keeps(const KeepSteps& k, int64_t step, int64_t newest);

class Keeper {
 public:
  // `policy` must have no `policy_problem` at `hz`.
  Keeper(const KeepPolicy& policy, double hz);

  // After the run has taken `step` steps: a copy is cloned when one is due (a multiple of
  // `every`, newer than the newest kept) and the older ones thinned. Returns whether one was taken
  // (false also when the game clones nothing).
  bool offer(const play::Run& run, int64_t step);
  // The copy taken at the largest kept step at or before `step`, or null when none is.
  const play::Run* at_or_before(int64_t step, int64_t* at = nullptr) const;
  // The kept steps, oldest first.
  std::vector<int64_t> kept_steps() const;
  size_t size() const { return frames_.size(); }
  int64_t newest() const { return frames_.empty() ? -1 : frames_.back().step; }
  const KeepSteps& steps() const { return steps_; }
  // Every copy dropped: a replay that changes its log past a copy must start again.
  void clear() { frames_.clear(); }

 private:
  struct Frame {
    int64_t step;
    std::unique_ptr<play::Run> run;
  };
  KeepSteps steps_;
  std::vector<Frame> frames_;  // by step, oldest first
};

}  // namespace ge::replay

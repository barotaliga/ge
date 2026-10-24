// The live recorder's definitions (live.hpp): the copies offered as it steps, the marks kept in
// step order, and the keeper lent to an instant replay and taken back.
//
// ge:summary The live recorder's definitions: step, the marks, the file, watch and resume
// ge:keywords replay live step mark mark_at file watch resume keeper
// ge:skill replay
// ge:test tests/replay_live_test.cpp
#include "replay/live.hpp"

#include <algorithm>
#include <utility>

#include "core/num.hpp"

namespace ge::replay {

Live::Live(const play::GameSpec& spec, const play::Setup& setup, const KeepPolicy& policy)
    : spec_(spec), session_(spec, setup), keeper_(policy, spec.hz) {
  const std::string problem = policy_problem(policy, spec.hz);
  if (!problem.empty()) {
    error_ = "the keep policy: " + problem;
    return;
  }
  if (session_.ok()) keeper_.offer(session_.run(), 0);
}

void Live::step(std::span<const double> input) {
  session_.step(input);
  if (!watching_) keeper_.offer(session_.run(), session_.steps());
}

void Live::mark(const std::string& kind, double weight) { mark_at(steps(), kind, weight); }

void Live::mark_at(int64_t step, const std::string& kind, double weight) {
  if (!mark_kind_ok(kind) || weight != weight) {
    bad_marks_++;
    return;
  }
  Mark m{std::clamp<int64_t>(step, 0, steps()), kind, weight};
  // After any mark at the same step, so marks filed at one step keep the order they were filed.
  const auto at = std::upper_bound(marks_.begin(), marks_.end(), m.step,
                                   [](int64_t s, const Mark& x) { return s < x.step; });
  marks_.insert(at, std::move(m));
}

ReplayFile Live::file(const std::string& recorded) const {
  ReplayFile f;
  f.log = session_.inputs();
  f.game_version = spec_.version;
  f.marks = marks_;
  f.recorded = recorded;
  return f;
}

std::unique_ptr<Player> Live::watch(double seconds_back) {
  if (watching_ || !ok()) return nullptr;
  auto replay = std::make_unique<Player>(spec_, session_.inputs(), std::move(keeper_));
  if (!replay->ok()) {
    error_ = "the instant replay: " + replay->error();
    keeper_ = replay->release_keeper();
    return nullptr;
  }
  watching_ = true;
  const auto back = static_cast<int64_t>(core::num::round(seconds_back * spec_.hz));
  replay->seek(steps() - back);
  return replay;
}

void Live::resume(Player& replay) {
  if (!watching_) return;
  keeper_ = replay.release_keeper();
  watching_ = false;
  keeper_.offer(session_.run(), session_.steps());
}

}  // namespace ge::replay

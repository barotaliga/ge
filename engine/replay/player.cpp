// The player's seek: where a seek starts (the run as it stands, a copy, or the setup) and the
// stepping off the log's runs through a cursor, so a step costs no walk of the log.
//
// ge:summary The player's definitions: the restart, the cursor over the log's runs, advance
// ge:keywords replay player restart cursor advance step_once ends
// ge:skill replay
// ge:test tests/replay_player_test.cpp
#include "replay/player.hpp"

#include <algorithm>
#include <utility>

#include "play/session.hpp"

namespace ge::replay {

Player::Player(const play::GameSpec& spec, play::InputLog log, const KeepPolicy& policy)
    : spec_(&spec), log_(std::move(log)), keeper_(policy, spec.hz) {
  error_ = policy_problem(policy, spec.hz);
  if (!error_.empty()) {
    error_ = "the keep policy: " + error_;
    return;
  }
  if (!play::setup_of(spec, log_, setup_, error_)) return;
  ends_.reserve(log_.runs.size());
  for (const play::InputRun& r : log_.runs) {
    length_ += r.count;
    ends_.push_back(length_);
  }
  run_ = spec.open(setup_, error_);
  if (run_) keeper_.offer(*run_, 0);
}

void Player::seek(int64_t step) { target_ = step < 0 ? 0 : step > length_ ? length_ : step; }

void Player::restart() {
  int64_t from = 0;
  const play::Run* copy = keeper_.at_or_before(target_, &from);
  std::unique_ptr<play::Run> next = copy ? copy->clone() : nullptr;
  if (next) {
    stats_.restores++;
  } else {
    std::string error;
    next = spec_->open(setup_, error);
    if (!next) return;  // it opened once with this setup; a game that refuses now keeps the run
    from = 0;
    stats_.reopens++;
  }
  run_ = std::move(next);
  at_ = from;
  cursor_ = static_cast<size_t>(std::upper_bound(ends_.begin(), ends_.end(), at_) - ends_.begin());
}

void Player::step_once() {
  while (cursor_ < ends_.size() && at_ >= ends_[cursor_]) cursor_++;
  run_->step(log_.runs[cursor_].values);
  at_++;
  stats_.steps++;
  keeper_.offer(*run_, at_);
}

bool Player::advance(int64_t budget) {
  if (!run_) return false;
  if (target_ != at_) {
    int64_t copy_at = -1;
    keeper_.at_or_before(target_, &copy_at);
    // Behind the run, or a copy nearer the target than the run: start from it (or the setup).
    if (target_ < at_ || copy_at > at_) restart();
  }
  for (int64_t n = 0; n < budget && at_ < target_; n++) step_once();
  return at_ == target_;
}

}  // namespace ge::replay

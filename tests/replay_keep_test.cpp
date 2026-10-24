// The keyframes (engine/replay/keep.hpp): the policy's refusals, the thinning rule by age band,
// the kept set the same whatever steps were offered, the nearest copy at or before a step, and a
// game that clones nothing keeping nothing.
//
// ge:summary Tests: the keeper's policy checks, its thinning by age and the nearest copy
// ge:keywords test replay keyframe keep thin policy nearest at_or_before
// ge:skill replay testing
// ge:test tests/replay_keep_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/keep.hpp"

namespace gr = ge::replay;
namespace gp = ge::play;

namespace {

// A run that only counts its steps; `clones` false makes it a game that keeps no copies.
class Counter final : public gp::Run {
 public:
  explicit Counter(bool clones) : clones_(clones) {}
  void step(std::span<const double>) override { n_++; }
  bool over() const override { return false; }
  uint64_t digest() const override { return static_cast<uint64_t>(n_); }
  std::string observe() override { return "{}"; }
  std::string state() const override { return "{}"; }
  std::string report() const override { return "{}"; }
  std::unique_ptr<gp::Run> clone() const override {
    return clones_ ? std::make_unique<Counter>(*this) : nullptr;
  }
  int64_t n_ = 0;
  bool clones_;
};

// At 1 Hz: a copy every 1 s, all of the last 4 s, one in 2 back to 8 s, one in 4 back to 16 s.
gr::KeepPolicy small() { return {1, 4, 2, 8, 4, 16}; }

}  // namespace

TEST(policy_problems_name_the_field) {
  CHECK(gr::policy_problem({}, 120).empty());
  CHECK(gr::policy_problem(small(), 1).empty());
  CHECK(gr::policy_problem({1.5, 4, 3, 8, 6, 16}, 1).find("every_s") != std::string::npos);
  CHECK(gr::policy_problem({1, 4, 3, 8, 4, 16}, 1).find("far_every_s") != std::string::npos);
  CHECK(gr::policy_problem({2, 4, 3, 8, 6, 16}, 1).find("mid_every_s") != std::string::npos);
  CHECK(gr::policy_problem({1, 9, 2, 8, 4, 16}, 1).find("mid_s") != std::string::npos);
  CHECK(gr::policy_problem({1, 4, 2, 8, 4, 6}, 1).find("window_s") != std::string::npos);
  CHECK(gr::policy_problem({}, 0).find("hz") != std::string::npos);
}

TEST(thinning_by_age_band) {
  const gr::KeepSteps k = gr::keep_steps(small(), 1);
  CHECK(gr::keeps(k, 0, 1000));   // the first step always
  CHECK(gr::keeps(k, 97, 100));   // near: every copy
  CHECK(!gr::keeps(k, 95, 100));  // mid: odd steps go
  CHECK(gr::keeps(k, 94, 100));
  CHECK(!gr::keeps(k, 90, 100));  // far: only multiples of 4
  CHECK(gr::keeps(k, 88, 100));
  CHECK(!gr::keeps(k, 80, 100));  // past the window
  CHECK(!gr::keeps(k, 101, 100));
}

TEST(kept_set_is_the_rule_whatever_was_offered) {
  Counter run(true);
  gr::Keeper keep(small(), 1);
  for (int64_t s = 0; s <= 40; s++) {
    keep.offer(run, s);
    run.n_++;
  }
  const std::vector<int64_t> want{0, 24, 28, 32, 34, 36, 37, 38, 39, 40};
  CHECK(keep.kept_steps() == want);
  // A keeper offered only every third step keeps the rule's subset of what it saw.
  gr::Keeper sparse(small(), 1);
  for (int64_t s = 0; s <= 40; s += 3) sparse.offer(run, s);
  for (int64_t s : sparse.kept_steps()) CHECK(gr::keeps(sparse.steps(), s, 39));
  CHECK(!keep.offer(run, 40));  // not newer than the newest
}

TEST(nearest_copy_at_or_before) {
  Counter run(true);
  gr::Keeper keep(small(), 1);
  for (int64_t s = 0; s <= 40; s++) {
    run.n_ = s;
    keep.offer(run, s);
  }
  int64_t at = -1;
  const auto* copy = dynamic_cast<const Counter*>(keep.at_or_before(31, &at));
  CHECK(copy != nullptr && at == 28 && copy->n_ == 28);
  CHECK(keep.at_or_before(100, &at) != nullptr && at == 40);
  CHECK(keep.at_or_before(5, &at) != nullptr && at == 0);
  CHECK(keep.at_or_before(-1) == nullptr);
}

TEST(a_game_that_clones_nothing_keeps_nothing) {
  Counter run(false);
  gr::Keeper keep({}, 120);
  CHECK(!keep.offer(run, 0));
  CHECK(keep.size() == 0);
  CHECK(keep.at_or_before(0) == nullptr);
}

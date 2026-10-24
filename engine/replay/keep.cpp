// The keeper's rule and its copies: the policy turned into steps and checked, the thinning after
// each copy taken, the nearest copy at or before a step.
//
// ge:summary The keeper's definitions: the policy in steps, the thinning, the nearest copy
// ge:keywords replay keyframe keep thin policy_problem keep_steps keeps offer at_or_before
// ge:skill replay
// ge:test tests/replay_keep_test.cpp
#include "replay/keep.hpp"

#include <algorithm>

#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::replay {

namespace {

namespace num = core::num;

// `s` seconds at `hz` as a whole number of steps, or -1 when it is not one.
int64_t whole_steps(double s, double hz) {
  const double n = s * hz;
  const double r = num::round(n);
  return r == n && r >= 0 ? static_cast<int64_t>(r) : -1;
}

}  // namespace

std::string policy_problem(const KeepPolicy& p, double hz) {
  if (!(hz > 0)) return "the step rate must be above 0 (the game spec's hz)";
  const KeepSteps k = keep_steps(p, hz);
  const auto name = [&](const char* field, double s) {
    return std::string(field) + " = " + num::to_string(s) + " s";
  };
  if (k.every <= 0) {
    return name("every_s", p.every_s) + " is not a whole number of steps above 0 at " +
           num::to_string(hz) + " Hz; pick a multiple of 1/hz";
  }
  struct Spacing {
    const char* field;
    double s;
    int64_t steps, of;
    const char* of_field;
  };
  const Spacing spacings[] = {
      {"mid_every_s", p.mid_every_s, k.mid_every, k.every, "every_s"},
      {"far_every_s", p.far_every_s, k.far_every, k.mid_every, "mid_every_s"}};
  for (const Spacing& sp : spacings) {
    if (sp.steps <= 0 || sp.steps % sp.of != 0) {
      return name(sp.field, sp.s) + " is not a whole multiple of " + sp.of_field +
             "; make it one, so a copy dropped is never wanted again";
    }
  }
  if (k.near < 0 || k.mid < 0 || k.window < 0) {
    return "near_s, mid_s and window_s must be whole numbers of steps, 0 or more";
  }
  if (k.mid < k.near) return name("mid_s", p.mid_s) + " is shorter than near_s; raise it";
  if (k.window != 0 && k.window < k.mid) {
    return name("window_s", p.window_s) + " is shorter than mid_s; raise it, or 0 for no limit";
  }
  return "";
}

KeepSteps keep_steps(const KeepPolicy& p, double hz) {
  return {whole_steps(p.every_s, hz),     whole_steps(p.near_s, hz),
          whole_steps(p.mid_every_s, hz), whole_steps(p.mid_s, hz),
          whole_steps(p.far_every_s, hz), whole_steps(p.window_s, hz)};
}

bool keeps(const KeepSteps& k, int64_t step, int64_t newest) {
  if (step == 0) return true;
  const int64_t age = newest - step;
  if (age < 0 || step % k.every != 0) return false;
  if (age <= k.near) return true;
  if (age <= k.mid) return step % k.mid_every == 0;
  if (k.window == 0 || age <= k.window) return step % k.far_every == 0;
  return false;
}

Keeper::Keeper(const KeepPolicy& policy, double hz) : steps_(keep_steps(policy, hz)) {}

bool Keeper::offer(const play::Run& run, int64_t step) {
  if (step % steps_.every != 0 || step <= newest()) return false;
  std::unique_ptr<play::Run> copy = run.clone();
  if (!copy) return false;
  frames_.push_back({step, std::move(copy)});
  std::erase_if(frames_, [&](const Frame& f) { return !keeps(steps_, f.step, step); });
  return true;
}

const play::Run* Keeper::at_or_before(int64_t step, int64_t* at) const {
  const auto it = std::upper_bound(frames_.begin(), frames_.end(), step,
                                   [](int64_t s, const Frame& f) { return s < f.step; });
  if (it == frames_.begin()) return nullptr;
  const Frame& f = *(it - 1);
  if (at) *at = f.step;
  return f.run.get();
}

std::vector<int64_t> Keeper::kept_steps() const {
  std::vector<int64_t> out;
  out.reserve(frames_.size());
  for (const Frame& f : frames_) out.push_back(f.step);
  return out;
}

}  // namespace ge::replay

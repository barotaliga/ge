// The humanness metrics, their bands and the out-of-band check (human.hpp).
//
// ge:summary axis_humanness, decision_humanness, the stated bands and the out-of-band lines
// ge:keywords ai humanness saturation jumps reversals hold lag bands entropy flips
// ge:skill bots
// ge:test tests/ai_human_test.cpp
#include "ai/human.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::ai {

namespace {

namespace num = ge::core::num;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInUse = 0.05;    // off its rest by more than this, the axis is in use
constexpr double kAtEnd = 0.02;    // within this of an end, it is held there
constexpr double kReverse = 0.05;  // a move this far the other way is a change of direction
constexpr double kJump = 0.5;      // a step's move over this share of the travel is a jump
constexpr double kStill = 0.02;    // within this of where a hold began, the axis holds still

constexpr std::array<HumanBand, 6> kBands{{
    {"saturated", Feel::stick, 0, 0.35,
     "a thumb on a stick rides near the rim on the hardest turns only"},
    {"jumps", Feel::any, 0, 0.01,
     "a hand needs about 50 ms to cross half a control's travel, six steps at 120 Hz"},
    {"reversals_min", Feel::any, 0, 120,
     "a person corrects a held control about twice a second at most"},
    {"hold_s", Feel::stick, 0.05, 3,
     "a thumb settles a stick part-way for longer than a step, and never perfectly still for long "
     "(a trigger part-way is only passing through)"},
    {"lag_s", Feel::any, 0.15, 0.4,
     "a seen change takes 150 to 400 ms to reach the hand: simple reaction plus the move"},
    {"flips_min", Feel::any, 0, 20,
     "a person changes their mind a few times a minute, not every second"},
}};

// The hold a step picked at random is in: the median of the holds weighted by their length, so a
// move through a dozen values in a tenth of a second does not outvote a second held still.
double median_by_time(std::vector<double> holds) {
  if (holds.empty()) return kNaN;
  std::sort(holds.begin(), holds.end());
  double total = 0;
  for (const double h : holds) total += h;
  double seen = 0;
  for (const double h : holds) {
    seen += h;
    if (seen >= total / 2) return h;
  }
  return holds.back();
}

}  // namespace

const std::span<const HumanBand> kHumanBands{kBands};

AxisHumanness axis_humanness(std::span<const double> pressed, std::span<const double> wanted,
                             double lo, double hi, double rest, double hz) {
  AxisHumanness h;
  h.steps = static_cast<int64_t>(pressed.size());
  h.lag_s = kNaN;
  if (pressed.empty() || hz <= 0) return h;
  const double travel = hi - lo;
  int64_t in_use = 0, at_end = 0, jumps = 0, reversals = 0;
  double moved = 0;
  // The direction last moved (+1, -1, 0 none yet) and the extreme reached that way.
  int dir = 0;
  double turn = pressed[0];
  // A hold counts where the axis is pressed part-way: let go or at a stop it may rest as long as
  // it likes.
  const auto between = [&](double v) {
    return std::fabs(v - rest) > kInUse && v < hi - kAtEnd && v > lo + kAtEnd;
  };
  std::vector<double> holds;
  int64_t held = 1;
  double hold_at = pressed[0];
  for (size_t i = 0; i < pressed.size(); i++) {
    const double v = pressed[i];
    if (std::fabs(v - rest) > kInUse) {
      in_use++;
      at_end += (v >= hi - kAtEnd || v <= lo + kAtEnd) ? 1 : 0;
    }
    if (i == 0) continue;
    const double d = v - pressed[i - 1];
    moved += std::fabs(d);
    jumps += std::fabs(d) > kJump * travel ? 1 : 0;
    if (std::fabs(v - hold_at) <= kStill) {
      held++;
    } else {
      if (between(hold_at)) holds.push_back(static_cast<double>(held) / hz);
      held = 1;
      hold_at = v;
    }
    // A change of direction: kReverse back from the furthest point reached the last way.
    if (dir == 0) {
      if (v > turn + kReverse || v < turn - kReverse) dir = v > turn ? 1 : -1;
      if (dir != 0) turn = v;
    } else if (dir * (v - turn) > 0) {
      turn = v;
    } else if (dir * (turn - v) > kReverse) {
      reversals++;
      dir = -dir;
      turn = v;
    }
  }
  if (between(hold_at)) holds.push_back(static_cast<double>(held) / hz);
  const double seconds = static_cast<double>(pressed.size()) / hz;
  h.saturated = in_use > 0 ? static_cast<double>(at_end) / static_cast<double>(in_use) : 0;
  h.jumps = static_cast<double>(jumps) / static_cast<double>(pressed.size());
  h.reversals_min = static_cast<double>(reversals) * 60 / seconds;
  h.rate = moved / seconds;
  h.hold_s = median_by_time(std::move(holds));
  const auto [want_lo, want_hi] = std::minmax_element(wanted.begin(), wanted.end());
  if (wanted.size() == pressed.size() && !wanted.empty() && *want_hi - *want_lo > kReverse) {
    // A wanted that never moves has no lag to find. The delay at which the pressed best follows the
    // wanted: the least mean |difference|.
    const auto most = static_cast<size_t>(num::round(kMostLag_s * hz));
    double best = std::numeric_limits<double>::infinity();
    for (size_t lag = 0; lag <= most && lag < pressed.size(); lag++) {
      double sum = 0;
      for (size_t i = lag; i < pressed.size(); i++) sum += std::fabs(pressed[i] - wanted[i - lag]);
      const double mean = sum / static_cast<double>(pressed.size() - lag);
      if (mean < best - 1e-12) {
        best = mean;
        h.lag_s = static_cast<double>(lag) / hz;
      }
    }
  }
  return h;
}

DecisionHumanness decision_humanness(const Why& why, int32_t agent, const char* name,
                                     double seconds, double close) {
  DecisionHumanness h;
  const char* last = nullptr;
  int64_t flips = 0, with_margin = 0, tight = 0;
  // The choices taken and how often, by name (a decision has a handful).
  std::vector<std::pair<std::string_view, int64_t>> taken;
  for (const Decision& d : why.decisions()) {
    if (d.agent != agent || std::string_view(d.name) != name) continue;
    h.calls++;
    if (last && std::string_view(last) != d.chosen) flips++;
    last = d.chosen;
    if (!std::isnan(d.margin)) {
      with_margin++;
      tight += d.margin < close ? 1 : 0;
    }
    bool found = false;
    for (auto& [c, n] : taken) {
      if (c == d.chosen) {
        n++;
        found = true;
      }
    }
    if (!found) taken.emplace_back(d.chosen, 1);
  }
  h.flips_min = seconds > 0 ? static_cast<double>(flips) * 60 / seconds : 0;
  h.close_share =
      with_margin > 0 ? static_cast<double>(tight) / static_cast<double>(with_margin) : 0;
  if (taken.size() > 1 && h.calls > 0) {
    double entropy = 0;
    for (const auto& [c, n] : taken) {
      const double p = static_cast<double>(n) / static_cast<double>(h.calls);
      entropy -= p * core::libm::log(p);
    }
    h.variety = entropy / core::libm::log(static_cast<double>(taken.size()));
  }
  return h;
}

namespace {

void check(const std::string& axis, Feel feel, const char* metric, double value,
           std::vector<OutOfBand>& out) {
  if (std::isnan(value)) return;
  for (const HumanBand& b : kBands) {
    if (std::string_view(b.metric) != metric) continue;
    if (b.feel != Feel::any && b.feel != feel) continue;
    if (value < b.lo || value > b.hi) out.push_back({axis, b.metric, value, b.lo, b.hi, b.because});
  }
}

}  // namespace

void check_axis(const std::string& axis, Feel feel, const AxisHumanness& h,
                std::vector<OutOfBand>& out) {
  check(axis, feel, "saturated", h.saturated, out);
  check(axis, feel, "jumps", h.jumps, out);
  check(axis, feel, "reversals_min", h.reversals_min, out);
  check(axis, feel, "hold_s", h.hold_s, out);
  check(axis, feel, "lag_s", h.lag_s, out);
}

void check_decision(const std::string& name, const DecisionHumanness& h,
                    std::vector<OutOfBand>& out) {
  check(name, Feel::any, "flips_min", h.flips_min, out);
}

std::string out_of_band_text(const OutOfBand& o) {
  return o.axis + " " + o.metric + " " + core::num::to_fixed(o.value, 3) + " (band " +
         core::num::to_fixed(o.lo, 2) + ".." + core::num::to_fixed(o.hi, 2) + "): " + o.because;
}

}  // namespace ge::ai

// Fall Line's moments (moments.hpp): the rules' table, and the events of a step filed as marks.
//
// ge:summary Fall Line's moments' definitions: the shot rules, the policy, Moments::file
// ge:keywords moments shot_rules director_policy file flight weight
// ge:skill sim replay
// ge:test tests/fall_line_play_test.cpp
#include "fall_line/sim/moments.hpp"

#include <vector>

namespace fall_line::sim {

namespace {

// A shoulder closing slower than this is a brush, not a moment, m/s.
constexpr double kLeastBump = 2;

}  // namespace

std::span<const ge::replay::ShotRule> shot_rules() {
  // kind, lead s, hold s, rate, scale, lens
  static const std::vector<ge::replay::ShotRule> rules{
      {"flight", 2.5, 3, 0.5, 1, "side"},  {"wipeout", 2, 3, 0.5, 1, "chase"},
      {"shoulder", 1.5, 2, 1, 1, "front"}, {"missed", 2, 1.5, 1, 1, "chase"},
      {"finish", 3, 2, 1, 1, "aerial"},
  };
  return rules;
}

ge::replay::DirectorPolicy director_policy() { return {3, 40, 8}; }

void Moments::file(const GameState& state, ge::replay::Live& live) {
  for (const GameEvent& e : state.events) {
    const bool shoulder = e.kind == EventKind::shoulder && (e.racer == 0 || e.gate == 0);
    if (e.racer != 0 && !shoulder) continue;
    switch (e.kind) {
      case EventKind::takeoff:
        takeoff_ = live.steps();
        break;
      case EventKind::landing:
        if (takeoff_ >= 0) {
          const double air = static_cast<double>(live.steps() - takeoff_) / kHz;
          live.mark_at(takeoff_, "flight", 1 + 2 * air + e.value / 3);
        }
        takeoff_ = -1;
        break;
      case EventKind::wipeout:
        takeoff_ = -1;
        live.mark("wipeout", 4);
        break;
      case EventKind::shoulder:
        if (e.value >= kLeastBump) live.mark("shoulder", e.value / 3);
        break;
      case EventKind::missed:
        live.mark("missed", 1.5);
        break;
      case EventKind::reset:
        live.mark("reset", 0.5);
        break;
      case EventKind::finish:
        live.mark("finish", 3);
        break;
      default:
        break;
    }
  }
}

}  // namespace fall_line::sim

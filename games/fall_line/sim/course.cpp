// The course's gates, finish and reset (course.hpp).
//
// ge:summary Gate crossings, the miss, the finish's clock, the reset pose
// ge:keywords course crossed_line step_course reset_racer bearing_to_next
// ge:skill sim
// ge:test tests/fall_line_sim_test.cpp
#include "fall_line/sim/course.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "mountain/mapgen/query.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace libm = ge::core::libm;

bool crossed_line(const mg::Checkpoint& cp, double x0, double z0, double x1, double z1,
                  double& share, double& lateral) {
  const double fx = libm::sin(cp.heading);
  const double fz = libm::cos(cp.heading);
  const double s0 = (x0 - cp.x) * fx + (z0 - cp.z) * fz;
  const double s1 = (x1 - cp.x) * fx + (z1 - cp.z) * fz;
  if (!(s0 < 0 && s1 >= 0)) return false;
  share = s0 / (s0 - s1);
  const double cx = x0 + (x1 - x0) * share;
  const double cz = z0 + (z1 - z0) * share;
  lateral = (cx - cp.x) * fz - (cz - cp.z) * fx;
  return true;
}

void step_course(GameState& state, int32_t i, double x0, double z0) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  Progress& p = r.progress;
  const auto& gates = state.level->checkpoints;
  if (p.finished || p.next >= static_cast<int32_t>(gates.size())) return;
  const mg::Checkpoint& cp = gates[static_cast<size_t>(p.next)];
  double share = 0;
  double lateral = 0;
  if (!crossed_line(cp, x0, z0, r.skier.x, r.skier.z, share, lateral)) return;
  const double off = lateral < 0 ? -lateral : lateral;
  if (off > cp.width / 2 + kGrace) {
    if (!p.missed) {
      p.missed = true;
      p.missed_gates++;
      state.events.push_back(GameEvent{EventKind::missed, i, p.next, off});
    }
    return;
  }
  state.events.push_back(GameEvent{EventKind::gate, i, p.next, lateral});
  p.missed = false;
  p.next++;
  if (p.next == static_cast<int32_t>(gates.size())) {
    p.finished = true;
    p.finish_time = state.t + share * kDt;
    state.events.push_back(GameEvent{EventKind::finish, i, p.next - 1, p.finish_time});
  }
}

void reset_racer(GameState& state, int32_t i) {
  Racer& r = state.racers[static_cast<size_t>(i)];
  Progress& p = r.progress;
  const mg::Level& level = *state.level;
  double x, z, heading;
  if (p.next == 0) {
    const mg::Spawn& slot = level.grid[static_cast<size_t>(i) % level.grid.size()];
    x = slot.x;
    z = slot.z;
    heading = slot.heading;
  } else {
    const mg::Checkpoint& last = level.checkpoints[static_cast<size_t>(p.next - 1)];
    const mg::TrackPoint at = mg::track_point_at(level.track, last.s + kResetPast);
    x = at.x;
    z = at.z;
    heading = at.heading;
  }
  r.skier = ge::skier::stand_skier(state.world(), x, z, heading, 0);
  p.missed = false;
  p.stuck = 0;
  p.resets++;
  state.events.push_back(GameEvent{EventKind::reset, i, p.next, 0});
}

double bearing_to_next(const mg::Level& level, const Progress& p, double x, double z) {
  const auto n = static_cast<int32_t>(level.checkpoints.size());
  const mg::Checkpoint& cp = level.checkpoints[static_cast<size_t>(p.next < n ? p.next : n - 1)];
  return libm::atan2(cp.x - x, cp.z - z);
}

}  // namespace fall_line::sim

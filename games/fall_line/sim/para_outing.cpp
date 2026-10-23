// The bot's outing under the paramotor (para_outing.hpp).
//
// ge:summary para_landing and the paramotor outing's pilot: the stretch, the spiral, the final
// ge:keywords para-outing landing stretch spiral final pilot para_as_controls
// ge:skill sim paramotor
// ge:test tests/fall_line_para_test.cpp
#include "fall_line/sim/para_outing.hpp"

#include "core/math.hpp"
#include "core/num.hpp"
#include "fall_line/sim/free.hpp"
#include "mountain/mapgen/query.hpp"
#include "paramotor/pilot.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace pm = ge::paramotor;
namespace num = ge::core::num;
using namespace para_outing;

namespace {

// The steepest the chain falls over any `kGradeOver` m from `s0` to `s1`, rise over run.
double steepest(const mg::Track& chain, double s0, double s1) {
  double most = 0;
  for (double s = s0; s + kGradeOver <= s1; s += kGradeOver / 3) {
    const double a = mg::track_point_at(chain, s).y;
    const double b = mg::track_point_at(chain, s + kGradeOver).y;
    most = num::max(most, (a - b) / kGradeOver);
  }
  return most;
}

}  // namespace

ParaLanding para_landing(const mg::Level& level, int32_t run) {
  const mg::Track chain = run_chain(level, run);
  ParaLanding at;
  if (chain.points.size() < 2) return at;
  const double top = chain.points.front().y;
  const double fall = top - chain.points.back().y;
  double from = chain.length;
  for (const mg::TrackPoint& p : chain.points) {
    if (top - p.y >= fall * kLandShare) {
      from = p.s;
      break;
    }
  }
  double head = num::max(0.0, chain.length - kStretch);
  for (double s = from; s + kStretch <= chain.length; s += kGradeOver) {
    if (steepest(chain, s, s + kStretch) <= kGentle) {
      head = s;
      break;
    }
  }
  const mg::TrackPoint p = mg::track_point_at(chain, head);
  return ParaLanding{p.x, p.z, head, num::min(chain.length, head + kStretch)};
}

Controls para_as_controls(const pm::ParaControls& p) {
  Controls c;
  c.ski.tuck = p.throttle;
  c.ski.brake = p.brake;
  c.ski.steer = p.steer;
  c.lean = -p.bar;
  return c;
}

ParaOutingPilot::ParaOutingPilot(const GameState& state) {
  const int32_t run = state.racers[0].free.start;
  landing_ = para_landing(*state.level, run);
  chain_ = run_chain(*state.level, run);
  down_ = plan_free(*state.level, run, state.spec);
}

Controls ParaOutingPilot::operator()(const GameState& state, int32_t i) const {
  if (!under_para(state, i)) return bot_controls(state, i, down_);
  const pm::ParaState& p = state.para.body;
  const ge::skier::SkierState& s = state.racers[static_cast<size_t>(i)].skier;
  const pm::ParaWorld world = para_world(state);
  pm::ParaAim aim;
  aim.has_aim = true;
  aim.x = landing_.x;
  aim.z = landing_.z;
  aim.height = kHold;
  if (!p.flying) return para_as_controls(pm::para_pilot(p, s, world, aim));
  const double to_head = ge::core::hypot(s.x - landing_.x, s.z - landing_.z);
  const mg::TrackHit hit = mg::nearest_track_point(chain_, s.x, s.z);
  const bool on_stretch =
      hit.s >= landing_.s - kOver && hit.s <= landing_.end && hit.distance < kBeside;
  if (p.agl < kFinal && (on_stretch || to_head < kOver)) {
    // THE FINAL: along the run, the throttle shut, the flare at the snow.
    const mg::TrackPoint ahead = mg::track_point_at(chain_, hit.s + kLead);
    aim.x = ahead.x;
    aim.z = ahead.z;
    aim.land = true;
    pm::ParaControls c = pm::para_pilot(p, s, world, aim);
    c.throttle = 0;
    return para_as_controls(c);
  }
  pm::ParaControls c = pm::para_pilot(p, s, world, aim);
  if (to_head < kOver) {
    // OVER THE STRETCH, high: spiral down on a toggle, the throttle shut.
    c.throttle = 0;
    c.steer = kSpiral;
  }
  return para_as_controls(c);
}

}  // namespace fall_line::sim

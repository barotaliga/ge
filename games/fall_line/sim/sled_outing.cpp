// The bot's outing on the snowmobile (sled_outing.hpp).
//
// ge:summary sled_climb_run, sled_up_line, stand_by_sled and the sled outing's pilot
// ge:keywords sled-outing climb up-line pilot stand_by_sled
// ge:skill sim snowmobile
// ge:test tests/fall_line_sled_test.cpp
#include "fall_line/sim/sled_outing.hpp"

#include <cmath>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/quat.hpp"
#include "fall_line/sim/free.hpp"
#include "fall_line/sim/outing.hpp"
#include "mountain/mapgen/query.hpp"
#include "skier/skier.hpp"
#include "snowmobile/pilot.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace sm = ge::snowmobile;
namespace core = ge::core;
namespace num = ge::core::num;
using namespace sled_outing;

int32_t sled_climb_run(const mg::Level& level, const SledSpot& spot) {
  if (!level.resort) return -1;
  const auto& runs = level.resort->runs;
  int32_t best = -1;
  double best_d = 0;
  // Green and blue first; any piste where there is neither.
  for (const bool easy : {true, false}) {
    for (size_t k = 0; k < runs.size(); k++) {
      const mg::Run& r = runs[k];
      if (r.kind != mg::RunKind::piste || r.into || r.points.size() < 2) continue;
      if (easy && r.grade != mg::PisteGrade::green && r.grade != mg::PisteGrade::blue) continue;
      const mg::TrackPoint& end = r.points.back();
      const double d = core::hypot(end.x - spot.x, end.z - spot.z);
      if (best < 0 || d < best_d) {
        best = static_cast<int32_t>(k);
        best_d = d;
      }
    }
    if (best >= 0) return best;
  }
  return best;
}

mg::Track sled_up_line(const mg::Level& level, const SledSpot& spot, int32_t run) {
  LineLayer line(level);
  line.to(spot.x, spot.z, outing::kWalkWidth);
  if (level.resort && run >= 0) {
    const mg::Run& r = level.resort->runs[static_cast<size_t>(run)];
    for (size_t k = r.points.size(); k-- > 0;) {
      const mg::TrackPoint& p = r.points[k];
      if (p.s < kStopDown) break;
      line.to(p.x, p.z, p.width);
    }
  }
  return line.done();
}

void stand_by_sled(GameState& state, int32_t i) {
  const sm::SledState& b = state.sled.body;
  const core::Vec3 left = core::rotate(b.q, core::Vec3{-1, 0, 0});
  const double x = b.x + left.x * sled_ride::kStandOff * 2;
  const double z = b.z + left.z * sled_ride::kStandOff * 2;
  state.racers[static_cast<size_t>(i)].skier =
      ge::skier::stand_skier(state.world(), x, z, b.heading, 0);
}

SledOutingPilot::SledOutingPilot(const GameState& state, int32_t run) : run_(run) {
  const mg::Level& level = *state.level;
  const SledSpot spot{state.sled.body.x, state.sled.body.z, state.sled.body.heading};
  up_ = sled_up_line(level, spot, run);
  // Ask once here, so a step never builds the line's index.
  if (up_.points.size() > 1) mg::nearest_track_point(up_, up_.points[0].x, up_.points[0].z);
  down_ = plan_line(level, run_chain(level, run), state.spec);
}

Controls SledOutingPilot::operator()(const GameState& state, int32_t i) const {
  const FreeSled& m = state.sled;
  Controls c;
  if (m.thrown) return c;  // put back on it by the run's reset
  if (!m.rider) {
    if (m.taken == 0) {
      c.machine = true;
      return c;
    }
    return bot_controls(state, i, down_);
  }
  const sm::SledState& b = m.body;
  const mg::TrackHit hit = mg::nearest_track_point(up_, b.x, b.z);
  if (hit.distance < kOnLine) reached_ = num::max(reached_, hit.s);
  if (reached_ >= up_.length - kStopShort) {
    c.ski.brake = 1;
    c.machine = b.speed < kHopUnder;
    return c;
  }
  const mg::TrackPoint aim = mg::track_point_at(up_, reached_ + kAimAhead);
  const sm::SledControls s =
      sm::sled_pilot(b, sled_world(state), sm::SledAim{aim.x, aim.z, kCruise});
  c.ski.tuck = s.throttle;
  c.ski.brake = s.brake;
  c.ski.steer = s.steer;
  c.lean = s.lean;
  return c;
}

}  // namespace fall_line::sim

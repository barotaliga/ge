// The bot's outing on the helicopter (heli_outing.hpp).
//
// ge:summary heli_drop_run, stand_by_heli and the helicopter outing's pilot
// ge:keywords heli-outing drop run head pilot stand_by_heli heli_as_controls
// ge:skill sim helicopter
// ge:test tests/fall_line_heli_test.cpp
#include "fall_line/sim/heli_outing.hpp"

#include "core/math.hpp"
#include "core/quat.hpp"
#include "fall_line/sim/free.hpp"
#include "helicopter/pilot.hpp"
#include "helicopter/scenarios.hpp"
#include "skier/skier.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace hc = ge::helicopter;
namespace core = ge::core;
using namespace heli_outing;

int32_t heli_drop_run(const mg::Level& level) {
  if (!level.resort) return -1;
  const auto& runs = level.resort->runs;
  int32_t best = -1;
  double best_y = 0;
  // Green, blue and red first; a black only where there is nothing else.
  for (const bool easy : {true, false}) {
    for (size_t k = 0; k < runs.size(); k++) {
      const mg::Run& r = runs[k];
      if (r.kind != mg::RunKind::piste || r.points.size() < 2) continue;
      if (easy && r.grade == mg::PisteGrade::black) continue;
      const double y = r.points.front().y;
      if (best < 0 || y > best_y) {
        best = static_cast<int32_t>(k);
        best_y = y;
      }
    }
    if (best >= 0) return best;
  }
  return best;
}

void stand_by_heli(GameState& state, int32_t i) {
  const hc::HeliState& b = state.heli.body;
  const core::Vec3 right = core::rotate(hc::heli_quat(b), core::Vec3{1, 0, 0});
  const core::Vec3 s = heli_seat(b);
  state.racers[static_cast<size_t>(i)].skier =
      ge::skier::stand_skier(state.world(), s.x + right.x * 2, s.z + right.z * 2, b.heading, 0);
}

Controls heli_as_controls(const hc::HeliControls& h) {
  Controls c;
  c.lever = h.collective;
  c.lean = -h.pitch;
  c.ski.steer = h.roll;
  c.pedal = h.pedal;
  return c;
}

HeliOutingPilot::HeliOutingPilot(const GameState& state, int32_t run) : run_(run) {
  const mg::Level& level = *state.level;
  if (level.resort && run >= 0) {
    const mg::TrackPoint& head = level.resort->runs[static_cast<size_t>(run)].points.front();
    head_x_ = head.x;
    head_z_ = head.z;
  }
  down_ = plan_line(level, run_chain(level, run), state.spec);
}

Controls HeliOutingPilot::operator()(const GameState& state, int32_t i, ge::ai::Why* why) const {
  const FreeHeli& m = state.heli;
  if (!m.rider) {
    if (m.taken == 0) {
      if (why) {
        why->decide("outing");
        why->fact("taken", m.taken);
        why->choose("board", "beside_it");
      }
      Controls c;
      c.machine = true;
      return c;
    }
    if (why) {
      why->decide("outing");
      why->fact("taken", m.taken);
      why->choose("ski_down", "dropped");
    }
    return bot_controls(state, i, down_, why);
  }
  const hc::HeliState& b = m.body;
  const double near = core::hypot(b.x - head_x_, b.z - head_z_);
  const double flat = core::hypot(b.vx, b.vz);
  const double over = b.y - state.level->ground_at(b.x, b.z);
  const bool drop = near < kDropNear && flat < kDropSlow && over < kDropHigh;
  if (why) {
    why->decide("outing");
    why->fact("head_m", near);
    why->fact("flat_ms", flat);
    why->fact("skids_m", over);
    why->choose(drop ? "drop" : "fly", drop ? "hovering_over_the_head" : "not_there_yet");
  }
  if (drop) {
    Controls c = heli_as_controls(m.flown);
    c.machine = true;
    return c;
  }
  hc::HeliAim aim;
  aim.x = head_x_;
  aim.z = head_z_;
  aim.height = near < kApproach ? kDropOver : kCruiseOver;
  const hc::HeliLoad load{state.spec.mass, hc::kSeatX};
  return heli_as_controls(hc::heli_pilot(b, heli_world(state), load, aim, why));
}

}  // namespace fall_line::sim

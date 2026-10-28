// The village on the free ride: planned at create, the skier against its traffic (village.hpp).
//
// ge:summary plan_village and meet_traffic: the pads passed, the trees felled, a vehicle's box
// ge:keywords village plan_village meet_traffic vehicle-box parked felled pads struck
// ge:skill sim
// ge:test tests/fall_line_village_test.cpp
#include "fall_line/sim/village.hpp"

#include <array>
#include <cmath>
#include <memory>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "mountain/mapgen/types.hpp"
#include "mountain/village/traffic_rules.hpp"
#include "skier/skier.hpp"
#include "skier/trees.hpp"
#include "skier/tuning.hpp"

namespace fall_line::sim {

namespace {

namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;
namespace sk = ge::skier;
namespace core = ge::core;
namespace lm = ge::core::libm;

constexpr double kSkierRadius = sk::tuning::wipeout::kBodyRadius;

// A vehicle's body at a moment: its middle on the snow, the way it faces, its velocity in the
// plan, and its size.
struct Body {
  double x, y, z;
  double heading;
  double vx, vz;
  vg::VehicleSize size;
};

// HIM AGAINST ONE BODY: held out of its box (the nearest point of its plan to him, his circle
// round that), the closing speed traded back. Returns the closing speed met (0 for none).
double meet_body(sk::SkierState& c, const Body& b) {
  const vg::VehicleSize& v = b.size;
  if (c.y > b.y + v.height || c.y < b.y - 2) return 0;
  const double hl = v.length / 2;
  const double hw = v.width / 2;
  const double reach = hl + kSkierRadius;
  if (std::fabs(c.x - b.x) > reach || std::fabs(c.z - b.z) > reach) return 0;
  const double fx = lm::sin(b.heading);
  const double fz = lm::cos(b.heading);
  // In the body's own frame: u along it, w to its right ((cos h, -sin h)).
  const double rx = c.x - b.x;
  const double rz = c.z - b.z;
  const double u = rx * fx + rz * fz;
  const double w = rx * fz - rz * fx;
  const double cu = core::clamp(u, -hl, hl);
  const double cw = core::clamp(w, -hw, hw);
  double nu = u - cu;
  double nw = w - cw;
  double d = core::hypot(nu, nw);
  double pen = kSkierRadius - d;
  if (d < 1e-9) {
    // Inside the body: out through the nearest face.
    const double to_side = hw - std::fabs(w);
    const double to_end = hl - std::fabs(u);
    if (to_side <= to_end) {
      nu = 0;
      nw = w < 0 ? -1 : 1;
      pen = to_side + kSkierRadius;
    } else {
      nu = u < 0 ? -1 : 1;
      nw = 0;
      pen = to_end + kSkierRadius;
    }
    d = 1;
  }
  if (pen <= 0) return 0;
  nu /= d;
  nw /= d;
  const double nx = nu * fx + nw * fz;
  const double nz = nu * fz - nw * fx;
  c.x += nx * pen;
  c.z += nz * pen;
  const double closing = -((c.vx - b.vx) * nx + (c.vz - b.vz) * nz);
  if (closing <= 0) return 0;
  c.vx += (1 + village::kRestitution) * closing * nx;
  c.vz += (1 + village::kRestitution) * closing * nz;
  c.speed = core::hypot3(c.vx, c.vy, c.vz);
  return closing;
}

void struck(GameState& state, int32_t i, int32_t what, double closing) {
  sk::SkierState& c = state.racers[static_cast<size_t>(i)].skier;
  if (closing >= village::kEventSpeed) {
    state.events.push_back(GameEvent{EventKind::traffic, i, what, closing});
  }
  if (closing < village::kThrowSpeed || c.down) return;
  sk::put_down(c, sk::Wipeout::tree);
  state.racers[static_cast<size_t>(i)].progress.wipeouts++;
  state.events.push_back(GameEvent{EventKind::wipeout, i, static_cast<int32_t>(c.cause), c.speed});
}

}  // namespace

void plan_village(GameState& state) {
  state.village.reset();
  const mg::Level& level = *state.level;
  std::array<mg::PlanPoint, 2> pads{};
  size_t n = 0;
  if (state.sled.present) pads[n++] = mg::PlanPoint{state.army.pad_x, state.army.pad_z};
  if (state.heli.present) pads[n++] = mg::PlanPoint{state.heli.pad_x, state.heli.pad_z};
  std::optional<vg::Streets> streets =
      vg::plan_streets(level, std::span<const mg::PlanPoint>(pads.data(), n));
  if (!streets) return;
  auto v = std::make_shared<Village>();
  v->streets = std::move(*streets);
  v->ground = vg::bake_ground(v->streets);
  v->felled = vg::felled_trees(level, v->ground);
  for (const uint8_t f : v->felled) v->felled_count += f;
  v->traffic = vg::plan_traffic(level, v->streets);
  state.trees = sk::build_tree_index(level, v->felled);
  state.village = std::move(v);
}

void meet_traffic(GameState& state) {
  if (!state.village) return;
  const std::optional<vg::TrafficPlan>& traffic = state.village->traffic;
  if (!traffic) return;
  const vg::TrafficPlan& plan = *traffic;
  const mg::Level& level = *state.level;
  for (int32_t i = 0; i < static_cast<int32_t>(state.racers.size()); i++) {
    Racer& racer = state.racers[static_cast<size_t>(i)];
    // On a lift or a machine he is the machine's: no car meets him.
    if (racer.free.lift.phase != LiftPhase::none) continue;
    if (on_sled(state, i) || (i == 0 && state.heli.rider)) continue;
    sk::SkierState& c = racer.skier;
    if (!vg::near_traffic(plan, c.x, c.z, 2)) continue;
    for (int32_t k = 0; k < static_cast<int32_t>(plan.vehicles.size()); k++) {
      const vg::VehiclePose p = vg::vehicle_at(level, plan, k, state.t);
      if (!p.shown) continue;
      const double vx = p.speed * lm::sin(p.heading);
      const double vz = p.speed * lm::cos(p.heading);
      const double closing =
          meet_body(c, Body{p.x, p.y, p.z, p.heading, vx, vz, vg::vehicle_size(p.kind)});
      if (closing > 0) struck(state, i, k, closing);
    }
    for (const vg::Parked& p : plan.parked) {
      const double closing =
          meet_body(c, Body{p.x, p.y, p.z, p.heading, 0, 0, vg::vehicle_size(p.kind)});
      if (closing > 0) struck(state, i, -1 - p.id, closing);
    }
  }
}

}  // namespace fall_line::sim

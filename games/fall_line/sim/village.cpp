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
#include "core/num.hpp"
#include "fall_line/sim/sled_ride.hpp"
#include "mountain/mapgen/types.hpp"
#include "mountain/village/buildings.hpp"
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
namespace num = ge::core::num;

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
  const double reach = core::hypot(hl, hw) + kSkierRadius;
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

void struck(GameState& state, int32_t i, EventKind kind, int32_t what, double closing) {
  sk::SkierState& c = state.racers[static_cast<size_t>(i)].skier;
  if (closing >= village::kEventSpeed) {
    state.events.push_back(GameEvent{kind, i, what, closing});
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
  v->traffic = vg::plan_traffic(level, v->streets);
  v->buildings =
      vg::place_buildings(level, v->streets, v->ground,
                          vg::clearance_of(level, std::span<const mg::PlanPoint>(pads.data(), n)));
  vg::fell_for_buildings(level, v->buildings, v->felled);
  v->felled_count = 0;
  for (const uint8_t f : v->felled) v->felled_count += f;
  v->box[0] = v->ground.x0;
  v->box[1] = v->ground.z0;
  v->box[2] = v->ground.x0 + v->ground.cols * vg::GroundMask::kCell;
  v->box[3] = v->ground.z0 + v->ground.rows * vg::GroundMask::kCell;
  for (const vg::Building& b : v->buildings) {
    const double r = core::hypot(b.width, b.depth) / 2;
    v->box[0] = num::min(v->box[0], b.x - r);
    v->box[1] = num::min(v->box[1], b.z - r);
    v->box[2] = num::max(v->box[2], b.x + r);
    v->box[3] = num::max(v->box[3], b.z + r);
  }
  state.trees = sk::build_tree_index(level, v->felled);
  state.village = std::move(v);
}

void meet_traffic(GameState& state) {
  if (!state.village) return;
  const Village& village = *state.village;
  const std::optional<vg::TrafficPlan>& traffic = village.traffic;
  const mg::Level& level = *state.level;
  for (int32_t i = 0; i < static_cast<int32_t>(state.racers.size()); i++) {
    Racer& racer = state.racers[static_cast<size_t>(i)];
    // On a lift or a machine he is the machine's: no car or wall meets him.
    if (racer.free.lift.phase != LiftPhase::none) continue;
    if (on_sled(state, i) || (i == 0 && state.heli.rider)) continue;
    sk::SkierState& c = racer.skier;
    if (c.x < village.box[0] || c.z < village.box[1] || c.x > village.box[2] ||
        c.z > village.box[3]) {
      continue;
    }
    // The walls: a building is a body that never moves, as tall as its ridge (its spire's for
    // the church).
    for (const vg::Building& b : village.buildings) {
      vg::VehicleSize size{};
      size.length = b.depth;
      size.width = b.width;
      size.height =
          (b.kind == vg::BuildingKind::church ? vg::buildings::kSpire : b.ridge) + (b.y - b.low);
      const double closing = meet_body(c, Body{b.x, b.low, b.z, b.heading, 0, 0, size});
      if (closing > 0) struck(state, i, EventKind::building, b.id, closing);
    }
    if (!traffic) continue;
    const vg::TrafficPlan& plan = *traffic;
    if (!vg::near_traffic(plan, c.x, c.z, 2)) continue;
    for (int32_t k = 0; k < static_cast<int32_t>(plan.vehicles.size()); k++) {
      const vg::VehiclePose p = vg::vehicle_at(level, plan, k, state.t);
      if (!p.shown) continue;
      const double vx = p.speed * lm::sin(p.heading);
      const double vz = p.speed * lm::cos(p.heading);
      const double closing =
          meet_body(c, Body{p.x, p.y, p.z, p.heading, vx, vz, vg::vehicle_size(p.kind)});
      if (closing > 0) struck(state, i, EventKind::traffic, k, closing);
    }
    for (const vg::Parked& p : plan.parked) {
      const double closing =
          meet_body(c, Body{p.x, p.y, p.z, p.heading, 0, 0, vg::vehicle_size(p.kind)});
      if (closing > 0) struck(state, i, EventKind::traffic, -1 - p.id, closing);
    }
  }
}

}  // namespace fall_line::sim

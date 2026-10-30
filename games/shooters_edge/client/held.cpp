// The weapon in the player's hand, frame by frame (held.hpp).
//
// ge:summary The weapon in hand from the listen server: which one, a shot, the look, the stride
// ge:keywords held-weapon step_held attack_finished
// ge:skill shooter render
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/held.hpp"

#include <cmath>

#include "brush/map.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kWalking = 0.5;  // m/s along the ground the stride's clock runs above

}  // namespace

view::HandFrame step_held(const Listen& l, const ViewState& v, HeldState& s,
                          const view::ViewSettings& settings,
                          std::span<const ge::viewmodel::HeldModel> models, double t, double dt) {
  const PlayerState p = player_state(l);
  view::HandInput in;
  in.weapon = static_cast<view::Weapon>(status_of(l, t).weapon);
  const float attack = field_named(l, p.entity, "attack_finished");
  const double now = l.server ? l.server->sv.time : 0;
  in.fired = !p.dead && attack > s.attack_finished && static_cast<double>(attack) > now + 1e-3;
  s.attack_finished = attack;
  if (in.fired) ++s.shots;
  in.place.look_yaw = v.pose.yaw;
  in.place.look_pitch = v.pose.pitch;
  const double vx = p.velocity.x, vy = p.velocity.y;
  in.place.ground_speed = std::sqrt(vx * vx + vy * vy) * ge::brush::kUnitM;
  in.place.grounded = p.on_ground;
  if (p.on_ground && in.place.ground_speed > kWalking) s.stride_s += dt;
  in.place.stride_s = s.stride_s;
  return view::step_hand(s.hand, in, settings, models, v.pose, dt);
}

}  // namespace shooters_edge::client

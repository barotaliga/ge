// The weapon in the player's hand, frame by frame (held.hpp).
//
// ge:summary The weapon in hand from the listen server: which one, a shot, the look, the stride
// ge:keywords held-weapon step_held attack_finished
// ge:skill shooter render
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/held.hpp"

#include <cmath>

#include "brush/map.hpp"
#include "core/output.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kWalking = 0.5;  // m/s along the ground the stride's clock runs above

}  // namespace

bool load_held_set(const GameAdapter& game, const SearchPath& files,
                   std::span<const std::string> dirs, std::vector<ge::viewmodel::HeldModel>& out) {
  namespace out_ = ge::core::output;
  std::string why;
  out.clear();
  if (!files.folder.empty()) {
    if (game.held_weapons(game, files, out, why)) {
      out_::status("shooters_edge: the held weapons are the game's own, from its files");
      return true;
    }
    out_::status("shooters_edge: no weapon is drawn: " +
                 (why.empty() ? std::string("the game's files hold no held weapons") : why) +
                 "; next: add the game's own packs to " + files.folder +
                 " (a classic game draws only its own weapons)");
    return false;
  }
  for (const std::string& dir : dirs)
    if (view::load_weapon_models(dir, out, &why)) return false;
  out_::status("shooters_edge: no weapon is drawn: " + why +
               "; next: pass -models <the tree's games/shooters_edge/content/models/view>");
  return false;
}

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

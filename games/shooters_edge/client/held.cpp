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
#include "shooters_edge/client/timedemo.hpp"
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
  in.dead = p.dead;  // the weapon sinks out of view (view/hand.hpp)
  const float attack = field_named(l, p.entity, "attack_finished");
  const double now = l.server ? l.server->sv.time : 0;
  in.fired = !p.dead && attack > s.attack_finished && static_cast<double>(attack) > now + 1e-3;
  s.attack_finished = attack;
  if (in.fired) {
    ++s.shots;
    // The mod's own weapon frame as the shot starts it: which of the axe's swings it chose.
    in.fire_frame = static_cast<int>(field_named(l, p.entity, "weaponframe"));
  }
  in.place.look_yaw = v.pose.yaw;
  in.place.look_pitch = v.pose.pitch;
  const double vx = p.velocity.x, vy = p.velocity.y;
  in.place.ground_speed = std::sqrt(vx * vx + vy * vy) * ge::brush::kUnitM;
  in.place.grounded = p.on_ground;
  if (p.on_ground && in.place.ground_speed > kWalking) s.stride_s += dt;
  in.place.stride_s = s.stride_s;
  return view::step_hand(s.hand, in, settings, models, v.pose, dt);
}

view::HandFrame step_demo_held(const DemoState& s, HeldState& h, const view::ViewSettings& settings,
                               std::span<const ge::viewmodel::HeldModel> models,
                               const ge::render::FrameView& frame, double t, double dt) {
  ge::camera::Pose pose;
  pose.eye = {frame.eye[0], frame.eye[1], frame.eye[2]};
  pose.yaw = frame.heading;
  pose.pitch = frame.pitch;
  pose.fov_y = frame.fov_y;
  pose.near_m = frame.near_m;
  view::HandInput in;
  in.weapon = static_cast<view::Weapon>(demo_status(s, t).weapon);
  in.place.look_yaw = pose.yaw;
  in.place.look_pitch = pose.pitch;
  in.place.grounded = true;
  // Dead, the recorder's weapon sinks out of view (the recording clears the weapon at once: the
  // one shown goes on being drawn as it sinks).
  in.dead = s.stats[kStatHealth] <= 0;
  view::HandFrame f = view::step_hand(h.hand, in, settings, models, pose, dt);
  // A classic set's frame as the server sent it: its fire animations played as recorded.
  const auto w = static_cast<size_t>(f.weapon);
  if (h.hand.at_eye && w < models.size()) {
    const auto slot = static_cast<size_t>(s.player);
    int wf = s.format == DemoFormat::single             ? s.weapon_frame
             : s.player >= 0 && slot < s.players.size() ? s.players[slot].weapon_frame
                                                        : 0;
    if (wf < 0 || static_cast<size_t>(wf) >= models[w].frames.size()) wf = 0;
    if (h.recorded_weapon != static_cast<int>(w)) h.recorded = FrameTrack{};
    h.recorded_weapon = static_cast<int>(w);
    int from = -1;
    float g = 1;
    track_frame(h.recorded, wf, t, from, g);
    f.frame_b = wf;
    f.frame_a = settings.lerp && from >= 0 ? from : wf;
    f.frame_t = settings.lerp && from >= 0 ? g : 0;
  }
  f.drawn = f.drawn && !s.intermission && (s.stats[kStatWeapon] != 0 || (in.dead && h.shown));
  h.shown = f.drawn;
  return f;
}

}  // namespace shooters_edge::client

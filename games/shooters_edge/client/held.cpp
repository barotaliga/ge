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
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kWalking = 0.5;  // m/s along the ground the stride's clock runs above

// The frame of a held model of `count` frames the sequel's hand on `frame` of weapon `w` shows.
// The sequel's own model has every frame its hand runs through. Another game's (classic remix's
// `sv_wear`) has a rest frame and its firing frames after it: the hand's firing frames are spread
// over those, and every other frame (raising, idling, lowering) is the rest.
int sequel_frame(int w, int frame, int count) {
  const server::SqWeaponRow& row = server::kSqWeapons[size_t(w)];
  if (count > row.last_deactivate) return frame >= 0 && frame < count ? frame : 0;
  const int firing = row.last_fire - row.last_activate;
  if (count < 2 || firing <= 0 || frame <= row.last_activate || frame > row.last_fire) return 0;
  return 1 + (frame - row.last_activate - 1) * (count - 1) / firing;
}

// The sequel's hand: the weapon its rules hold and the frame its hand is on (activate, fire, idle,
// deactivate, all the model's own), drawn from the last over the time its frames last; the hand's
// place (look, stride, sinking at death) as any game's; a shot (its kick, light and fire) each
// time it steps onto a frame that fires.
view::HandFrame step_sequel_held(const server::SequelServer& sq, const PlayerState& p,
                                 const ViewState& v, HeldState& s,
                                 const view::ViewSettings& settings,
                                 std::span<const ge::viewmodel::HeldModel> models, double t,
                                 double dt) {
  const server::SqHand& hand = sq.players[0].hand;
  view::HandInput in;
  const int w = hand.weapon;
  in.weapon = static_cast<view::Weapon>(w);
  in.sequel = true;
  in.dead = p.dead;
  // A shot each time the hand steps onto a frame that fires (the held-fire guns loop in their
  // range, a shot a frame).
  if (w >= 0 && w < server::kSqWeaponCount) {
    const server::SqWeaponRow& row = server::kSqWeapons[static_cast<size_t>(w)];
    in.fired = !p.dead && hand.state == server::SqHandState::firing &&
               hand.frame >= row.fire_from && hand.frame <= row.fire_to &&
               (hand.frame != s.sequel_frame || w != s.recorded_weapon);
  }
  s.sequel_frame = hand.frame;
  if (in.fired) ++s.shots;
  in.place.look_yaw = v.pose.yaw;
  in.place.look_pitch = v.pose.pitch;
  const double vx = p.velocity.x, vy = p.velocity.y;
  in.place.ground_speed = std::sqrt(vx * vx + vy * vy) * ge::brush::kUnitM;
  in.place.grounded = p.on_ground;
  if (p.on_ground && in.place.ground_speed > kWalking) s.stride_s += dt;
  in.place.stride_s = s.stride_s;
  view::HandFrame f = view::step_hand(s.hand, in, settings, models, v.pose, dt);
  const auto held = static_cast<size_t>(f.weapon);
  if (held < models.size() && static_cast<int>(held) == w) {
    int wf = sequel_frame(w, hand.frame, static_cast<int>(models[held].frames.size()));
    if (s.recorded_weapon != w) s.recorded = FrameTrack{};
    s.recorded_weapon = w;
    int from = -1;
    float g = 1;
    track_frame(s.recorded, wf, t, from, g);
    f.frame_b = wf;
    f.frame_a = settings.lerp && from >= 0 ? from : wf;
    f.frame_t = settings.lerp && from >= 0 ? g : 0;
  }
  return f;
}

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
  // The sequel's rules keep the hand themselves: its weapon and the frame it is on.
  if (const server::SequelServer* sq = l.rules ? server::sequel_of(*l.rules) : nullptr)
    return step_sequel_held(*sq, p, v, s, settings, models, t, dt);
  const auto game = static_cast<view::Weapon>(status_of(l, t).weapon);
  in.weapon = s.show >= 0 ? static_cast<view::Weapon>(s.show) : game;
  in.dead = p.dead;  // the weapon sinks out of view (view/hand.hpp)
  const float attack = field_named(l, p.entity, "attack_finished");
  const double now = l.server ? l.server->sv.time : 0;
  in.fired = !p.dead && in.weapon == game && attack > s.attack_finished &&
             static_cast<double>(attack) > now + 1e-3;
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
  const auto game = static_cast<view::Weapon>(demo_status(s, t).weapon);
  in.weapon = h.show >= 0 ? static_cast<view::Weapon>(h.show) : game;
  in.place.look_yaw = pose.yaw;
  in.place.look_pitch = pose.pitch;
  in.place.grounded = true;
  // Dead, the recorder's weapon sinks out of view (the recording clears the weapon at once: the
  // one shown goes on being drawn as it sinks).
  in.dead = s.stats[kStatHealth] <= 0;
  view::HandFrame f = view::step_hand(h.hand, in, settings, models, pose, dt);
  // A classic set's frame as the server sent it: its fire animations played as recorded.
  const auto w = static_cast<size_t>(f.weapon);
  if (h.hand.at_eye && w < models.size() && f.weapon == game) {
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

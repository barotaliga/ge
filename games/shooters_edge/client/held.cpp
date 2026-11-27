// The weapon in the player's hand, frame by frame (held.hpp).
//
// ge:summary The weapon in hand from the listen server: which one, a shot, the look, the stride
// ge:keywords held-weapon step_held attack_finished
// ge:skill shooter render
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/held.hpp"

#include <cmath>

#include "brush/map.hpp"
#include "classic/weapon_predict.hpp"
#include "core/output.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kWalking = 0.5;  // m/s along the ground the stride's clock runs above
constexpr double kDegRad = 0.017453292519943295;

// The frame to draw of a whole view model (one resting past frame 0, its raise first) sent
// `frame`: its own frame from Quake II's hand, or Quake's frames as its rest and fire run
// (q2_arms_kick.hpp's `WholeFrames`). Any other model's frame as sent.
int whole_drawn(const ge::viewmodel::HeldModel& m, WholeFrames& w, int frame, double t) {
  if (m.rest <= 0) return frame;
  read_whole_frames(w, frame, t);
  const int clip = ge::viewmodel::find_held_clip(m, "fire");
  if (clip < 0) return w.quake ? m.rest : frame;
  const ge::viewmodel::HeldClip& c = m.clips[static_cast<size_t>(clip)];
  return whole_frame(w, m.rest, c.first, c.first + c.count - 1, frame);
}

}  // namespace

void q2_gun_sway(ge::shooter::Vec3f gun_angles, ge::shooter::Vec3f gun_offset,
                 ge::shooter::Vec3f view_angles, view::HandInput& in) {
  in.game_sway = true;
  in.place.game_turn[0] = -gun_angles.y * kDegRad;  // its yaw is to the left, ours to the right
  in.place.game_turn[1] = -gun_angles.x * kDegRad;  // its pitch is down, ours up
  in.place.game_turn[2] = gun_angles.z * kDegRad;
  const ge::shooter::ViewAxes ax = ge::shooter::angle_vectors(view_angles);
  in.place.game_shift_m[0] = ge::shooter::dot(gun_offset, ax.right) * ge::brush::kUnitM;
  in.place.game_shift_m[1] = ge::shooter::dot(gun_offset, ax.up) * ge::brush::kUnitM;
  in.place.game_shift_m[2] = ge::shooter::dot(gun_offset, ax.forward) * ge::brush::kUnitM;
}

namespace {

// The frame of a held model of `count` frames Quake II's hand on `frame` of weapon `w` shows.
// Quake II's own model has every frame its hand runs through. Another game's (classic remix's
// `sv_wear`) has a rest frame and its firing frames after it: the hand's firing frames are spread
// over those, and every other frame (raising, idling, lowering) is the rest.
int q2_frame(int w, int frame, int count) {
  const server::SqWeaponRow& row = server::kSqWeapons[size_t(w)];
  if (count > row.last_deactivate) return frame >= 0 && frame < count ? frame : 0;
  const int firing = row.last_fire - row.last_activate;
  if (count < 2 || firing <= 0 || frame <= row.last_activate || frame > row.last_fire) return 0;
  return 1 + (frame - row.last_activate - 1) * (count - 1) / firing;
}

// Quake II's hand: the weapon its rules hold and the frame its hand is on (activate, fire, idle,
// deactivate, all the model's own), drawn from the last over the time its frames last; the hand's
// place (look, stride, sinking at death) as any game's; a shot (its kick, light and fire) each
// time it steps onto a frame that fires.
view::HandFrame step_q2_held(const server::Q2Server& sq, const PlayerState& p, const ViewState& v,
                             HeldState& s, const view::ViewSettings& settings,
                             std::span<const ge::viewmodel::HeldModel> models, double t,
                             double dt) {
  const server::SqHand& hand = sq.players[0].hand;
  view::HandInput in;
  const int w = hand.weapon;
  in.weapon = static_cast<view::Weapon>(w);
  in.q2 = true;
  in.dead = p.dead;
  // A shot each time the hand passes a frame that fires since the last step. The server fires a
  // frame and steps past it at once, so a gun that fires on its first frame (the shotguns, the
  // blaster) is never seen on it: the step that starts firing counts from the first fire frame,
  // and a held-fire gun's loop back to the start of its range is a shot too.
  const bool firing = hand.state == server::SqHandState::firing;
  if (w >= 0 && w < server::kSqWeaponCount && firing && !p.dead) {
    const server::SqWeaponRow& row = server::kSqWeapons[static_cast<size_t>(w)];
    const bool began = !s.q2_firing || w != s.q2_weapon;
    int from = hand.frame;  // the frames passed since the last step: from..hand.frame
    if (began)
      from = row.last_activate + 1;
    else if (hand.frame > s.q2_frame)
      from = s.q2_frame + 1;
    else if (hand.frame == s.q2_frame)
      from = hand.frame + 1;  // none
    in.fired = std::max(from, row.fire_from) <= std::min(hand.frame, row.fire_to);
  }
  s.q2_frame = hand.frame;
  s.q2_firing = firing;
  s.q2_weapon = w;
  if (in.fired) ++s.shots;
  q2_gun_sway(sq.players[0].seen.gun_angles, sq.players[0].seen.gun_offset, p.v_angle, in);
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
    int wf = q2_frame(w, hand.frame, static_cast<int>(models[held].frames.size()));
    if (s.recorded_weapon != w) s.recorded = FrameTrack{};
    s.recorded_weapon = w;
    int from = -1;
    float g = 1;
    track_frame(s.recorded, wf, t, from, g, kFrameSpan);
    f.frame_b = wf;
    f.frame_a = settings.lerp && from >= 0 ? from : wf;
    f.frame_t = settings.lerp && from >= 0 ? g : 0;
  }
  return f;
}

// The developer's look (`gun_next`, `gun_prev`): the frame asked shown still, held to the model's.
void gun_look(const view::ViewSettings& s, std::span<const ge::viewmodel::HeldModel> models,
              view::HandFrame& f) {
  const auto w = static_cast<size_t>(f.weapon);
  if (s.gun_frame <= 0 || w >= models.size() || models[w].frames.empty()) return;
  f.frame_a = f.frame_b = std::min(s.gun_frame, static_cast<int>(models[w].frames.size()) - 1);
  f.frame_t = 0;
}

// A hosted game library's hand: the weapon whose view model it names (Quake II's set, by the
// Quake II's names, read from the mod's folder first) on the frame it names. The library's frames
// carry its own firing; the hand adds no kick or flash of its own.
view::HandFrame step_native_held(const server::NativeQ2& ns, const PlayerState& p,
                                 const ViewState& v, HeldState& s,
                                 const view::ViewSettings& settings,
                                 std::span<const ge::viewmodel::HeldModel> models, double t,
                                 double dt) {
  const auto& nb = ns.world.clients[0].numbers;
  const auto row = static_cast<size_t>(nb.weapon_model);
  const std::string_view name =
      row < ns.world.models.size() ? std::string_view(ns.world.models[row]) : std::string_view();
  int w = -1;
  for (int k = 0; k < server::kSqWeaponCount; k++)
    if (name == server::kSqWeapons[static_cast<size_t>(k)].view_model) w = k;
  const int frame = static_cast<int>(nb.weapon_frame);
  view::HandInput in;
  in.weapon = static_cast<view::Weapon>(settings.gun_model >= 0 ? settings.gun_model
                                        : w < 0                 ? 0
                                                                : w);
  in.q2 = true;
  in.dead = p.dead || w < 0;
  in.place.look_yaw = v.pose.yaw;
  in.place.look_pitch = v.pose.pitch;
  const double vx = p.velocity.x, vy = p.velocity.y;
  in.place.ground_speed = std::sqrt(vx * vx + vy * vy) * ge::brush::kUnitM;
  in.place.grounded = p.on_ground;
  if (p.on_ground && in.place.ground_speed > kWalking) s.stride_s += dt;
  in.place.stride_s = s.stride_s;
  view::HandFrame f = view::step_hand(s.hand, in, settings, models, v.pose, dt);
  const auto held = static_cast<size_t>(f.weapon);
  if (w >= 0 && held < models.size() && static_cast<int>(held) == w) {
    const int count = static_cast<int>(models[held].frames.size());
    const int drawn = whole_drawn(models[held], s.whole, frame, t);
    const int wf = drawn >= 0 && drawn < count ? drawn : 0;
    if (s.recorded_weapon != w) s.recorded = FrameTrack{};
    int from = -1;
    float g = 1;
    track_frame(s.recorded, wf, t, from, g, kFrameSpan);
    f.frame_b = wf;
    f.frame_a = settings.lerp && from >= 0 ? from : wf;
    f.frame_t = settings.lerp && from >= 0 ? g : 0;
  }
  s.recorded_weapon = w;
  gun_look(settings, models, f);
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

namespace {

// Quake II's weapons fired whole by our rules (classic remix's `cr_items q2`): the rules step
// Quake II's hand and keep its frame in `weaponframe`, so the view model (every frame kept,
// qw_adapter.cpp) is drawn on that frame; the frames carry the firing; the view's kick is
// Quake II's, read off the same frames (q2_arms_kick.hpp, client/present.cpp).
view::HandFrame step_q2_arms_held(const Listen& l, const PlayerState& p, const ViewState& v,
                                  HeldState& s, const view::ViewSettings& settings,
                                  std::span<const ge::viewmodel::HeldModel> models, double t,
                                  double dt) {
  const int w = static_cast<int>(status_of(l, t).weapon);
  const int frame = static_cast<int>(field_named(l, p.entity, "weaponframe"));
  view::HandInput in;
  in.weapon = static_cast<view::Weapon>(settings.gun_model >= 0 ? settings.gun_model : w);
  in.q2 = true;
  in.dead = p.dead;
  in.place.look_yaw = v.pose.yaw;
  in.place.look_pitch = v.pose.pitch;
  const double vx = p.velocity.x, vy = p.velocity.y;
  in.place.ground_speed = std::sqrt(vx * vx + vy * vy) * ge::brush::kUnitM;
  in.place.grounded = p.on_ground;
  if (p.on_ground && in.place.ground_speed > kWalking) s.stride_s += dt;
  in.place.stride_s = s.stride_s;
  in.place.bob_m = v.eyes.bob;
  view::HandFrame f = view::step_hand(s.hand, in, settings, models, v.pose, dt);
  const auto held = static_cast<size_t>(f.weapon);
  if (held < models.size() && static_cast<int>(held) == w) {
    const int count = static_cast<int>(models[held].frames.size());
    const int drawn = whole_drawn(models[held], s.whole, frame, t);
    const int wf = drawn >= 0 && drawn < count ? drawn : 0;
    if (s.recorded_weapon != w) s.recorded = FrameTrack{};
    int from = -1;
    float g = 1;
    track_frame(s.recorded, wf, t, from, g, kFrameSpan);
    f.frame_b = wf;
    f.frame_a = settings.lerp && from >= 0 ? from : wf;
    f.frame_t = settings.lerp && from >= 0 ? g : 0;
  }
  s.recorded_weapon = w;
  gun_look(settings, models, f);
  return f;
}

}  // namespace

view::HandFrame step_held(const Listen& l, const ViewState& v, HeldState& s,
                          const view::ViewSettings& settings,
                          std::span<const ge::viewmodel::HeldModel> models, double t, double dt) {
  const PlayerState p = player_state(l);
  view::HandInput in;
  // Quake II's rules keep the hand themselves: its weapon and the frame it is on.
  if (const server::Q2Server* sq = l.rules ? server::q2_of(*l.rules) : nullptr)
    return step_q2_held(*sq, p, v, s, settings, models, t, dt);
  if (const server::NativeQ2* ns = l.rules ? server::native_q2_of(*l.rules) : nullptr)
    return step_native_held(*ns, p, v, s, settings, models, t, dt);
  // A single-player program's player: the weapon is gone at once when he dies, and is not drawn
  // over the intermission's camera, whichever game's weapon he holds.
  const bool hidden = l.campaign_host != nullptr && (p.dead || l.campaign.end != LevelEnd::none);
  if (listen_q2_arms(l)) {
    view::HandFrame f = step_q2_arms_held(l, p, v, s, settings, models, t, dt);
    if (hidden) f.drawn = false;
    return f;
  }
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
  in.place.bob_m = v.eyes.bob;  // the stride's bob, the weapon pushed forward by its share
  view::HandFrame f = view::step_hand(s.hand, in, settings, models, v.pose, dt);
  if (hidden) f.drawn = false;
  return f;
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
  const auto slot = static_cast<size_t>(s.player);
  const int now_frame = s.format == DemoFormat::single             ? s.weapon_frame
                        : s.player >= 0 && slot < s.players.size() ? s.players[slot].weapon_frame
                                                                   : 0;
  auto game = static_cast<view::Weapon>(demo_status(s, t).weapon);
  // r_viewmodellastfired, played back: the weapon that last fired stays drawn while the player
  // flicks through others, until another one fires (or the watched player changes).
  if (!settings.last_fired || !h.recording || s.stats[kStatWeapon] == 0 ||
      h.last_fired_player != s.player)
    h.last_fired = -1;
  if (settings.last_fired && h.recording && s.stats[kStatWeapon] != 0) {
    if (now_frame != 0) {
      h.last_fired = static_cast<int>(game);
      h.last_fired_player = s.player;
    } else if (h.last_fired >= 0) {
      game = static_cast<view::Weapon>(h.last_fired);
    }
  }
  in.weapon = settings.gun_model >= 0 ? static_cast<view::Weapon>(settings.gun_model)
              : h.show >= 0           ? static_cast<view::Weapon>(h.show)
                                      : game;
  in.place.look_yaw = pose.yaw;
  in.place.look_pitch = pose.pitch;
  in.place.grounded = true;
  // Dead, the recorder's weapon sinks out of view (the recording clears the weapon at once: the
  // one shown goes on being drawn as it sinks).
  in.dead = s.stats[kStatHealth] <= 0;
  // A joined server's weapon predicted: each shot it fired fires the hand (net/qw_weapon.hpp).
  if (h.own_shots >= 0 && s.own_shots > h.own_shots && !in.dead && in.weapon == game) {
    in.fired = true;
    in.fire_frame = s.own_shot_frame;
  }
  h.own_shots = s.own_shots;
  // A whole view model (Quake II's: its frame 0 the bottom of its raise, out of sight) is drawn
  // from the frames sent from the start.
  const auto shown = static_cast<size_t>(game);
  const bool whole_view = shown < models.size() && models[shown].rest > 0;
  if (now_frame != 0 || whole_view) h.frames_live = true;
  // Rules that never animate the weapon frame (our arena's), unpredicted: the shot the server's
  // weapon sound on the player says, once per frame read (any game, a demo or a joined server).
  if (!in.fired && !h.frames_live && s.frames != h.frames_read && !in.dead && in.weapon == game) {
    for (const DemoSound& snd : s.started) {
      if (snd.entity != s.player + 1 || snd.channel != 1 || snd.name != nullptr) continue;
      if (snd.sound <= 0 || static_cast<size_t>(snd.sound) >= s.sounds.size()) continue;
      if (ge::classic::weapon_sound_bit_of_sample(s.sounds[static_cast<size_t>(snd.sound)]) != 0 &&
          s.sounds[static_cast<size_t>(snd.sound)] != "weapons/lhit.wav")
        in.fired = true;
    }
  }
  h.frames_read = s.frames;
  if (s.format == DemoFormat::q2) q2_gun_sway(s.gun_angles, s.gun_offset, s.angles, in);
  view::HandFrame f = view::step_hand(h.hand, in, settings, models, pose, dt);
  // A classic set's frame as the server sent it: its fire animations played as recorded.
  const auto w = static_cast<size_t>(f.weapon);
  if (h.hand.at_eye && w < models.size() && f.weapon == game && h.frames_live) {
    int wf = whole_drawn(models[w], h.whole, now_frame, t);
    if (wf < 0 || static_cast<size_t>(wf) >= models[w].frames.size()) wf = 0;
    if (h.recorded_weapon != static_cast<int>(w)) h.recorded = FrameTrack{};
    h.recorded_weapon = static_cast<int>(w);
    int from = -1;
    float g = 1;
    track_frame(h.recorded, wf, t, from, g, kFrameSpan);
    f.frame_b = wf;
    f.frame_a = settings.lerp && from >= 0 ? from : wf;
    f.frame_t = settings.lerp && from >= 0 ? g : 0;
  }
  f.drawn = f.drawn && !s.intermission && (s.stats[kStatWeapon] != 0 || (in.dead && h.shown));
  h.shown = f.drawn;
  gun_look(settings, models, f);
  return f;
}

}  // namespace shooters_edge::client

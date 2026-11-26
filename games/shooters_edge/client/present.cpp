// The player's view frame by frame (present.hpp).
//
// ge:summary The player's view frame by frame: the first-person eyes, the missiles and blasts
// ge:keywords present frame_view missiles blasts
// ge:skill camera ui shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/present.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"

#include "brush/map.hpp"
#include "camera/to_view.hpp"
#include "shooter/brush_hull.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace {
constexpr double kDeg = 3.14159265358979323846 / 180;
constexpr double kTau = 2 * 3.14159265358979323846;
}  // namespace

void add_view_camera_cvars(ge::console::Cvars& c, ge::camera::CameraCvars& out) {
  ge::camera::CameraDefaults d;  // the classic look's numbers, in map units
  d.unit_m = ge::brush::kUnitM;
  d.bob = 0.02;
  d.bobcycle = 0.6;
  d.bobup = 0.5;
  d.rollangle = 2;
  d.rollspeed = 200;
  d.kicktime = 0.5;
  d.kickpitch = d.kickroll = 0.6;
  d.idlescale = 0;
  d.landdip = 0;
  ge::camera::add_camera_cvars(c, out, d);
  c.bind({.name = "v_viewheight",
          .fallback = "0",
          .flags = ge::console::kArchive,
          .help = "Raises or lowers your eye by a few units; any value but 0 also stops the bob.",
          .min = -7,
          .max = 4,
          .values = "0: the game's own eye and bob (the default); -7..4: units up or down"},
         &out.viewlift);
  const ge::console::CvarLayer quake(c, "classic");
  // The idle sway's shape (both Quakes): v_idlescale times the level, in degrees, at the cycle's
  // rate in radians a second; the intermission sways by it at a scale of 1. Not archived.
  struct Axis {
    const char* cycle;
    const char* level;
    const char* cycle_fallback;
    const char* level_fallback;
    const char* what;
  };
  static constexpr Axis kAxes[3] = {
      {"v_ipitch_cycle", "v_ipitch_level", "1", "0.3", "pitch"},
      {"v_iyaw_cycle", "v_iyaw_level", "2", "0.3", "yaw"},
      {"v_iroll_cycle", "v_iroll_level", "0.5", "0.1", "roll"},
  };
  for (int k = 0; k < 3; k++) {
    const Axis& a = kAxes[k];
    c.bind({.name = a.cycle,
            .fallback = a.cycle_fallback,
            .help = std::string("How fast the idle sway turns the view's ") + a.what +
                    ", in radians a second.",
            .min = 0,
            .max = 100,
            .values = "0: that way still; 0..100: that many radians a second"},
           &out.idle_cycle[k]);
    c.bind({.name = a.level,
            .fallback = a.level_fallback,
            .help = std::string("How far the idle sway turns the view's ") + a.what +
                    ", in degrees at a v_idlescale of 1.",
            .min = 0,
            .max = 30,
            .values = "0: that way still; 0..30: that many degrees"},
           &out.idle_level[k]);
  }
  c.bind({.name = "cl_bobhead",
          .fallback = "0",
          .flags = ge::console::kArchive,
          .help = "Whether your head carries the weapon's bob as well as its own.",
          .kind = ge::console::CvarKind::boolean},
         &out.bobhead);
}

void apply_view_cvars(const ge::camera::CameraCvars& c, ViewState& v) {
  static const ge::camera::FirstPersonRow kClassic =
      ge::camera::classic_first_person(ge::brush::kUnitM);
  ge::camera::apply_first_person_cvars(c, kClassic, v.row);
  // The sway's shape: the map's pitch and yaw turn the other way from the rig's.
  for (int k = 0; k < 3; k++) {
    v.idle_cycle[k] = c.idle_cycle[k];
    v.idle_level[k] = c.idle_level[k];
  }
  auto period = [](double cycle) { return cycle > 0 ? kTau / cycle : 0.0; };
  v.row.idle_pitch_share = -c.idle_level[0];
  v.row.idle_yaw_share = -c.idle_level[1];
  v.row.idle_roll_share = c.idle_level[2];
  v.row.idle_pitch_period_s = period(c.idle_cycle[0]);
  v.row.idle_yaw_period_s = period(c.idle_cycle[1]);
  v.row.idle_roll_period_s = period(c.idle_cycle[2]);
  v.viewheight_m = c.viewheight * c.unit_m;
  // v_viewheight: the eye raised or dropped by a few units, its own bob given up for it (the
  // weapon keeps its bob); cl_bobhead: the weapon's bob carried by the eye as well.
  v.row.eye_lift_m = c.viewlift * c.unit_m;
  v.row.bob_eye = (c.viewlift != 0 ? 0 : 1) + (c.bobhead ? 1 : 0);
  if (c.fov > 0) v.row.fov_y = v.row.zoom_fov_y = ge::camera::fov_y_of(c.fov * kDeg, 4.0 / 3.0);
}

ge::render::FrameView frame_view(const Listen& l, ViewState& v, int frames, double t, double dt) {
  // The mod's explosions since the last look (a rocket at point blank goes off in the frame that
  // fired it, so it is never seen in flight).
  if (frames > 0) missiles_of(l, v.missiles);
  if (l.explosions.size() < v.explosions_seen) v.explosions_seen = 0;  // a new map
  for (; v.explosions_seen < l.explosions.size(); v.explosions_seen++)
    v.blasts.push_back(
        {engine_point(l.explosions[v.explosions_seen]), t, l.explosions[v.explosions_seen]});
  std::erase_if(v.blasts, [&](const Blast& b) { return t - b.t > kBlastS; });

  const PlayerState me = player_state(l);
  // The rules' own view (Quake II's): the bob, the tilts and the kicks are theirs, so the rig
  // adds none; it eases a step up as Quake II's client does, 20 units at most over a tenth.
  ge::camera::FirstPersonRow row = v.row;
  if (me.rules_view) {
    row.bob = 0;
    row.roll_angle = row.kick_pitch = row.kick_roll = 0;
    row.step_rise_mps = kRulesStepRate * ge::brush::kUnitM;
    row.step_max_m = kRulesStepMost * ge::brush::kUnitM;
    // Carried by a lift: its rise is eased below, not taken for a step to climb.
    if (me.pushed.z != 0) row.step_max_m = 0;
  }
  // A single-player program's intermission: the view stands where the program put the player,
  // with no bob or roll, swaying idly at a scale of 1 by the v_i* settings (pitch and yaw 0.3
  // degrees, roll 0.1, at 1, 2 and 0.5 radians a second of the game's clock by default).
  const bool intermission = l.campaign_host != nullptr && l.campaign.end != LevelEnd::none;
  if (intermission) row.bob = row.roll_angle = row.kick_pitch = row.kick_roll = 0;
  ge::camera::Subject s = subject_of(l, row);
  if (v.viewheight_m > 0 && !me.rules_view) row.eye_stand_m = v.viewheight_m;
  // The eye's place between the last two states, one server frame behind.
  if (l.server != nullptr) {
    const double now = l.server->sv.time;
    // Quake II's rules' own view changes once a frame of theirs (a tenth): eased across it.
    const bool by_rules_frame = me.rules_frame >= 0;
    if (by_rules_frame && me.rules_frame != v.rules_frame) {
      const bool first = v.rules_frame < 0 || me.rules_frame < v.rules_frame;
      v.offset_prev = first ? me.view_ofs : v.offset_now;
      v.kick_prev = first ? me.kick : v.kick_now;
      v.fov_prev = first ? me.fov : v.fov_now;
      v.offset_now = me.view_ofs;
      v.kick_now = me.kick;
      v.fov_now = me.fov;
      v.rules_frame = me.rules_frame;
      v.rules_frame_s = now;
    }
    if (now != v.eye_now_s) {
      const float dx = me.origin.x - v.eye_now.x, dy = me.origin.y - v.eye_now.y,
                  dz = me.origin.z - v.eye_now.z;
      const bool cut = v.eye_now_s < 0 || now < v.eye_now_s || l.forced ||
                       dx * dx + dy * dy + dz * dz > kEyeSnapUnits * kEyeSnapUnits;
      v.eye_prev = cut ? me.origin : v.eye_now;
      v.eye_prev_s = cut ? now : v.eye_now_s;
      v.eye_now = me.origin;
      v.eye_now_s = now;
      if (!by_rules_frame) {
        v.offset_prev = cut ? me.view_ofs : v.offset_now;
        v.kick_prev = cut ? me.kick : v.kick_now;
        v.fov_prev = cut ? me.fov : v.fov_now;
        v.offset_now = me.view_ofs;
        v.kick_now = me.kick;
        v.fov_now = me.fov;
      }
    }
    const double gap = v.eye_now_s - v.eye_prev_s;
    const double tic = listen_tic(l);
    const double drawn = now + std::min(l.owed, tic) - tic;
    const auto f = gap > 0 && gap <= 0.25
                       ? static_cast<float>(std::clamp((drawn - v.eye_prev_s) / gap, 0.0, 1.0))
                       : 1.0f;
    // The share of Quake II's rules frame gone by: its view and its movers' carry eased over it.
    const auto fr = by_rules_frame
                        ? static_cast<float>(std::clamp(
                              (me.rules_age + std::min(l.owed, tic)) / kRulesFrameS, 0.0, 1.0))
                        : f;
    // The rules' view between their last two frames, always (their 10 a second would judder).
    if (me.rules_view) {
      const auto mix = [fr](sh::Vec3f a, sh::Vec3f b) { return a + (b - a) * fr; };
      v.offset = mix(v.offset_prev, v.offset_now);
      v.kick = mix(v.kick_prev, v.kick_now);
      if (v.q2 != nullptr && !v.q2->kick_angles) v.kick = {};  // cl_kickangles 0
      // The rules' field of view (a zoom eased over their frame), across at 4:3 (cl_adjustfov).
      if (v.fov_prev > 0 && v.fov_now > 0)
        row.fov_y = row.zoom_fov_y = q2_fov_y(v.fov_prev + (v.fov_now - v.fov_prev) * fr,
                                              v.q2 == nullptr || v.q2->adjust_fov != 0, v.aspect);
      row.eye_stand_m = row.eye_crouch_m =
          (static_cast<double>(v.offset.z) - ge::shooter::kPlayerMins.z) * ge::brush::kUnitM;
    }
    if (v.lerp && gap > 0 && gap <= 0.25) {
      sh::Vec3f feet{v.eye_prev.x + (v.eye_now.x - v.eye_prev.x) * f,
                     v.eye_prev.y + (v.eye_now.y - v.eye_prev.y) * f,
                     v.eye_prev.z + (v.eye_now.z - v.eye_prev.z) * f};
      feet.z += ge::shooter::kPlayerMins.z;
      s.pos = engine_point(feet);
    }
    // A lift's rise in one rules frame shown spread over it: the eye still owes the share to come.
    if (by_rules_frame && (me.pushed.x != 0 || me.pushed.y != 0 || me.pushed.z != 0)) {
      // What the eye's own easing between server frames already holds back of it.
      const bool eased = v.lerp && gap > 0 && gap <= 0.25 && v.rules_frame_s == v.eye_now_s;
      const sh::Vec3f owed = me.pushed * std::max(0.0f, (1.0f - fr) - (eased ? 1.0f - f : 0.0f));
      const ge::camera::Vec3 o = engine_point({0, 0, 0}), d = engine_point(owed);
      s.pos = {s.pos.x - (d.x - o.x), s.pos.y - (d.y - o.y), s.pos.z - (d.z - o.z)};
    }
  }
  // The blows since the last look, each tipping the view toward its source by its count.
  if (l.blows.size() < v.blows_seen) v.blows_seen = 0;  // a new map
  for (; v.blows_seen < l.blows.size(); v.blows_seen++) {
    const Blow& b = l.blows[v.blows_seen];
    const ge::camera::Vec3 to = engine_point(me.origin), from = engine_point(b.from);
    ge::camera::Vec3 dir{to.x - from.x, to.y - from.y, to.z - from.z};
    const double len = ge::core::hypot3(dir.x, dir.y, dir.z);
    if (len > 0) dir = {dir.x / len, dir.y / len, dir.z / len};
    ge::camera::add_moment(s, ge::camera::Moment::hit,
                           std::max(kKickCountMin, 0.5 * b.armor + 0.5 * b.blood), dir);
  }
  if (l.forced) ge::camera::add_moment(s, ge::camera::Moment::teleport, 0);
  if (s.grounded && !v.was_grounded)
    ge::camera::add_moment(s, ge::camera::Moment::landing,
                           -static_cast<double>(v.fall_speed) * ge::brush::kUnitM);
  v.was_grounded = s.grounded;
  v.fall_speed = me.velocity.z;
  ge::render::FrameView view;
  view.time_s = t;
  v.pose = ge::camera::frame_first_person(row, s, v.eyes, dt, 1.0);
  if (me.rules_view) {  // the rules' kick angles, and the eye moved across by their offset
    const ge::camera::Vec3 o = engine_point({0, 0, 0}),
                           d = engine_point({v.offset.x, v.offset.y, 0});
    v.pose.eye = {v.pose.eye.x + d.x - o.x, v.pose.eye.y + d.y - o.y, v.pose.eye.z + d.z - o.z};
    v.pose.pitch -= v.kick.x * kDeg;
    v.pose.yaw -= v.kick.y * kDeg;
    v.pose.roll += v.kick.z * kDeg;
  }
  // Quake II's weapons on other rules: Quake II's kick, read off the hand's frames.
  if (listen_q2_arms(l)) {
    const sh::ViewAxes ax = sh::angle_vectors(me.v_angle);
    const float forward[3] = {ax.forward.x, ax.forward.y, ax.forward.z};
    const ge::classic::Q2Kick k = arms_kick_step(
        v.arms_kick, string_field_named(l, me.entity, "weaponmodel"),
        static_cast<int>(field_named(l, me.entity, "weaponframe")), t, forward,
        !wants_campaign(l.opt), static_cast<uint32_t>(me.entity));
    const ge::camera::Vec3 o = engine_point({0, 0, 0}),
                           d = engine_point({k.origin[0], k.origin[1], k.origin[2]});
    v.pose.eye = {v.pose.eye.x + d.x - o.x, v.pose.eye.y + d.y - o.y, v.pose.eye.z + d.z - o.z};
    if (v.q2 == nullptr || v.q2->kick_angles) {  // cl_kickangles 0: the angles left out
      v.pose.pitch -= k.angles[0] * kDeg;
      v.pose.yaw -= k.angles[1] * kDeg;
      v.pose.roll += k.angles[2] * kDeg;
    }
  }
  // The program's punch on the view (pitch down and yaw left positive in the map's angles).
  v.pose.pitch -= me.punch.x * kDeg;
  v.pose.yaw -= me.punch.y * kDeg;
  v.pose.roll += me.punch.z * kDeg;
  if (me.dead && me.rules_view) {  // the rules' dead view: on its side, looking up at the killer
    v.pose.pitch = engine_pitch(me.v_angle.x);
    v.pose.yaw = engine_heading(me.v_angle.y);
    v.pose.roll = me.v_angle.z * kDeg;
  } else if (me.dead) {
    v.pose.roll = kDeadRollDeg * kDeg;  // lying on the floor
  }
  if (intermission) {
    const double clock = l.server != nullptr ? l.server->sv.time : t;
    v.pose.pitch -= v.idle_level[0] * std::sin(clock * v.idle_cycle[0]) * kDeg;
    v.pose.yaw -= v.idle_level[1] * std::sin(clock * v.idle_cycle[1]) * kDeg;
    v.pose.roll += v.idle_level[2] * std::sin(clock * v.idle_cycle[2]) * kDeg;
  }
  ge::camera::to_frame_view(v.pose, view);
  return view;
}

}  // namespace shooters_edge::client

// ge:summary Shooter's Edge's first-person hand: the switch, the kick, the flash, the cvars
// ge:keywords shooters-edge viewmodel step_hand held_row add_view_cvars switch flash
// ge:keywords pick_take axe-swing takes breathe cl_gunidle
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#include "shooters_edge/view/hand.hpp"

#include <string_view>

#include "core/math.hpp"

namespace shooters_edge::view {

namespace con = ge::console;

// A frame stepped on by a shot is held this many of the clip's frame times waiting for the next
// shot before the weapon goes back to rest (the classic weapon falls back a frame after the last).
constexpr double kShotFrameHold = 1.5;

void add_view_cvars(con::Cvars& c, ViewSettings& out) {
  c.bind({.name = "r_drawviewmodel",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether the held weapon is drawn.",
          .kind = con::CvarKind::boolean,
          .values = "0: not drawn; 1: drawn (the default)"},
         &out.draw);
  c.bind({.name = "r_viewmodel_fov",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "The held weapon's own lens, in degrees vertical.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 150,
          .values = "0: the view's (the default); 1..150: that many degrees"},
         &out.fov_deg);
  const std::string_view names[3] = {"cl_gunx", "cl_guny", "cl_gunz"};
  const std::string_view helps[3] = {"How far the held weapon is moved right, in metres.",
                                     "How far the held weapon is moved up, in metres.",
                                     "How far the held weapon is moved forward, in metres."};
  const std::string_view values[3] = {"-0.5..0.5: that far, negative to the left (0 the default)",
                                      "-0.5..0.5: that far, negative down (0 the default)",
                                      "-0.5..0.5: that far, negative back (0 the default)"};
  for (int k = 0; k < 3; ++k)
    c.bind({.name = names[k],
            .fallback = "0",
            .flags = con::kArchive,
            .help = helps[k],
            .min = -0.5,
            .max = 0.5,
            .values = values[k]},
           &out.gun_m[k]);
  c.bind({.name = "cl_gunsway",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "How much the held weapon sways, swings with your stride and breathes, a share "
                  "of the game's.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 2,
          .values = "0: still; 0..2: that share (1 the default)"},
         &out.sway);
  c.bind({.name = "cl_gunidle",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "How much the held weapon breathes at rest, a share of the game's.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 3,
          .values = "0: still; 0..3: that share (1 the default)"},
         &out.idle);
  c.bind({.name = "cl_gunidle_rate",
          .fallback = "15",
          .flags = con::kArchive,
          .help = "The held weapon's breaths a minute at rest, which a run quickens.",
          .kind = con::CvarKind::number,
          .min = 4,
          .max = 40,
          .values = "4..40: that many (15 the default)"},
         &out.idle_rate);
  c.bind({.name = "cl_gunidle_exertion",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "How much a run quickens and deepens the held weapon's breathing after it, a "
                  "share of the game's.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 3,
          .values = "0: not at all; 0..3: that share (1 the default)"},
         &out.idle_exertion);
  c.bind(
      {.name = "r_viewmodel_center",
       .fallback = "1",
       .flags = con::kArchive,
       .help =
           "Whether the weapon is held in the middle under the crosshair, the classic arena way.",
       .kind = con::CvarKind::boolean,
       .values = "0: to the right; 1: in the middle (the default)"},
      &out.centre);
  c.bind({.name = "r_viewmodel_hands",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether the hands and arms holding the weapon are drawn.",
          .kind = con::CvarKind::boolean,
          .values = "0: the weapon alone; 1: hands and arms (the default)"},
         &out.hands);
  c.bind({.name = "cl_gunswitch",
          .fallback = "0.2",
          .flags = con::kArchive,
          .help = "The seconds a weapon change takes, the old one down and the new one up.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 2,
          .values = "0: at once; 0..2: that many seconds (0.2 the default)"},
         &out.switch_s);
  c.bind({.name = "cl_gunswitch_fire",
          .fallback = "4",
          .flags = con::kArchive,
          .help = "How many times faster a shot during a weapon change raises the new weapon.",
          .kind = con::CvarKind::number,
          .min = 1,
          .max = 20,
          .values = "1: no faster; 1..20: that many times (4 the default)"},
         &out.switch_fire);
}

namespace {

// The breath's cvars on a row: its depth, tip and wander by the sway's share and the idle's, its
// rate the player's, the run's mark on it by the exertion's share.
void breathe(vm::PlaceRow& r, const vm::PlaceRow& base, const ViewSettings& s) {
  const double k = s.sway * s.idle;
  r.breath_m = base.breath_m * k;
  r.breath_pitch = base.breath_pitch * k;
  r.breath_wander_m = base.breath_wander_m * k;
  r.breath_hz = s.idle_rate / 60.0;
  r.exert_rate = base.exert_rate * s.idle_exertion;
  r.exert_depth = base.exert_depth * s.idle_exertion;
}

}  // namespace

vm::PlaceRow held_row(const WeaponLook& look, const ViewSettings& s, const vm::PlaceRow& base) {
  vm::PlaceRow r = base;
  for (int k = 0; k < 3; ++k) r.offset_m[k] = look.offset_m[k] + s.gun_m[k];
  if (s.centre && !look.upright) {  // a melee weapon swings from the right either way
    r.offset_m[0] = s.gun_m[0];
    r.offset_m[1] += kCentreRiseM;
    r.offset_m[2] -= kCentreNearM;
  }
  r.fov_y = s.fov_deg * ge::core::kPi / 180;
  r.sway_max = base.sway_max * s.sway;
  r.sway_shift = base.sway_shift * s.sway;
  r.bob_side = base.bob_side * s.sway;
  breathe(r, base, s);
  return r;
}

vm::PlaceRow eye_row(const ViewSettings& s) {
  vm::PlaceRow r = vm::classic_row();
  const vm::PlaceRow base = r;
  for (int k = 0; k < 3; ++k) r.offset_m[k] = s.gun_m[k];
  r.lower_m = kEyeLowerM;
  r.lower_pitch = kEyeLowerPitch;
  r.fov_y = s.fov_deg * ge::core::kPi / 180;
  r.sway_max = base.sway_max * s.sway;
  r.sway_shift = base.sway_shift * s.sway;
  r.bob_side = base.bob_side * s.sway;
  breathe(r, base, s);
  return r;
}

namespace {

// The `fire` take a shot plays: the one whose frames hold `fire_frame` (the game's word), else one
// at random with even odds; 0 for a model with one take.
int pick_take(Hand& hand, const vm::HeldModel& m, int fire_frame) {
  const int n = vm::count_held_clips(m, "fire");
  if (n < 2) return 0;
  for (int k = 0; k < n && fire_frame > 0; ++k) {
    const vm::HeldClip& c = m.clips[static_cast<size_t>(vm::find_held_clip(m, "fire", k))];
    if (fire_frame >= c.first && fire_frame < c.first + c.count) return k;
  }
  const int k = static_cast<int>(ge::core::next(hand.takes) * n);
  return k < n ? k : n - 1;
}

}  // namespace

HandFrame step_hand(Hand& hand, const HandInput& in, const ViewSettings& s,
                    std::span<const vm::HeldModel> models, const ge::camera::Pose& view, double dt,
                    double motion) {
  if (hand.fresh) {
    hand.held = hand.wanted = in.weapon;
    hand.fresh = false;
  }
  hand.wanted = in.weapon;
  const Weapon before = hand.held;
  if (in.fired && !in.dead && (hand.held != hand.wanted || hand.place.lowered > 0)) {
    // The simulation fires the new weapon at once: the old one is gone, and the new one is rushed
    // up from low in the picture (or from where it is, already on its way up), firing as it comes.
    if (hand.held != hand.wanted && s.switch_s > 0)
      hand.place.lowered = ge::core::num::max(hand.place.lowered, kRushFrom);
    hand.held = hand.wanted;
    hand.rushed = true;
  }
  // Down at the bottom of a switch, or a switch at once: the new weapon goes into the hand.
  if (hand.held != hand.wanted && (hand.place.lowered >= 1.0 || s.switch_s <= 0))
    hand.held = hand.wanted;
  if (hand.held != before) {  // a new weapon comes up at rest
    hand.fire_t = -1;
    hand.shot_frame = -1;
    hand.barrel = -1;
    hand.settle_t = -1;
  }

  const WeaponLook& look =
      in.sequel ? sequel_weapon_look(static_cast<int>(hand.held)) : weapon_look(hand.held);
  vm::PlaceRow row = hand.at_eye ? eye_row(s) : held_row(look, s);
  const bool down = hand.held != hand.wanted || in.dead;
  if (in.dead)
    row.switch_s = kDeathLowerS;  // the death's slower sink
  else if (down)
    row.switch_s = s.switch_s * kSwitchDownShare;
  else
    row.switch_s = s.switch_s * (1 - kSwitchDownShare) /
                   (hand.rushed ? ge::core::num::max(s.switch_fire, 1.0) : 1.0);
  if (row.switch_s <= 0) hand.place.lowered = down ? 1.0 : 0.0;  // at once: no frame between
  vm::PlaceInput pin = in.place;
  pin.lowered = down ? 1.0 : 0.0;
  pin.shot = in.fired ? look.kick : 0.0;
  HandFrame f;
  f.weapon = hand.held;
  f.placement = vm::frame_viewmodel(row, pin, hand.place, dt, motion);
  if (hand.place.lowered <= 0 || down) hand.rushed = false;
  f.switching = hand.held != hand.wanted || hand.place.lowered > 0;
  f.rushed = hand.rushed;
  f.lowered = hand.place.lowered;
  f.drawn = s.draw && hand.place.lowered < 1.0;
  f.hands = s.hands;
  f.fired = in.fired;

  // The fire clip from its start on a shot, held at its end once played; or, for a clip stepped a
  // frame a shot, the next frame round on each shot, held until the next one, the shot leaving
  // from the barrel that frame fires (`barrel_of_frame`), and back at rest a frame's time after
  // the last shot. The barrel is picked before the flash is placed, so both stand on it.
  if (static_cast<size_t>(hand.held) < models.size()) {
    const vm::HeldModel& m = models[static_cast<size_t>(hand.held)];
    const int clip = vm::find_held_clip(m, "fire");
    const vm::HeldClip* c = clip >= 0 ? &m.clips[static_cast<size_t>(clip)] : nullptr;
    if (c != nullptr && c->frame_a_shot && c->count > 0) {
      if (in.fired) {
        hand.settle_t = -1;
        hand.shot_frame = (hand.shot_frame + 1) % c->count;
        hand.fire_t = 0;
        hand.barrel = vm::barrel_of_frame(m, c->first + hand.shot_frame);
      }
      if (hand.fire_t >= 0 && hand.shot_frame >= 0) {
        f.frame_a = f.frame_b =
            c->first + hand.shot_frame;  // held: the shot's barrel is this frame's
        hand.fire_t += dt;
        if (hand.fire_t * c->fps > kShotFrameHold) {
          hand.settle_from = c->first + hand.shot_frame;
          hand.settle_t = s.lerp ? 0 : -1;
          hand.fire_t = -1;
          hand.shot_frame = -1;
        }
      }
    } else {
      hand.barrel = -1;
      if (in.fired && c != nullptr) {
        hand.fire_t = 0;
        hand.take = pick_take(hand, m, in.fire_frame);
      }
      const int played = c != nullptr ? vm::find_held_clip(m, "fire", hand.take) : -1;
      const vm::HeldClip* p = played >= 0 ? &m.clips[static_cast<size_t>(played)] : c;
      if (hand.fire_t >= 0 && p != nullptr) {
        vm::held_clip_frames(m, played >= 0 ? played : clip, hand.fire_t, false, f.frame_a,
                             f.frame_b, f.frame_t);
        hand.fire_t += dt;
        if (hand.fire_t * p->fps > p->count - 1) {
          hand.settle_from = p->first + p->count - 1;
          hand.settle_t = s.lerp ? 0 : -1;
          hand.fire_t = -1;
        }
      }
    }
    // Back to rest over a frame's time from the frame the clip ended on.
    if (hand.fire_t < 0 && hand.settle_t >= 0) {
      const double fps = c != nullptr ? c->fps : 10.0;
      if (hand.settle_t * fps < 1 && hand.settle_from != 0) {
        f.frame_a = hand.settle_from;
        f.frame_b = 0;
        f.frame_t = hand.settle_t * fps;
        hand.settle_t += dt;
      } else {
        hand.settle_t = -1;
      }
    }
  }

  if (in.fired && look.flash_s > 0) hand.flash_left = look.flash_s;
  if (hand.flash_left > 0 && look.flash_s > 0 && static_cast<size_t>(hand.held) < models.size()) {
    const vm::HeldModel& m = models[static_cast<size_t>(hand.held)];
    float tip[3];
    vm::muzzle_at(m, 0, 0, 0, tip, hand.barrel);
    vm::held_to_world(f.placement, row.fov_y, view, tip, f.flash.position);
    for (int k = 0; k < 3; ++k) f.flash.colour[k] = look.flash_rgb[k];
    f.flash.intensity = static_cast<float>(
        look.flash_intensity * ge::core::clamp(hand.flash_left / look.flash_s, 0.0, 1.0));
    f.flash.range_m = look.flash_range_m;
    f.flash.radius_m = 0.2f;
    f.flashing = true;
  }
  hand.flash_left = hand.flash_left > dt ? hand.flash_left - dt : 0;

  if (static_cast<size_t>(hand.held) < models.size()) {
    vm::muzzle_to_world(models[static_cast<size_t>(hand.held)], f.frame_a, f.frame_b, f.frame_t,
                        f.placement, row.fov_y, view, f.muzzle_at, f.muzzle_dir, f.muzzle_up,
                        hand.barrel);
    f.arc = vm::arc_to_world(models[static_cast<size_t>(hand.held)], f.frame_a, f.frame_b,
                             f.frame_t, f.placement, row.fov_y, view, f.arc_from, f.arc_to);
  }
  return f;
}

}  // namespace shooters_edge::view

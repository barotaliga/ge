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
  c.bind({"r_drawviewmodel", "1", con::kArchive, "draw the held weapon (0 off)",
          con::CvarKind::boolean},
         &out.draw);
  c.bind({"r_viewmodel_fov", "0", con::kArchive,
          "the held weapon's own lens, deg vertical; 0: the view's", con::CvarKind::number, 0, 150},
         &out.fov_deg);
  const std::string_view names[3] = {"cl_gunx", "cl_guny", "cl_gunz"};
  const std::string_view helps[3] = {"the held weapon moved right, m",
                                     "the held weapon moved up, m",
                                     "the held weapon moved forward, m"};
  for (int k = 0; k < 3; ++k)
    c.bind({names[k], "0", con::kArchive, helps[k], con::CvarKind::number, -0.5, 0.5},
           &out.gun_m[k]);
  c.bind({"cl_gunsway", "1", con::kArchive,
          "the held weapon's sway, stride swing and breath, share of the game's (0 still)",
          con::CvarKind::number, 0, 2},
         &out.sway);
  c.bind({"cl_gunidle", "1", con::kArchive,
          "the held weapon's breathing at rest, share of the game's (0 none)",
          con::CvarKind::number, 0, 3},
         &out.idle);
  c.bind({"cl_gunidle_rate", "15", con::kArchive,
          "the held weapon's breaths a minute at rest (a run quickens them)", con::CvarKind::number,
          4, 40},
         &out.idle_rate);
  c.bind({"cl_gunidle_exertion", "1", con::kArchive,
          "how much a run quickens and deepens the held weapon's breathing after it, share of "
          "the game's (0 none)",
          con::CvarKind::number, 0, 3},
         &out.idle_exertion);
  c.bind({"r_viewmodel_center", "1", con::kArchive,
          "hold the weapon in the middle under the crosshair, the classic arena way (0: to the "
          "right)",
          con::CvarKind::boolean},
         &out.centre);
  c.bind(
      {"r_viewmodel_hands", "1", con::kArchive,
       "draw the hands and arms holding the weapon (0: the weapon alone)", con::CvarKind::boolean},
      &out.hands);
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
  if (in.fired && hand.held != hand.wanted) {
    // The simulation fires the new weapon at once: it is in the hand, up.
    hand.held = hand.wanted;
    hand.place.lowered = 0;
  }
  // Down at the bottom of a switch: the new weapon goes into the hand and comes up.
  if (hand.held != hand.wanted && hand.place.lowered >= 1.0) hand.held = hand.wanted;
  if (hand.held != before) {  // a new weapon comes up at rest
    hand.fire_t = -1;
    hand.shot_frame = -1;
    hand.barrel = -1;
    hand.settle_t = -1;
  }

  const WeaponLook& look = weapon_look(hand.held);
  vm::PlaceRow row = hand.at_eye ? eye_row(s) : held_row(look, s);
  if (in.dead) row.switch_s = kDeathLowerS;  // the death's slower sink
  vm::PlaceInput pin = in.place;
  pin.lowered = hand.held != hand.wanted || in.dead ? 1.0 : 0.0;
  pin.shot = in.fired ? look.kick : 0.0;
  HandFrame f;
  f.weapon = hand.held;
  f.placement = vm::frame_viewmodel(row, pin, hand.place, dt, motion);
  f.switching = hand.held != hand.wanted || hand.place.lowered > 0;
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
  }
  return f;
}

}  // namespace shooters_edge::view

// ge:summary Shooter's Edge's first-person hand: the switch, the kick, the flash, the cvars
// ge:keywords shooters-edge viewmodel step_hand held_row add_view_cvars switch flash
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#include "shooters_edge/view/hand.hpp"

#include <string_view>

#include "core/math.hpp"

namespace shooters_edge::view {

namespace con = ge::console;

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
  r.idle_m = base.idle_m * s.sway;
  return r;
}

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
  if (hand.held != before) hand.fire_t = -1;  // a new weapon comes up at rest

  const WeaponLook& look = weapon_look(hand.held);
  const vm::PlaceRow row = held_row(look, s);
  vm::PlaceInput pin = in.place;
  pin.lowered = hand.held != hand.wanted ? 1.0 : 0.0;
  pin.shot = in.fired ? look.kick : 0.0;
  HandFrame f;
  f.weapon = hand.held;
  f.placement = vm::frame_viewmodel(row, pin, hand.place, dt, motion);
  f.switching = hand.held != hand.wanted || hand.place.lowered > 0;
  f.drawn = s.draw && hand.place.lowered < 1.0;
  f.hands = s.hands;
  f.fired = in.fired;

  if (in.fired && look.flash_s > 0) hand.flash_left = look.flash_s;
  if (hand.flash_left > 0 && look.flash_s > 0 && static_cast<size_t>(hand.held) < models.size()) {
    const vm::HeldModel& m = models[static_cast<size_t>(hand.held)];
    float tip[3];
    vm::muzzle_at(m, 0, 0, 0, tip);
    vm::held_to_world(f.placement, row.fov_y, view, tip, f.flash.position);
    for (int k = 0; k < 3; ++k) f.flash.colour[k] = look.flash_rgb[k];
    f.flash.intensity = static_cast<float>(
        look.flash_intensity * ge::core::clamp(hand.flash_left / look.flash_s, 0.0, 1.0));
    f.flash.range_m = look.flash_range_m;
    f.flash.radius_m = 0.2f;
    f.flashing = true;
  }
  hand.flash_left = hand.flash_left > dt ? hand.flash_left - dt : 0;

  // The fire clip from its start on a shot, held at its end once played.
  if (static_cast<size_t>(hand.held) < models.size()) {
    const vm::HeldModel& m = models[static_cast<size_t>(hand.held)];
    const int clip = vm::find_held_clip(m, "fire");
    if (in.fired && clip >= 0) hand.fire_t = 0;
    if (hand.fire_t >= 0 && clip >= 0) {
      vm::held_clip_frames(m, clip, hand.fire_t, false, f.frame_a, f.frame_b, f.frame_t);
      const vm::HeldClip& c = m.clips[static_cast<size_t>(clip)];
      hand.fire_t += dt;
      if (hand.fire_t * c.fps > c.count - 1) hand.fire_t = -1;
    }
  }
  if (static_cast<size_t>(hand.held) < models.size()) {
    vm::muzzle_to_world(models[static_cast<size_t>(hand.held)], f.frame_a, f.frame_b, f.frame_t,
                        f.placement, row.fov_y, view, f.muzzle_at, f.muzzle_dir, f.muzzle_up);
  }
  return f;
}

}  // namespace shooters_edge::view

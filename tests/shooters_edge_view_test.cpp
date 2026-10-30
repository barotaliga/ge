// Shooter's Edge's held weapons (games/shooters_edge/view): the set is the eight number keys in
// order, every free model loads and mounts muzzle forward at its stated length, the hand puts a
// weapon away and raises the next on the game's change (and snaps it up on a shot), a shot lights
// the muzzle and fades, and the player's cvars move the row.
//
// ge:summary Tests for Shooter's Edge's held weapons: the set, the mounts, the switch, the flash
// ge:keywords test shooters-edge viewmodel weapons mount switch flash cvars hand
// ge:skill render testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/view/hand.hpp"
#include "shooters_edge/view/weapons.hpp"

namespace {

namespace se = shooters_edge::view;
namespace vm = ge::viewmodel;

const std::vector<vm::HeldModel>& models() {
  static const std::vector<vm::HeldModel> m = [] {
    std::vector<vm::HeldModel> out;
    std::string error;
    se::load_weapon_models(std::string(GE_SOURCE_DIR) + "/games/shooters_edge/content/models/view",
                           out, &error);
    return out;
  }();
  return m;
}

ge::camera::Pose eye() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.fov_y = 1.2;
  return v;
}

}  // namespace

TEST(the_set_is_the_number_keys_in_order) {
  CHECK(se::weapon_looks().size() == static_cast<size_t>(se::kWeapons));
  for (int i = 0; i < se::kWeapons; ++i) {
    se::Weapon w;
    CHECK(se::weapon_of_impulse(i + 1, w) && static_cast<int>(w) == i);
    CHECK(se::weapon_look(w).weapon == w);
  }
  se::Weapon w;
  CHECK(!se::weapon_of_impulse(0, w) && !se::weapon_of_impulse(9, w));
  // Only the melee weapon throws no light.
  for (const se::WeaponLook& l : se::weapon_looks())
    CHECK((l.flash_s > 0) == (l.weapon != se::Weapon::melee));
}

TEST(every_free_weapon_loads_mounted_muzzle_forward) {
  const std::vector<vm::HeldModel>& m = models();
  CHECK(m.size() == static_cast<size_t>(se::kWeapons));
  for (size_t i = 0; i < m.size(); ++i) {
    const se::WeaponLook& look = se::weapon_looks()[i];
    float lo[3], hi[3];
    vm::held_bounds(m[i], lo, hi);
    const float along = look.upright ? hi[1] - lo[1] : hi[2] - lo[2];
    CHECK(std::fabs(along - look.length_m) < 1e-3);
    if (look.upright) continue;
    // The grip at the origin: the muzzle ahead of it, the stock behind.
    float tip[3];
    vm::muzzle_at(m[i], 0, 0, 0, tip);
    CHECK(tip[2] > 0.5f * hi[2] && lo[2] < 0);
  }
}

TEST(a_change_puts_the_weapon_away_then_raises_the_next) {
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  const se::ViewSettings s;
  const double dt = 1.0 / 60;
  se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), dt);
  CHECK(f.weapon == se::Weapon::shotgun && !f.switching && f.drawn);
  in.weapon = se::Weapon::rocket_launcher;
  bool saw_down = false, saw_old_lowering = false;
  int up_at = -1;
  for (int i = 1; i <= 60 && up_at < 0; ++i) {
    f = se::step_hand(hand, in, s, models(), eye(), dt);
    if (f.weapon == se::Weapon::shotgun && f.switching) saw_old_lowering = true;
    if (!f.drawn) saw_down = true;
    if (f.weapon == se::Weapon::rocket_launcher && !f.switching) up_at = i;
  }
  CHECK(saw_old_lowering && saw_down);
  CHECK(up_at > 0 && up_at * dt <= 2 * se::weapon_row().switch_s + 0.1);
}

TEST(a_shot_during_a_switch_snaps_the_new_weapon_up) {
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::nailgun;
  const se::ViewSettings s;
  se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  in.weapon = se::Weapon::beam_gun;
  se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  in.fired = true;
  const se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  CHECK(f.weapon == se::Weapon::beam_gun && f.drawn && f.flashing);
}

TEST(a_shot_lights_the_muzzle_and_fades) {
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  const se::ViewSettings s;
  const double dt = 1.0 / 60;
  se::step_hand(hand, in, s, models(), eye(), dt);
  in.fired = true;
  se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), dt);
  const se::WeaponLook& look = se::weapon_look(se::Weapon::shotgun);
  CHECK(f.flashing && f.flash.intensity > 0.9f * look.flash_intensity);
  // In front of the eye and to the right, where the barrel is.
  CHECK(f.flash.position[2] > 0.3 && f.flash.position[0] > 0);
  in.fired = false;
  float last = f.flash.intensity;
  int frames = 1;
  while (f.flashing && frames < 60) {
    f = se::step_hand(hand, in, s, models(), eye(), dt);
    if (f.flashing) CHECK(f.flash.intensity < last);
    last = f.flash.intensity;
    ++frames;
  }
  CHECK(!f.flashing && frames * dt <= look.flash_s + 2 * dt);
}

TEST(the_cvars_move_the_row) {
  ge::console::Cvars cvars;
  se::ViewSettings s;
  se::add_view_cvars(cvars, s);
  const se::WeaponLook& look = se::weapon_look(se::Weapon::shotgun);
  const vm::PlaceRow game = se::held_row(look, s);
  CHECK(game.offset_m[0] == look.offset_m[0] && game.fov_y == 0);
  CHECK(cvars.set("cl_gunx", "-0.1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("r_viewmodel_fov", "60", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("cl_gunsway", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("r_drawviewmodel", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  const vm::PlaceRow moved = se::held_row(look, s);
  CHECK(std::fabs(moved.offset_m[0] - (look.offset_m[0] - 0.1)) < 1e-12);
  CHECK(std::fabs(moved.fov_y - 60 * 3.14159265358979 / 180) < 1e-9);
  CHECK(moved.sway_max == 0 && moved.bob_side == 0 && moved.idle_m == 0);
  CHECK(!s.draw);
  // Not drawn: still switching and flashing, unseen.
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  in.fired = true;
  const se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  CHECK(!f.drawn && f.flashing);
}

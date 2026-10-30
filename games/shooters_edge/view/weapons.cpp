// ge:summary Shooter's Edge's held weapons: the set's table, the mounts, the loader, the row
// ge:keywords shooters-edge viewmodel weapon_looks weapon_mount load_weapon_models weapon_row
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#include "shooters_edge/view/weapons.hpp"

#include "assets/gltf.hpp"

namespace shooters_edge::view {

namespace {

// clang-format off
constexpr WeaponLook kLooks[kWeapons] = {
    // weapon                   name                length upright  offset (right, up, fwd)   kick  refire  flash rgb             int    range  s
    {Weapon::melee,            "melee",            0.34, true,  {0.20, -0.36, 0.34},  0.4, 0.5,  {0, 0, 0},             0.0f,  0.0f, 0.0},
    {Weapon::shotgun,          "shotgun",          0.46, false, {0.14, -0.17, 0.32},  1.0, 0.5,  {1.0f, 0.78f, 0.42f},  0.35f, 6.0f, 0.1},
    {Weapon::double_shotgun,   "double_shotgun",   0.46, false, {0.14, -0.18, 0.36},  1.6, 0.7,  {1.0f, 0.78f, 0.42f},  0.55f, 7.0f, 0.12},
    {Weapon::nailgun,          "nailgun",          0.44, false, {0.14, -0.17, 0.30},  0.25, 0.1, {1.0f, 0.66f, 0.32f},  0.2f,  4.0f, 0.06},
    {Weapon::heavy_nailgun,    "heavy_nailgun",    0.52, false, {0.14, -0.17, 0.32},  0.35, 0.1, {1.0f, 0.66f, 0.32f},  0.25f, 5.0f, 0.06},
    {Weapon::grenade_launcher, "grenade_launcher", 0.50, false, {0.15, -0.20, 0.44},  1.2, 0.6,  {1.0f, 0.78f, 0.42f},  0.3f,  5.0f, 0.1},
    {Weapon::rocket_launcher,  "rocket_launcher",  0.62, false, {0.15, -0.19, 0.38},  1.5, 0.8,  {1.0f, 0.72f, 0.38f},  0.45f, 7.0f, 0.14},
    {Weapon::beam_gun,         "beam_gun",         0.52, false, {0.15, -0.18, 0.34},  0.15, 0.1, {0.62f, 0.76f, 1.0f},  0.3f,  6.0f, 0.1},
};
// clang-format on

}  // namespace

std::span<const WeaponLook> weapon_looks() { return kLooks; }

const WeaponLook& weapon_look(Weapon w) { return kLooks[static_cast<int>(w)]; }

bool weapon_of_impulse(int impulse, Weapon& out) {
  if (impulse < 1 || impulse > kWeapons) return false;
  out = static_cast<Weapon>(impulse - 1);
  return true;
}

vm::Mount weapon_mount(const WeaponLook& look, const float lo[3], const float hi[3]) {
  vm::Mount m;
  if (look.upright) {
    // Held by the bottom of its handle, the head up: the model's +y stays up and its +z forward;
    // scaled to its height.
    const double h = hi[1] - lo[1];
    m.scale = h > 0 ? look.length_m / h : 1.0;
    m.grip[0] = 0.5;
    m.grip[1] = 0.0;
    m.grip[2] = 0.5;
    return m;
  }
  m.muzzle_negative = true;  // the kit's barrels point along -z once in the engine's frame
  m.length_m = look.length_m;
  m.grip[0] = 0.5;
  m.grip[1] = 0.3;
  m.grip[2] = 0.3;
  return m;
}

bool load_weapon_models(const std::string& dir, std::vector<vm::HeldModel>& out,
                        std::string* error) {
  out.clear();
  for (const WeaponLook& look : kLooks) {
    const std::string path = dir + "/" + std::string(look.name) + ".glb";
    ge::assets::LoadResult r = ge::assets::load_gltf_file(path);
    if (!r.ok) {
      *error = path + ": " + r.error +
               "; next: run from the repository's root or pass the folder that holds the view "
               "models (games/shooters_edge/content/models/view)";
      return false;
    }
    vm::HeldModel m = vm::held_from_model(r.model);
    float lo[3], hi[3];
    vm::held_bounds(m, lo, hi);
    vm::fit_mount(m, weapon_mount(look, lo, hi));
    out.push_back(std::move(m));
  }
  return true;
}

vm::PlaceRow weapon_row() {
  vm::PlaceRow r = vm::modern_row();
  r.switch_s = 0.12;
  return r;
}

}  // namespace shooters_edge::view

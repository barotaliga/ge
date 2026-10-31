// ge:summary Shooter's Edge's held weapons: the set's table, the mounts, the loader, the row
// ge:keywords shooters-edge viewmodel weapon_looks weapon_mount load_weapon_models weapon_row
// ge:keywords load_weapon_model weapon_model_path keep-textures
// ge:skill render shooter
// ge:test tests/shooters_edge_view_test.cpp
#include "shooters_edge/view/weapons.hpp"

#include <filesystem>
#include <system_error>

#include "assets/gltf.hpp"

namespace shooters_edge::view {

namespace {

// clang-format off
constexpr WeaponLook kLooks[kWeapons] = {
    // name, held length m, offset from the eye (right, up, forward m), kick, refire s, flash s,
    // flash rgb, flash intensity, flash range m, the weapon, held upright
    {"axe",               0.38, {0.11, -0.10, 0.20}, 0.4, 0.5, 0.0, {0, 0, 0}, 0.0f, 0.0f, Weapon::melee, true},
    {"shotgun",           0.46, {0.09, -0.09, 0.17}, 1.0, 0.5, 0.1, {1.0f, 0.78f, 0.42f}, 0.35f, 6.0f, Weapon::shotgun, false},
    {"double_shotgun",    0.46, {0.09, -0.09, 0.17}, 1.6, 0.7, 0.12, {1.0f, 0.78f, 0.42f}, 0.55f, 7.0f, Weapon::double_shotgun, false},
    {"minigun",           0.44, {0.10, -0.10, 0.16}, 0.25, 0.1, 0.06, {1.0f, 0.66f, 0.32f}, 0.2f, 4.0f, Weapon::nailgun, false},
    {"railgun",           0.52, {0.09, -0.09, 0.17}, 0.35, 0.1, 0.06, {1.0f, 0.66f, 0.32f}, 0.25f, 5.0f, Weapon::heavy_nailgun, false},
    {"grenade_launcher",  0.50, {0.09, -0.10, 0.17}, 1.2, 0.6, 0.1, {1.0f, 0.78f, 0.42f}, 0.3f, 5.0f, Weapon::grenade_launcher, false},
    {"rocket_launcher",   0.62, {0.10, -0.11, 0.16}, 1.5, 0.8, 0.14, {1.0f, 0.72f, 0.38f}, 0.45f, 7.0f, Weapon::rocket_launcher, false},
    {"beam_gun",          0.52, {0.15, -0.18, 0.34}, 0.15, 0.1, 0.1, {0.62f, 0.76f, 1.0f}, 0.3f, 6.0f, Weapon::beam_gun, false},
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
  m.bounds_frame = 0;  // the rest pose: a swing or a pump does not move the mount
  if (look.upright) {
    // Held by the bottom of its handle, the head up, its edge along -z (the guns' frame, turned
    // round): scaled to its height.
    m.muzzle_negative = true;
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

std::string weapon_model_path(const std::string& dir, const WeaponLook& look) {
  const std::string name(look.name);
  std::string built = dir + "/" + name + "/" + name + ".glb";
  std::error_code ec;
  if (std::filesystem::is_regular_file(built, ec)) return built;
  return dir + "/" + name + ".glb";
}

bool load_weapon_model(const std::string& path, const WeaponLook& look, vm::HeldModel& out,
                       std::string* error, vm::HeldModel* weapon_only) {
  ge::assets::LoadOptions keep;
  keep.keep_textures = true;
  ge::assets::LoadResult r = ge::assets::load_gltf_file(path, keep);
  if (!r.ok) {
    *error = path + ": " + r.error;
    return false;
  }
  out = vm::held_with_clips(r.model, kClipFps);
  // The weapon alone, for the mount's box: the hands and arms holding it (nodes named
  // `<name>.hand...`) reach back toward the eye and are not its length.
  ge::assets::Model bare = r.model;
  std::erase_if(bare.meshes, [](const ge::assets::Mesh& m) {
    return m.name.find(".hand") != std::string::npos;
  });
  const bool hands = bare.meshes.size() != r.model.meshes.size() && !bare.meshes.empty();
  vm::HeldModel weapon = vm::held_from_model(hands ? bare : r.model);
  float lo[3], hi[3];
  vm::held_bounds(weapon, lo, hi, 0);
  vm::Mount mount = weapon_mount(look, lo, hi);
  if (hands) {
    // Built round its hands, at its true size: the same scale for every weapon, held at its origin.
    mount.muzzle_negative = true;
    mount.length_m = 0;
    mount.scale = kBuiltScale;
    mount.grip_at_origin = true;
  }
  vm::fit_mount(out, mount, &weapon);
  if (weapon_only) {
    vm::HeldModel copy = weapon;
    vm::fit_mount(weapon, mount, &copy);
    *weapon_only = std::move(weapon);
  }
  return true;
}

bool load_weapon_models(const std::string& dir, std::vector<vm::HeldModel>& out,
                        std::string* error) {
  out.clear();
  for (const WeaponLook& look : kLooks) {
    vm::HeldModel m;
    if (!load_weapon_model(weapon_model_path(dir, look), look, m, error)) {
      *error +=
          "; next: run from the repository's root or pass the folder that holds the view "
          "models (games/shooters_edge/content/models/view)";
      return false;
    }
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

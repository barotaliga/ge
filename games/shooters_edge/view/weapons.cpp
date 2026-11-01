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

// The fires, at the true size (a real gun's: a few milliseconds of glowing gas, small and dim).
constexpr std::string_view kShotgunFire =
    "life=0.02,length=0.26,width=0.15,glow=0.02,flash=0.22,sparks=3,light_range=4,"
    "smoke.smoke=0.5";
constexpr std::string_view kDoubleFire =
    "life=0.024,length=0.3,width=0.2,glow=0.022,flash=0.28,prongs=6,sparks=4,light_range=5,"
    "smoke.smoke=0.65";
constexpr std::string_view kMinigunFire =
    "life=0.016,length=0.14,width=0.08,glow=0.02,flash=0.16,sparks=1,light_range=3,"
    "smoke.smoke=0.18,smoke_speed=1.5";
constexpr std::string_view kRailgunFire =
    "length=0.03,width=0.03,glow=0,flash=0,sparks=0,light_range=0,smoke.smoke=0.16,"
    "smoke_speed=1.5";
constexpr std::string_view kGrenadeFire =
    "life=0.022,length=0.14,width=0.11,glow=0.018,flash=0.13,sparks=0,light_range=2.5,"
    "smoke.smoke=0.55";
constexpr std::string_view kRocketFire =
    "life=0.036,length=0.54,width=0.12,glow=0.03,flash=0.22,sparks=3,light_range=5.5,"
    "smoke.smoke=0.65";

// clang-format off
constexpr WeaponLook kLooks[kWeapons] = {
    // name, held length m, offset from the eye (right, up, forward m), kick, refire s, flash s,
    // flash rgb, flash intensity, flash range m, the weapon, held upright, the fire and its knobs
    {"axe",               0.38, {0.11, -0.12, 0.18}, 0.4, 0.5, 0.0, {0, 0, 0}, 0.0f, 0.0f, Weapon::melee, true, "", ""},
    {"shotgun",           0.46, {0.09, -0.12, 0.09}, 1.0, 0.5, 0.1, {1.0f, 0.78f, 0.42f}, 0.35f, 6.0f, Weapon::shotgun, false, "muzzle.shotgun", kShotgunFire},
    {"double_shotgun",    0.46, {0.09, -0.12, 0.11}, 1.6, 0.7, 0.12, {1.0f, 0.78f, 0.42f}, 0.55f, 7.0f, Weapon::double_shotgun, false, "muzzle.shotgun", kDoubleFire},
    {"minigun",           0.44, {0.10, -0.13, 0.08}, 0.25, 0.1, 0.06, {1.0f, 0.66f, 0.32f}, 0.2f, 4.0f, Weapon::nailgun, false, "muzzle.nailgun", kMinigunFire},
    {"railgun",           0.52, {0.09, -0.12, 0.09}, 0.35, 0.1, 0.06, {1.0f, 0.66f, 0.32f}, 0.25f, 5.0f, Weapon::heavy_nailgun, false, "muzzle.nailgun", kRailgunFire},
    {"grenade_launcher",  0.50, {0.09, -0.13, 0.09}, 1.2, 0.6, 0.1, {1.0f, 0.78f, 0.42f}, 0.3f, 5.0f, Weapon::grenade_launcher, false, "muzzle.grenade", kGrenadeFire},
    {"rocket_launcher",   0.62, {0.10, -0.14, 0.08}, 1.5, 0.8, 0.14, {1.0f, 0.72f, 0.38f}, 0.45f, 7.0f, Weapon::rocket_launcher, false, "muzzle.rocket", kRocketFire},
    {"lightning_gun",     0.52, {0.10, -0.14, 0.08}, 0.15, 0.1, 0.1, {0.62f, 0.76f, 1.0f}, 0.3f, 6.0f, Weapon::beam_gun, false, "", ""},
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
  if (hands) vm::mark_hands(out, r.model, ".hand");
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

// The map picture's item icons (item_icons.hpp).
//
// ge:summary The map overview's item icons: the game's own models rastered on the CPU, the table
// ge:keywords item-icons map-overview model-icon box-icon respawn
// ge:skill map-design ui
#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/app/overview_floors.hpp"
#include "morph/model.hpp"
#include "morph/overview_mesh.hpp"
#include "morph/read.hpp"
#include "shooters_edge/livemap/icons.hpp"
#include "ui/image.hpp"

namespace shooters_edge::livemap {

namespace {

namespace br = ge::brush;
namespace ba = ge::brush::app;
namespace cl = shooters_edge::client;

// The files as the figure art reads them.
ge::classic::ReadFile reader(const cl::SearchPath& files) {
  return [&files](std::string_view path, std::vector<uint8_t>& store) {
    return cl::find_file(files, path, store);
  };
}

}  // namespace

// A model file (the first family's .mdl or the second's .md2) as a mesh, its skin `skin`.
bool model_mesh(const cl::SearchPath& files, std::span<const uint8_t> palette,
                const std::string& path, int skin, Mesh* out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = cl::find_file(files, path, store);
  if (bytes.empty()) return false;
  ge::morph::Model m;
  std::string err;
  if (!ge::morph::read_model(bytes, path, &m, &err) || m.frames.empty()) return false;
  ge::morph::SkinPaint paint;
  paint.palette = palette;
  std::vector<uint8_t> picture;
  if ((m.skins.empty() || palette.size() < 768) && !m.skin_paths.empty())
    picture = ge::classic::read_skin_picture(
        reader(files), palette,
        m.skin_paths[size_t(std::clamp(skin, 0, int(m.skin_paths.size()) - 1))], &paint.pw,
        &paint.ph);
  paint.picture = picture;
  *out = ge::morph::overview_mesh(m, 0, skin, paint);
  return true;
}

bool read_model_art(const cl::SearchPath& files, std::span<const uint8_t> palette,
                    const std::string& path, ModelArt* out, const std::string& skin_path) {
  return ge::classic::read_model_art(reader(files), palette, path, out, skin_path);
}

// A small map (the first family's health and ammo boxes) as a mesh.
bool box_mesh(const cl::SearchPath& files, std::span<const uint8_t> palette,
              const std::string& path, Mesh* out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = cl::find_file(files, path, store);
  if (bytes.empty() || palette.size() < 768) return false;
  ba::Pack pack;
  pack.bytes.assign(bytes.begin(), bytes.end());
  pack.files.push_back({path, 0, uint32_t(bytes.size())});
  ba::Scene scene;
  std::string err;
  if (!ba::load_scene(pack, path, palette, &scene, &err) || scene.textures.levels.empty())
    return false;
  *out = ba::overview_box(scene);
  return true;
}

// ---- what a map's things are --------------------------------------------------------------

// The first family's things, then the second's.
constexpr ItemKind kItems[] = {
    {"item_artifact_super_damage", -1, "progs/quaddama.mdl", 0, false, Tier::power, "QUAD",
     0x4f8cff, 60},
    {"item_artifact_invulnerability", -1, "progs/invulner.mdl", 0, false, Tier::power, "PENT",
     0xff4b4b, 300},
    {"item_artifact_invisibility", -1, "progs/invisibl.mdl", 0, false, Tier::power, "RING",
     0xffd34b, 300},
    {"item_artifact_envirosuit", -1, "progs/suit.mdl", 0, false, Tier::major, "SUIT", 0x8fdc5a, 60},
    {"item_armorInv", -1, "progs/armor.mdl", 2, false, Tier::power, "RA", 0xff4b4b, 20},
    {"item_armor2", -1, "progs/armor.mdl", 1, false, Tier::major, "YA", 0xffd34b, 20},
    {"item_armor1", -1, "progs/armor.mdl", 0, false, Tier::major, "GA", 0x6ad46a, 20},
    {"item_health", 2, "maps/b_bh100.bsp", 0, true, Tier::power, "MH", 0x4fd0ff, 20},
    {"item_health", 1, "maps/b_bh10.bsp", 0, true, Tier::minor, "+15", 0x9ad0a0, 0},
    {"item_health", -1, "maps/b_bh25.bsp", 0, true, Tier::minor, "+25", 0x9ad0a0, 0},
    {"weapon_rocketlauncher", -1, "progs/g_rock2.mdl", 0, false, Tier::power, "RL", 0xff8a3c, 30},
    {"weapon_lightning", -1, "progs/g_light.mdl", 0, false, Tier::power, "LG", 0x8fd8ff, 30},
    {"weapon_grenadelauncher", -1, "progs/g_rock.mdl", 0, false, Tier::major, "GL", 0xff8a3c, 30},
    {"weapon_supernailgun", -1, "progs/g_nail2.mdl", 0, false, Tier::major, "SNG", 0xd0d0d0, 30},
    {"weapon_nailgun", -1, "progs/g_nail.mdl", 0, false, Tier::minor, "NG", 0xd0d0d0, 0},
    {"weapon_supershotgun", -1, "progs/g_shot.mdl", 0, false, Tier::minor, "SSG", 0xd0d0d0, 0},
    {"item_rockets", 1, "maps/b_rock1.bsp", 0, true, Tier::minor, "R", 0xb08a5a, 0},
    {"item_rockets", -1, "maps/b_rock0.bsp", 0, true, Tier::minor, "R", 0xb08a5a, 0},
    {"item_shells", 1, "maps/b_shell1.bsp", 0, true, Tier::minor, "S", 0xb08a5a, 0},
    {"item_shells", -1, "maps/b_shell0.bsp", 0, true, Tier::minor, "S", 0xb08a5a, 0},
    {"item_spikes", 1, "maps/b_nail1.bsp", 0, true, Tier::minor, "N", 0xb08a5a, 0},
    {"item_spikes", -1, "maps/b_nail0.bsp", 0, true, Tier::minor, "N", 0xb08a5a, 0},
    {"item_cells", 1, "maps/b_batt1.bsp", 0, true, Tier::minor, "C", 0xb08a5a, 0},
    {"item_cells", -1, "maps/b_batt0.bsp", 0, true, Tier::minor, "C", 0xb08a5a, 0},
    {"item_quad", -1, "models/items/quaddama/tris.md2", 0, false, Tier::power, "QUAD", 0x4f8cff,
     60},
    {"item_invulnerability", -1, "models/items/invulner/tris.md2", 0, false, Tier::power, "INV",
     0xff4b4b, 300},
    {"item_armor_body", -1, "models/items/armor/body/tris.md2", 0, false, Tier::power, "BA",
     0xff4b4b, 20},
    {"item_armor_combat", -1, "models/items/armor/combat/tris.md2", 0, false, Tier::major, "CA",
     0xffd34b, 20},
    {"item_armor_jacket", -1, "models/items/armor/jacket/tris.md2", 0, false, Tier::major, "JA",
     0x6ad46a, 20},
    {"item_health_mega", -1, "models/items/mega_h/tris.md2", 0, false, Tier::power, "MH", 0x4fd0ff,
     20},
    {"item_health_large", -1, "models/items/healing/large/tris.md2", 0, false, Tier::minor, "+25",
     0x9ad0a0, 0},
    {"item_health", -1, "models/items/healing/medium/tris.md2", 0, false, Tier::minor, "+10",
     0x9ad0a0, 0},
    {"weapon_railgun", -1, "models/weapons/g_rail/tris.md2", 0, false, Tier::power, "RG", 0x8fd8ff,
     30},
    {"weapon_hyperblaster", -1, "models/weapons/g_hyperb/tris.md2", 0, false, Tier::major, "HB",
     0xd0d0d0, 30},
    {"weapon_chaingun", -1, "models/weapons/g_chain/tris.md2", 0, false, Tier::major, "CG",
     0xd0d0d0, 30},
    {"weapon_machinegun", -1, "models/weapons/g_machn/tris.md2", 0, false, Tier::minor, "MG",
     0xd0d0d0, 0},
    {"weapon_shotgun", -1, "models/weapons/g_shotg/tris.md2", 0, false, Tier::minor, "SG", 0xd0d0d0,
     0},
    {"weapon_bfg", -1, "models/weapons/g_bfg/tris.md2", 0, false, Tier::power, "BFG", 0x63e68a, 30},
};

const ItemKind* item_of(const br::Entity& e) {
  const std::string& cls = e.get("classname");
  const int flags = std::atoi(e.get("spawnflags").c_str());
  if ((flags & 2048) != 0) return nullptr;  // not in deathmatch
  for (const ItemKind& k : kItems)
    if (cls == k.cls && (k.flags < 0 || (flags & k.flags) != 0)) return &k;
  return nullptr;
}

int icon_size(Tier t) { return t == Tier::power ? 76 : t == Tier::major ? 58 : 38; }

}  // namespace shooters_edge::livemap

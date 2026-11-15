// THE ITEM ICONS OF A MAP PICTURE (the `map-overview` lab): a thing on a map (armour, a power, a
// weapon, a health or ammo box) as a picture of the game's own model from its pack, drawn on the
// CPU from three quarters above with its skin, lit from the upper left, cut out with a dark rim, so
// a picture of a map can show it larger than in the game. The first family's models (.mdl, and its
// health and ammo boxes, which are small maps) and the second's (.md2 with their pictures).
//
// ge:summary The map picture's item icons: the pack's models drawn on the CPU, cut out
// ge:keywords item-icons model-icons icon cut-out three-quarter armour power weapon health ammo
// ge:keywords map-overview mdl md2 box-model
// ge:skill map-design labs
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "shooters_edge/client/files.hpp"
#include "ui/image.hpp"

namespace shooters_edge::labs {

// Triangles with their texture: positions in map units (z up), texels into `tex` within each
// vertex's rectangle.
struct Mesh {
  std::vector<float> pos;   // xyz a vertex
  std::vector<float> st;    // texels
  std::vector<float> rect;  // x y w h a vertex
  std::vector<uint32_t> idx;
  std::vector<uint8_t> tex;  // RGBA
  int tw = 0, th = 0;
};

// `m` from three quarters above (`azimuth` round z from +x toward +y, `elevation` up), lit from
// the upper left, cut out on a clear ground with a dark rim, `size` px square.
ge::ui::Image draw_icon(const Mesh& m, int size, double azimuth_deg, double elevation_deg);
// A model file (.mdl or .md2) as a mesh, its skin `skin`; false when the files have none.
bool model_mesh(const shooters_edge::client::SearchPath& files, std::span<const uint8_t> palette,
                const std::string& path, int skin, Mesh* out);
// A small map (the first family's health and ammo boxes) as a mesh.
bool box_mesh(const shooters_edge::client::SearchPath& files, std::span<const uint8_t> palette,
              const std::string& path, Mesh* out);

enum class Tier : uint8_t { power, major, minor };

struct ItemKind {
  const char* cls;
  int flags;  // spawnflags it needs (-1 any)
  const char* model;
  int skin;
  bool box;  // a small map, not a model
  Tier tier;
  const char* tag;  // the badge's letters, and the label under a weapon
  uint32_t colour;  // its glow and badge
  int respawn;      // seconds, 0 none shown
};

// What the entity is (nullptr: not an item drawn, or one kept out of deathmatch).
const ItemKind* item_of(const ge::brush::Entity& e);
// The icon's size on the picture, px.
int icon_size(Tier t);

}  // namespace shooters_edge::labs

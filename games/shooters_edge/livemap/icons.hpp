// THE ITEM ICONS OF A MAP PICTURE (the `map-overview` lab): a thing on a map (armour, a power, a
// weapon, a health or ammo box) as a picture of the game's own model from its pack, drawn on the
// CPU from three quarters above with its skin, lit from the upper left, cut out with a dark rim, so
// a picture of a map can show it larger than in the game. The first family's models (.mdl, and its
// health and ammo boxes, which are small maps) and the second's (.md2 with their pictures).
//
// THE LIVE MAP's FIGURES (live_map.hpp) are drawn the same way, at a fixed size rather than fitted:
// a model read once (`ModelArt`), one frame of it (the entity's frame number) in a player's shirt
// and pants colours (`art_mesh`), turned to the entity's yaw and seen from the south from three
// quarters above (`draw_figure`), its origin at the picture's middle, so a figure animates and
// turns in place without its size jumping from frame to frame. The drawing is the engine's
// (engine/overview/mesh_picture.hpp), a model's frame as a mesh the morph plugin's
// (plugins/morph/overview_mesh.hpp), a box's the brush plugin's; this file holds only what the
// game's things and colours mean.
//
// ge:summary The map picture's item icons: the pack's models drawn on the CPU, cut out
// ge:keywords item-icons model-icons icon cut-out three-quarter armour power weapon health ammo
// ge:keywords map-overview mdl md2 box-model live-map figure player-colours translation
// ge:keywords draw_figure model_art art_mesh
// ge:skill map-design labs
// ge:test tests/shooters_edge_live_map_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "classic/figure_art.hpp"
#include "morph/model.hpp"
#include "overview/mesh_picture.hpp"
#include "shooters_edge/client/files.hpp"
#include "ui/image.hpp"

namespace shooters_edge::livemap {

// The overview's textured mesh and its drawing (engine/overview/mesh_picture.hpp).
using Mesh = ge::overview::Mesh;
using ge::overview::draw_figure;
using ge::overview::draw_icon;

// A model file (.mdl or .md2) as a mesh, its skin `skin`; false when the files have none.
bool model_mesh(const shooters_edge::client::SearchPath& files, std::span<const uint8_t> palette,
                const std::string& path, int skin, Mesh* out);
// A small map (the first family's health and ammo boxes) as a mesh.
bool box_mesh(const shooters_edge::client::SearchPath& files, std::span<const uint8_t> palette,
              const std::string& path, Mesh* out);

// A model read once, one frame of it in a player's colours (plugins/classic/figure_art.hpp).
using ge::classic::art_mesh;
using ge::classic::frame_index;
using ge::classic::ModelArt;
using ge::classic::translate;
// `path` read into `out` from the game's files (ge::classic::read_model_art over them).
bool read_model_art(const shooters_edge::client::SearchPath& files,
                    std::span<const uint8_t> palette, const std::string& path, ModelArt* out,
                    const std::string& skin_path = "");
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
  int respawn;      // seconds back after taken (a weapon only when weapons do not stay), 0 none
};

// What the entity is (nullptr: not an item drawn, or one kept out of deathmatch).
const ItemKind* item_of(const ge::brush::Entity& e);
// The icon's size on the picture, px.
int icon_size(Tier t);

}  // namespace shooters_edge::livemap
